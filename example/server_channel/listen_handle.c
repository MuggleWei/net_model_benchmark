#include "listen_handle.h"
#include "haclog/haclog.h"
#include "netmodel/net_model_common.h"

void listen_handle_message(muggle_event_loop_t *evloop,
						   muggle_socket_context_t *ctx, msg_hdr_t *hdr,
						   void *payload)
{
	MUGGLE_UNUSED(evloop);

	switch (hdr->msg_type) {
	case MSG_TYPE_REQ_LOGIN: {
		msg_req_login_t *req = (msg_req_login_t *)payload;

		// NOTE: assume login success
		char buf[64];
		muggle_event_fd fd = muggle_socket_ctx_get_fd(ctx);
		muggle_socket_remote_addr(fd, buf, sizeof(buf), 0);
		LOG_INFO("login success, user_id=%u, remote_addr=%s", req->user_id,
				 buf);

		session_t *session = (session_t *)muggle_socket_ctx_get_data(ctx);
		session->user_id = req->user_id;
		session->is_detached = 1;

		muggle_socket_ctx_ref_retain(ctx);
		muggle_socket_ctx_set_flag(ctx, MUGGLE_EV_CTX_FLAG_CLOSED);

		LOG_INFO("detach socket context: remote_addr=%s", buf);
	} break;
	default: {
		LOG_ERROR("unhandle message type: %u", hdr->msg_type);
	} break;
	}
}

void on_listen_add_ctx(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx)
{
	MUGGLE_UNUSED(evloop);

	if (ctx == NULL) {
		return;
	}

	muggle_event_fd fd = muggle_socket_ctx_get_fd(ctx);
	switch (muggle_socket_ctx_type(ctx)) {
	case MUGGLE_SOCKET_CTX_TYPE_TCP_LISTEN: {
		char buf[64];
		muggle_socket_local_addr(fd, buf, sizeof(buf), 0);
		LOG_INFO("on evloop add listen ctx: %s", buf);
	} break;
	default: {
		char buf[64];
		muggle_socket_remote_addr(fd, buf, sizeof(buf), 0);
		LOG_INFO("on evloop add remote ctx: %s", buf);
	} break;
	}
}
void on_listen_connect(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx)
{
	MUGGLE_UNUSED(evloop);

	muggle_event_fd fd = muggle_socket_ctx_get_fd(ctx);
	char buf[64];
	muggle_socket_remote_addr(fd, buf, sizeof(buf), 0);
	LOG_INFO("on connection: %s", buf);

	int enable = 1;
	muggle_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&enable,
					  sizeof(enable));

	session_t *session = (session_t *)malloc(sizeof(session_t));
	memset(session, 0, sizeof(*session));
	session->ctx = ctx;
	session->is_detached = 0;
	session->user_id = 0;
	session->is_client = 1;
	muggle_socket_remote_addr(fd, session->remote_addr,
							  sizeof(session->remote_addr), 0);
	muggle_bytes_buffer_init(&session->bytes_buf, 1024 * 1024 * 4);
	muggle_socket_ctx_set_data(ctx, session);
}
void on_listen_message(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx)
{
	session_t *session = (session_t *)muggle_socket_ctx_get_data(ctx);
	netmodel_decode(evloop, ctx, &session->bytes_buf, listen_handle_message);
}
void on_listen_close(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx)
{
	session_t *session = (session_t *)muggle_socket_ctx_get_data(ctx);
	if (session && session->is_detached) {
		LOG_INFO("on detach: %s", session->remote_addr);

		evloop_listen_data_t *data =
			(evloop_listen_data_t *)muggle_evloop_get_data(evloop);
		uint32_t idx_worker = session->user_id % data->num_worker;

		session->is_detached = 0;
		ctx->base.flags = 0;

		muggle_event_loop_t *worker = data->workers[idx_worker];
		muggle_socket_evloop_add_ctx(worker, ctx);
	} else {
		LOG_INFO("on close: %s", session->remote_addr);
	}
}
void on_listen_release(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx)
{
	MUGGLE_UNUSED(evloop);

	session_t *session = (session_t *)muggle_socket_ctx_get_data(ctx);
	if (session) {
		LOG_INFO("on release: %s", session->remote_addr);
		muggle_bytes_buffer_destroy(&session->bytes_buf);
		free(session);
	}
}
