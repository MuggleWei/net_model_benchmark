#include "upstream_handle.h"
#include "haclog/haclog.h"
#include "netmodel/net_model_common.h"

static void upstream_handle_message(muggle_event_loop_t *evloop,
									muggle_socket_context_t *ctx,
									msg_hdr_t *hdr, void *payload)
{
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);

	switch (hdr->msg_type) {
	case MSG_TYPE_RSP_ORDER: {
		order_t *order = (order_t *)payload;

		order->server_rcv_rsp_ts.sec = ts.tv_sec;
		order->server_rcv_rsp_ts.nsec = ts.tv_nsec;

		LOG_DEBUG("server recv rsp order: "
				  "uid=%u, coid=%llu, ts=%llu.%lu, rsp_cnt=%llu",
				  order->user_id, (unsigned long long)order->coid,
				  (unsigned long long)order->server_rcv_rsp_ts.sec,
				  (unsigned long)order->server_rcv_rsp_ts.nsec,
				  (unsigned long long)order->rsp_cnt);

		server_context_t *server_ctx =
			(server_context_t *)muggle_evloop_get_data(evloop);
		server_context_push(server_ctx->client_chan, ctx, hdr->msg_type,
							sizeof(msg_hdr_t) + sizeof(order_t), (void *)hdr);
	} break;
	}
}

static muggle_thread_ret_t connect_upstream_routine(void *p_args)
{
	muggle_event_loop_t *evloop = (muggle_event_loop_t*)p_args;
	server_context_t *server_ctx = muggle_evloop_get_data(evloop);

	muggle_socket_context_t *ctx = connect_upstream(server_ctx->cfg);
	muggle_socket_evloop_add_ctx(evloop, ctx);

	return 0;
}

muggle_socket_context_t *connect_upstream(server_config_t *cfg)
{
	muggle_socket_t fd = MUGGLE_INVALID_SOCKET;
	do {
		LOG_INFO("try connect upstream: %s %s", cfg->upstream_host,
				 cfg->upstream_port);
		fd = muggle_tcp_connect(cfg->upstream_host, cfg->upstream_port, 3);
		if (fd == MUGGLE_INVALID_SOCKET) {
			// LOG_SYS_ERR(LOG_LEVEL_ERROR, "failed connect");
			LOG_ERROR("failed connect upstream: %s %s", cfg->upstream_host,
					  cfg->upstream_port);
			muggle_msleep(3000);
			continue;
		}

		int enable = 1;
		muggle_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&enable,
						  sizeof(enable));

		muggle_socket_set_nonblock(fd, 1);

		break;
	} while (1);

	LOG_INFO("success connect upstream");

	muggle_socket_context_t *ctx =
		(muggle_socket_context_t *)malloc(sizeof(muggle_socket_context_t));
	muggle_socket_ctx_init(ctx, fd, NULL, MUGGLE_SOCKET_CTX_TYPE_TCP_CLIENT);

	session_t *session = (session_t *)malloc(sizeof(session_t));
	memset(session, 0, sizeof(*session));
	session->ctx = ctx;
	session->is_detached = 0;
	session->user_id = 0;
	session->is_client = 0;
	muggle_socket_remote_addr(fd, session->remote_addr,
							  sizeof(session->remote_addr), 0);
	muggle_bytes_buffer_init(&session->bytes_buf, 1024 * 1024 * 4);
	muggle_socket_ctx_set_data(ctx, session);

	return ctx;
}

void on_upstream_add_ctx(muggle_event_loop_t *evloop,
						 muggle_socket_context_t *ctx)
{
	server_context_t *server_ctx =
		(server_context_t *)muggle_evloop_get_data(evloop);

	muggle_socket_ctx_ref_retain(ctx);
	server_context_push(server_ctx->upstream_chan, ctx, CHAN_MSG_TYPE_ADD_CTX,
						0, NULL);
}
void on_upstream_message(muggle_event_loop_t *evloop,
						 muggle_socket_context_t *ctx)
{
	session_t *session = (session_t *)muggle_socket_ctx_get_data(ctx);
	netmodel_decode(evloop, ctx, &session->bytes_buf, upstream_handle_message);
}
void on_upstream_close(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx)
{
	server_context_t *server_ctx =
		(server_context_t *)muggle_evloop_get_data(evloop);
	session_t *session = (session_t *)muggle_socket_ctx_get_data(ctx);
	LOG_INFO("on close upstream: %s", session->remote_addr);

	server_context_push(server_ctx->upstream_chan, ctx, CHAN_MSG_TYPE_DEL_CTX,
						0, NULL);

	muggle_thread_t th;
	muggle_thread_create(&th, connect_upstream_routine, evloop);
	muggle_thread_detach(&th);
}
void on_upstream_release(muggle_event_loop_t *evloop,
						 muggle_socket_context_t *ctx)
{
	server_context_t *server_ctx =
		(server_context_t *)muggle_evloop_get_data(evloop);
	if (muggle_socket_ctx_ref_release(ctx) == 0) {
		server_context_del_ctx(server_ctx, ctx);
	}
}
