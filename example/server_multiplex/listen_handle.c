#include "listen_handle.h"
#include "netmodel/net_model_common.h"
#include "worker_handle.h"

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

		worker_user_data_t *user_data =
			(worker_user_data_t *)muggle_socket_ctx_get_data(ctx);
		user_data->user_id = req->user_id;
		user_data->is_detached = 1;

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

	worker_user_data_t *user_data =
		(worker_user_data_t *)malloc(sizeof(worker_user_data_t));
	memset(user_data, 0, sizeof(*user_data));
	user_data->is_detached = 0;
	user_data->user_id = 0;
	user_data->is_client = 1;
	muggle_bytes_buffer_init(&user_data->bytes_buf, 1024 * 1024 * 4);
	muggle_socket_ctx_set_data(ctx, user_data);
}
void on_listen_message(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx)
{
	worker_user_data_t *user_data =
		(worker_user_data_t *)muggle_socket_ctx_get_data(ctx);
	netmodel_decode(evloop, ctx, &user_data->bytes_buf, listen_handle_message);
}
void on_listen_close(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx)
{
	muggle_event_fd fd = muggle_socket_ctx_get_fd(ctx);
	char buf[64];
	muggle_socket_remote_addr(fd, buf, sizeof(buf), 0);

	worker_user_data_t *user_data =
		(worker_user_data_t *)muggle_socket_ctx_get_data(ctx);
	if (user_data && user_data->is_detached) {
		LOG_INFO("on detach: %s", buf);

		evloop_listen_data_t *data =
			(evloop_listen_data_t *)muggle_evloop_get_data(evloop);
		uint32_t idx_worker = user_data->user_id % data->num_worker;

		user_data->is_detached = 0;
		ctx->base.flags = 0;

		muggle_event_loop_t *worker = data->workers[idx_worker];
		muggle_socket_evloop_add_ctx(worker, ctx);
	} else {
		LOG_INFO("on close: %s", buf);
	}
}
void on_listen_release(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx)
{
	MUGGLE_UNUSED(evloop);

	muggle_event_fd fd = muggle_socket_ctx_get_fd(ctx);
	char buf[64];
	muggle_socket_remote_addr(fd, buf, sizeof(buf), 0);
	LOG_INFO("on release: %s", buf);

	worker_user_data_t *user_data =
		(worker_user_data_t *)muggle_socket_ctx_get_data(ctx);
	if (user_data) {
		muggle_bytes_buffer_destroy(&user_data->bytes_buf);
		free(user_data);
	}
}
