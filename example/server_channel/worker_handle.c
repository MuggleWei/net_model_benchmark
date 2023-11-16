#include "worker_handle.h"
#include "haclog/haclog.h"
#include "netmodel/net_model_common.h"

static void client_handle_message(muggle_event_loop_t *evloop,
								  muggle_socket_context_t *ctx, msg_hdr_t *hdr,
								  void *payload)
{
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);

	switch (hdr->msg_type) {
	case MSG_TYPE_REQ_ORDER: {
		order_t *order = (order_t *)payload;

		order->server_rcv_ts.sec = ts.tv_sec;
		order->server_rcv_ts.nsec = ts.tv_nsec;

		LOG_DEBUG("server recv order: "
				  "uid=%u, coid=%llu, ts=%llu.%lu",
				  order->user_id, (unsigned long long)order->coid,
				  (unsigned long long)order->server_rcv_ts.sec,
				  (unsigned long)order->server_rcv_ts.nsec);

		worker_evloop_data_t *data =
			(worker_evloop_data_t *)muggle_evloop_get_data(evloop);
		server_context_t *server_ctx = data->server_ctx;

		server_context_push(server_ctx->upstream_chan, ctx, hdr->msg_type,
							sizeof(msg_hdr_t) + hdr->payload_len, (void *)hdr);
	} break;
	}
}

void on_worker_add_ctx(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx)
{
	session_t *session = (session_t *)muggle_socket_ctx_get_data(ctx);
	worker_evloop_data_t *data =
		(worker_evloop_data_t *)muggle_evloop_get_data(evloop);
	server_context_t *server_ctx = data->server_ctx;

	LOG_INFO("worker add client ctx: %s", session->remote_addr);

	if (session->user_id >=
		sizeof(server_ctx->user_ctxs) / sizeof(server_ctx->user_ctxs[0])) {
		muggle_socket_ctx_set_flag(ctx, MUGGLE_EV_CTX_FLAG_CLOSED);
		return;
	}

	muggle_socket_ctx_ref_retain(ctx);
	server_context_push(server_ctx->client_chan, ctx, CHAN_MSG_TYPE_ADD_CTX, 0,
						NULL);
}
void on_worker_message(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx)
{
	session_t *session = (session_t *)muggle_socket_ctx_get_data(ctx);

	netmodel_decode(evloop, ctx, &session->bytes_buf, client_handle_message);
}
void on_worker_close(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx)
{
	worker_evloop_data_t *data =
		(worker_evloop_data_t *)muggle_evloop_get_data(evloop);
	server_context_t *server_ctx = data->server_ctx;

	session_t *session = (session_t *)muggle_socket_ctx_get_data(ctx);
	LOG_INFO("on close client: %s", session->remote_addr);

	server_context_push(server_ctx->client_chan, ctx, CHAN_MSG_TYPE_DEL_CTX, 0,
						NULL);
}
void on_worker_release(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx)
{
	worker_evloop_data_t *data =
		(worker_evloop_data_t *)muggle_evloop_get_data(evloop);
	server_context_t *server_ctx = data->server_ctx;

	if (muggle_socket_ctx_ref_release(ctx) == 0) {
		server_context_del_ctx(server_ctx, ctx);
	}
}
