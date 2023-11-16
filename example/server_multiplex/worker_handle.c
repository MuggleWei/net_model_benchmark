#include "worker_handle.h"
#include "netmodel/net_model_common.h"

static muggle_thread_ret_t connect_upstream_routine(void *p_args)
{
	worker_evloop_data_t *data = (worker_evloop_data_t *)p_args;

	muggle_socket_context_t *ctx = connect_upstream(data);
	muggle_socket_evloop_add_ctx(data->evloop, ctx);

	return 0;
}

muggle_socket_context_t *connect_upstream(worker_evloop_data_t *data)
{
	muggle_socket_t fd = MUGGLE_INVALID_SOCKET;
	do {
		LOG_INFO("try connect upstream: %s %s", data->upstream_host,
				 data->upstream_port);
		fd = muggle_tcp_connect(data->upstream_host, data->upstream_port, 3);
		if (fd == MUGGLE_INVALID_SOCKET) {
			// LOG_SYS_ERR(LOG_LEVEL_ERROR, "failed connect");
			LOG_ERROR("failed connect upstream: %s %s", data->upstream_host,
					  data->upstream_port);
			muggle_msleep(3000);
			continue;
		}

		int enable = 1;
		muggle_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&enable,
						  sizeof(enable));

		break;
	} while (1);

	LOG_INFO("success connect upstream");

	muggle_socket_context_t *ctx =
		(muggle_socket_context_t *)malloc(sizeof(muggle_socket_context_t));
	muggle_socket_ctx_init(ctx, fd, NULL, MUGGLE_SOCKET_CTX_TYPE_TCP_CLIENT);

	worker_user_data_t *user_data =
		(worker_user_data_t *)malloc(sizeof(worker_user_data_t));
	memset(user_data, 0, sizeof(*user_data));
	user_data->is_detached = 0;
	user_data->user_id = 0;
	user_data->is_client = 0;
	muggle_bytes_buffer_init(&user_data->bytes_buf, 1024 * 1024 * 4);
	muggle_socket_ctx_set_data(ctx, user_data);

	return ctx;
}

void worker_handle_message(muggle_event_loop_t *evloop,
						   muggle_socket_context_t *ctx, msg_hdr_t *hdr,
						   void *payload)
{
	MUGGLE_UNUSED(evloop);

	struct timespec ts;
	timespec_get(&ts, TIME_UTC);

	worker_evloop_data_t *data =
		(worker_evloop_data_t *)muggle_evloop_get_data(evloop);
	worker_user_data_t *user_data =
		(worker_user_data_t *)muggle_socket_ctx_get_data(ctx);

	switch (hdr->msg_type) {
	case MSG_TYPE_REQ_ORDER: {
		order_t *order = (order_t *)payload;

		order->server_rcv_ts.sec = ts.tv_sec;
		order->server_rcv_ts.nsec = ts.tv_nsec;

		if (order->user_id != user_data->user_id) {
			LOG_ERROR("user id not equal!!!");
			return;
		}

		LOG_DEBUG("server recv order: "
				  "uid=%u, coid=%llu, ts=%llu.%lu",
				  order->user_id, (unsigned long long)order->coid,
				  (unsigned long long)order->server_rcv_ts.sec,
				  (unsigned long)order->server_rcv_ts.nsec);

		if (data->upstream_ctx) {
			struct timespec ts;
			timespec_get(&ts, TIME_UTC);
			order->server_snd_ts.sec = ts.tv_sec;
			order->server_snd_ts.nsec = ts.tv_nsec;

			muggle_socket_ctx_write(data->upstream_ctx, hdr,
									sizeof(msg_hdr_t) + sizeof(order_t));

			LOG_DEBUG("server send order: "
					  "uid=%u, coid=%llu, ts=%llu.%lu",
					  order->user_id, (unsigned long long)order->coid,
					  (unsigned long long)order->server_snd_ts.sec,
					  (unsigned long)order->server_snd_ts.nsec);
		}
	} break;
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

		worker_user_linked_node_t *head = &data->user_ctxs[order->user_id];
		worker_user_linked_node_t *node = head->next;
		while (node) {
			timespec_get(&ts, TIME_UTC);
			order->server_snd_rsp_ts.sec = ts.tv_sec;
			order->server_snd_rsp_ts.nsec = ts.tv_nsec;

			muggle_socket_ctx_write(node->ctx, hdr,
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
		LOG_ERROR("unhandle message type: %u", hdr->msg_type);
	} break;
	}
}

void on_worker_add_ctx(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx)
{
	if (ctx == NULL) {
		return;
	}

	muggle_event_fd fd = muggle_socket_ctx_get_fd(ctx);
	char remote_addr[64];
	muggle_socket_remote_addr(fd, remote_addr, sizeof(remote_addr), 0);

	worker_evloop_data_t *data =
		(worker_evloop_data_t *)muggle_evloop_get_data(evloop);
	worker_user_data_t *user_data =
		(worker_user_data_t *)muggle_socket_ctx_get_data(ctx);
	strncpy(user_data->remote_addr, remote_addr,
			sizeof(user_data->remote_addr) - 1);
	user_data->remote_addr[sizeof(user_data->remote_addr) - 1] = '\0';

	switch (muggle_socket_ctx_type(ctx)) {
	case MUGGLE_SOCKET_CTX_TYPE_TCP_CLIENT: {
		if (user_data->is_client) {
			LOG_INFO("worker add client ctx: %s", remote_addr);

			if (user_data->user_id >=
				sizeof(data->user_ctxs) / sizeof(data->user_ctxs[0])) {
				muggle_socket_ctx_set_flag(ctx, MUGGLE_EV_CTX_FLAG_CLOSED);
				return;
			}

			worker_user_linked_node_t *head =
				&data->user_ctxs[user_data->user_id];
			worker_user_linked_node_t *node =
				(worker_user_linked_node_t *)malloc(
					sizeof(worker_user_linked_node_t));
			node->ctx = ctx;
			node->next = head->next;
			head->next = node;

			NET_MODEL_NEW_STACK_MSG(MSG_TYPE_RSP_LOGIN, order_t, rsp);
			rsp->user_id = user_data->user_id;
			NET_MODEL_SND_MSG(ctx, rsp);
		} else {
			data->upstream_ctx = ctx;
			LOG_INFO("worker add upstream ctx: %s", remote_addr);
		}
	} break;
	default: {
		LOG_ERROR("add unexpected ctx: %s", remote_addr);
	} break;
	}
}
void on_worker_message(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx)
{
	worker_user_data_t *user_data =
		(worker_user_data_t *)muggle_socket_ctx_get_data(ctx);
	netmodel_decode(evloop, ctx, &user_data->bytes_buf, worker_handle_message);
}
void on_worker_close(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx)
{
	worker_evloop_data_t *data =
		(worker_evloop_data_t *)muggle_evloop_get_data(evloop);
	worker_user_data_t *user_data =
		(worker_user_data_t *)muggle_socket_ctx_get_data(ctx);

	if (user_data->is_client) {
		LOG_INFO("on close client: %s", user_data->remote_addr);

		worker_user_linked_node_t *head = &data->user_ctxs[user_data->user_id];
		worker_user_linked_node_t *node = head->next;
		worker_user_linked_node_t *prev = head;
		while (node) {
			if (node->ctx == ctx) {
				prev->next = node->next;
				free(node);
				break;
			} else {
				prev = node;
				node = node->next;
			}
		}
	} else {
		LOG_INFO("on close upstream: %s", user_data->remote_addr);
		data->upstream_ctx = NULL;

		muggle_thread_t th;
		muggle_thread_create(&th, connect_upstream_routine, data);
		muggle_thread_detach(&th);
	}
}
void on_worker_release(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx)
{
	MUGGLE_UNUSED(evloop);

	worker_user_data_t *user_data =
		(worker_user_data_t *)muggle_socket_ctx_get_data(ctx);
	if (user_data) {
		LOG_INFO("on release: %s", user_data->remote_addr);
		muggle_bytes_buffer_destroy(&user_data->bytes_buf);
		free(user_data);
	}
}
