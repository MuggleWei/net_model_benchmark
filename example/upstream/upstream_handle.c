#include "upstream_handle.h"
#include "netmodel/net_model_common.h"

void handle_message(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx,
					msg_hdr_t *hdr, void *payload)
{
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);

	switch (hdr->msg_type) {
	case MSG_TYPE_REQ_ORDER: {
		order_store_t *order = (order_store_t *)malloc(sizeof(order_store_t));
		order->ctx = ctx;
		memcpy(&order->order, payload, sizeof(order->order));
		order->order.upstream_rcv_ts.sec = ts.tv_sec;
		order->order.upstream_rcv_ts.nsec = ts.tv_nsec;
		order->order.rsp_cnt = 0;

		evloop_data_t *data = muggle_evloop_get_data(evloop);
		muggle_linked_list_t *order_list = &data->order_list;
		muggle_linked_list_append(order_list, NULL, order);

		LOG_DEBUG("upstream recv order: "
				  "uid=%u, coid=%llu, ts=%llu.%lu",
				  order->order.user_id, (unsigned long long)order->order.coid,
				  (unsigned long long)order->order.upstream_rcv_ts.sec,
				  (unsigned long)order->order.upstream_rcv_ts.nsec);
	} break;
	default: {
		LOG_ERROR("unhandle message type: %u", hdr->msg_type);
	} break;
	}
}

void on_add_ctx(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx)
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
void on_connect(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx)
{
	MUGGLE_UNUSED(evloop);

	muggle_event_fd fd = muggle_socket_ctx_get_fd(ctx);
	char buf[64];
	muggle_socket_remote_addr(fd, buf, sizeof(buf), 0);
	LOG_INFO("on connection: %s", buf);

	int enable = 1;
	muggle_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&enable,
					  sizeof(enable));

	muggle_bytes_buffer_t *bytes_buf =
		(muggle_bytes_buffer_t *)malloc(sizeof(muggle_bytes_buffer_t));
	muggle_bytes_buffer_init(bytes_buf, 1024 * 1024 * 4);
	muggle_socket_ctx_set_data(ctx, bytes_buf);
}
void on_message(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx)
{
	muggle_bytes_buffer_t *bytes_buf =
		(muggle_bytes_buffer_t *)muggle_socket_ctx_get_data(ctx);
	netmodel_decode(evloop, ctx, bytes_buf, handle_message);
}
void on_timer(muggle_event_loop_t *evloop)
{
	evloop_data_t *data = muggle_evloop_get_data(evloop);

	struct timespec curr_ts;
	timespec_get(&curr_ts, TIME_UTC);

	uint64_t elapsed_ms = (curr_ts.tv_sec - data->last_ts.tv_sec) * 1000 +
						  curr_ts.tv_nsec / 1000000 -
						  data->last_ts.tv_nsec / 1000000;
	if (elapsed_ms < data->timer_interval_ms) {
		return;
	}
	memcpy(&data->last_ts, &curr_ts, sizeof(data->last_ts));

	muggle_linked_list_t *order_list = &data->order_list;
	muggle_linked_list_node_t *node = muggle_linked_list_first(order_list);
	for (; node; node = muggle_linked_list_next(order_list, node)) {
		order_store_t *order_store = (order_store_t *)node->data;
		order_t *order = &order_store->order;
		order->rsp_cnt++;
		if (order->rsp_cnt == MAX_ORDER_RSP) {
			order->order_status = ORDER_STATUS_TOTAL_DEAL;
		} else {
			order->order_status = ORDER_STATUS_PART_DEAL;
		}

		NET_MODEL_NEW_STACK_MSG(MSG_TYPE_RSP_ORDER, order_t, rsp);
		memcpy(rsp, order, sizeof(*rsp));

		struct timespec ts;
		timespec_get(&ts, TIME_UTC);
		rsp->upstream_snd_ts.sec = ts.tv_sec;
		rsp->upstream_snd_ts.nsec = ts.tv_nsec;

		NET_MODEL_SND_MSG(order_store->ctx, rsp);

		LOG_DEBUG("upstream send order: "
				  "uid=%u, coid=%llu, ts=%llu.%lu",
				  order_store->order.user_id,
				  (unsigned long long)order_store->order.coid,
				  (unsigned long long)order_store->order.upstream_snd_ts.sec,
				  (unsigned long)order_store->order.upstream_snd_ts.nsec);

		// NOTE: on timer only return one rsp
		// break;
	}

	node = muggle_linked_list_first(order_list);
	while (node) {
		order_store_t *order_store = (order_store_t *)node->data;
		order_t *order = &order_store->order;
		if (order->rsp_cnt < MAX_ORDER_RSP) {
			break;
		}
		node = muggle_linked_list_remove(order_list, node, free_order, NULL);
	}
}
void on_close(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx)
{
	muggle_event_fd fd = muggle_socket_ctx_get_fd(ctx);
	char buf[64];
	muggle_socket_remote_addr(fd, buf, sizeof(buf), 0);
	LOG_INFO("on close: %s", buf);

	evloop_data_t *data = muggle_evloop_get_data(evloop);
	muggle_linked_list_t *order_list = &data->order_list;
	muggle_linked_list_node_t *node = muggle_linked_list_first(order_list);
	while (node) {
		order_store_t *order_store = (order_store_t *)node->data;
		if (order_store->ctx != ctx) {
			node = muggle_linked_list_next(order_list, node);
		} else {
			node =
				muggle_linked_list_remove(order_list, node, free_order, NULL);
		}
	}
}
void on_release(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx)
{
	MUGGLE_UNUSED(evloop);

	muggle_event_fd fd = muggle_socket_ctx_get_fd(ctx);
	char buf[64];
	muggle_socket_remote_addr(fd, buf, sizeof(buf), 0);
	LOG_INFO("on release: %s", buf);

	muggle_bytes_buffer_t *bytes_buf = muggle_socket_ctx_get_data(ctx);
	if (bytes_buf) {
		muggle_bytes_buffer_destroy(bytes_buf);
		free(bytes_buf);
		muggle_socket_ctx_set_data(ctx, NULL);
	}
}

void free_order(void *pool, void *data)
{
	MUGGLE_UNUSED(pool);
	free(data);
}
