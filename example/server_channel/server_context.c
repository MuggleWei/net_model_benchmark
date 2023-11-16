#include "server_context.h"
#include "haclog/haclog.h"
#include "netmodel/net_model_common.h"

static muggle_thread_ret_t upstream_chan_routine(void *p_args)
{
	server_context_t *server_ctx = (server_context_t *)p_args;
	muggle_channel_t *chan = server_ctx->upstream_chan;
	muggle_socket_context_t *upstream_ctx = NULL;

	while (1) {
		chan_msg_t *msg = (chan_msg_t *)muggle_channel_read(chan);
		switch (msg->msg_type) {
		case CHAN_MSG_TYPE_ADD_CTX: {
			upstream_ctx = msg->ctx;
		} break;
		case CHAN_MSG_TYPE_DEL_CTX: {
			upstream_ctx = NULL;
			if (muggle_socket_ctx_ref_release(msg->ctx) == 0) {
				server_context_del_ctx(server_ctx, msg->ctx);
			}
		} break;
		case MSG_TYPE_REQ_ORDER: {
			if (upstream_ctx == NULL) {
				LOG_ERROR("upstream is not connected");
			}

			msg_hdr_t *hdr = (msg_hdr_t *)(msg + 1);
			order_t *order = (order_t *)(hdr + 1);

			struct timespec ts;
			timespec_get(&ts, TIME_UTC);
			order->server_snd_ts.sec = ts.tv_sec;
			order->server_snd_ts.nsec = ts.tv_nsec;

			muggle_socket_ctx_write(upstream_ctx, hdr, msg->num_bytes);

			LOG_DEBUG("server send order: "
					  "uid=%u, coid=%llu, ts=%llu.%lu",
					  order->user_id, (unsigned long long)order->coid,
					  (unsigned long long)order->server_snd_ts.sec,
					  (unsigned long)order->server_snd_ts.nsec);
		} break;
		default: {
			LOG_ERROR("unhandle message type: %u", msg->msg_type);
		} break;
		}

		free(msg);
	}

	return 0;
}

static muggle_thread_ret_t client_chan_routine(void *p_args)
{
	server_context_t *server_ctx = (server_context_t *)p_args;
	muggle_channel_t *chan = server_ctx->client_chan;
	while (1) {
		chan_msg_t *msg = (chan_msg_t *)muggle_channel_read(chan);
		switch (msg->msg_type) {
		case CHAN_MSG_TYPE_ADD_CTX: {
			muggle_socket_context_t *ctx = msg->ctx;
			session_t *session = muggle_socket_ctx_get_data(ctx);
			user_context_t *user_ctx = &server_ctx->user_ctxs[session->user_id];

			user_context_add(user_ctx, session);

			{
				muggle_spinlock_lock(&user_ctx->lock);

				NET_MODEL_NEW_STACK_MSG(MSG_TYPE_RSP_LOGIN, order_t, rsp);
				rsp->user_id = session->user_id;
				NET_MODEL_SND_MSG(ctx, rsp);

				muggle_spinlock_unlock(&user_ctx->lock);
			}
		} break;
		case CHAN_MSG_TYPE_DEL_CTX: {
			if (muggle_socket_ctx_ref_release(msg->ctx) == 0) {
				server_context_del_ctx(server_ctx, msg->ctx);
			}
		} break;
		case MSG_TYPE_RSP_ORDER: {
			msg_hdr_t *hdr = (msg_hdr_t *)(msg + 1);
			order_t *order = (order_t *)(hdr + 1);

			user_context_t *user_ctx = &server_ctx->user_ctxs[order->user_id];

			struct timespec ts;
			user_session_linked_node_t *node = user_ctx->head.next;
			while (node) {
				timespec_get(&ts, TIME_UTC);
				order->server_snd_rsp_ts.sec = ts.tv_sec;
				order->server_snd_rsp_ts.nsec = ts.tv_nsec;

				muggle_socket_context_t *ctx = node->session->ctx;

				muggle_socket_ctx_write(ctx, hdr,
										sizeof(msg_hdr_t) + sizeof(order_t));
				node = node->next;
				LOG_DEBUG("server send rsp order: "
						  "uid=%u, coid=%llu, ts=%llu.%lu, rsp_cnt=%llu",
						  order->user_id, (unsigned long long)order->coid,
						  (unsigned long long)order->server_snd_rsp_ts.sec,
						  (unsigned long)order->server_snd_rsp_ts.nsec,
						  (unsigned long long)order->rsp_cnt);
			}
		} break;
		default: {
			LOG_ERROR("unhandle message type: %u", msg->msg_type);
		} break;
		}

		free(msg);
	}

	return 0;
}

bool server_context_init(server_context_t *ctx, server_config_t *cfg,
						 muggle_channel_t *upstream_chan,
						 muggle_channel_t *client_chan)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->cfg = cfg;
	ctx->upstream_chan = upstream_chan;
	ctx->client_chan = client_chan;
	for (size_t i = 0; i < sizeof(ctx->user_ctxs) / sizeof(ctx->user_ctxs[0]);
		 i++) {
		if (!user_context_init(&ctx->user_ctxs[i])) {
			LOG_ERROR("failed init user context");
			return false;
		}
	}

	return true;
}

void server_context_run(server_context_t *ctx)
{
	muggle_thread_t th_upstream_chan;
	muggle_thread_create(&th_upstream_chan, upstream_chan_routine, ctx);
	muggle_thread_detach(&th_upstream_chan);

	muggle_thread_t th_client_chan;
	muggle_thread_create(&th_client_chan, client_chan_routine, ctx);
	muggle_thread_detach(&th_client_chan);
}

void server_context_push(muggle_channel_t *chan, muggle_socket_context_t *ctx,
						 uint32_t msg_type, uint32_t num_bytes, void *data)
{
	chan_msg_t *msg = (chan_msg_t *)malloc(sizeof(chan_msg_t) + num_bytes);
	memset(msg, 0, sizeof(*msg));
	msg->msg_type = msg_type;
	msg->num_bytes = num_bytes;
	msg->ctx = ctx;
	if (data) {
		void *dst = (void *)(msg + 1);
		memcpy(dst, data, num_bytes);
	}
	muggle_channel_write(chan, msg);
}

void server_context_del_ctx(server_context_t *server_ctx,
							muggle_socket_context_t *ctx)
{
	session_t *session = (session_t *)muggle_socket_ctx_get_data(ctx);
	if (session) {
		LOG_INFO("on release: %s", session->remote_addr);

		if (session->is_client && session->user_id != 0 &&
			session->user_id >= sizeof(server_ctx->user_ctxs) /
									sizeof(server_ctx->user_ctxs[0])) {
			user_context_t *user_ctx = &server_ctx->user_ctxs[session->user_id];
			user_context_del(user_ctx, session);
		}

		muggle_bytes_buffer_destroy(&session->bytes_buf);
		free(session);
	}
	muggle_socket_ctx_close(ctx);
	free(ctx);
}
