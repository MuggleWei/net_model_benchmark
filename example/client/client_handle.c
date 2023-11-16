#include "client_handle.h"
#include "netmodel/net_model_common.h"

void handle_message(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx,
					msg_hdr_t *hdr, void *payload)
{
	MUGGLE_UNUSED(ctx);

	evloop_data_t *data = (evloop_data_t *)muggle_evloop_get_data(evloop);

	switch (hdr->msg_type) {
	case MSG_TYPE_RSP_LOGIN: {
		data->is_login = 1;
		LOG_INFO("success login");
	} break;
	case MSG_TYPE_RSP_ORDER: {
		struct timespec ts;
		timespec_get(&ts, TIME_UTC);

		order_t *rsp = (order_t *)payload;
		if (rsp->coid >= data->max_idx) {
			LOG_ERROR("invalid coid: %llu", (unsigned long long)rsp->coid);
			return;
		}
		rsp->client_rcv_rsp_ts.sec = ts.tv_sec;
		rsp->client_rcv_rsp_ts.nsec = ts.tv_nsec;

		LOG_DEBUG("server recv rsp order: "
				  "uid=%u, coid=%llu, ts=%llu.%lu, rsp_cnt=%llu",
				  rsp->user_id, (unsigned long long)rsp->coid,
				  (unsigned long long)rsp->server_rcv_rsp_ts.sec,
				  (unsigned long)rsp->server_rcv_rsp_ts.nsec,
				  (unsigned long long)rsp->rsp_cnt);

		if (rsp->rsp_cnt == 1) {
			memcpy(&data->orders[rsp->coid], rsp, sizeof(order_t));
		}
		if (rsp->coid == data->max_idx - 1 && rsp->rsp_cnt == MAX_ORDER_RSP) {
			LOG_INFO("mission completed, wait exit event loop");
			timespec_get(&data->finish_ts, TIME_UTC);
		}
	} break;
	default: {
		LOG_ERROR("unhandle message type: %u", hdr->msg_type);
	} break;
	}
}

void on_add_ctx(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx)
{
	if (ctx == NULL) {
		return;
	}

	muggle_event_fd fd = muggle_socket_ctx_get_fd(ctx);
	char buf[64];
	muggle_socket_remote_addr(fd, buf, sizeof(buf), 0);
	switch (muggle_socket_ctx_type(ctx)) {
	case MUGGLE_SOCKET_CTX_TYPE_TCP_CLIENT: {
		LOG_INFO("on evloop add client ctx: %s", buf);
	} break;
	default: {
		LOG_ERROR("evloop add unknown ctx: %s", buf);
	} break;
	}

	muggle_bytes_buffer_t *bytes_buf =
		(muggle_bytes_buffer_t *)malloc(sizeof(muggle_bytes_buffer_t));
	muggle_bytes_buffer_init(bytes_buf, 1024 * 1024 * 4);
	muggle_socket_ctx_set_data(ctx, bytes_buf);

	evloop_data_t *data = muggle_evloop_get_data(evloop);
	data->ctx = ctx;

	// send req login
	NET_MODEL_NEW_STACK_MSG(MSG_TYPE_REQ_LOGIN, msg_req_login_t, req);
	req->user_id = data->user_id;
	NET_MODEL_SND_MSG(data->ctx, req);
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

	if (data->ctx == NULL) {
		return;
	}
	if (data->is_login == 0) {
		return;
	}
	if (data->curr_idx >= data->max_idx) {
		if (data->finish_ts.tv_sec > 0) {
			struct timespec curr_ts;
			timespec_get(&curr_ts, TIME_UTC);
			if (curr_ts.tv_sec - data->finish_ts.tv_sec > 3) {
				LOG_INFO("exit evloop and dump orders");
				muggle_evloop_exit(evloop);
			}
		}
		return;
	}

	struct timespec curr_ts;
	timespec_get(&curr_ts, TIME_UTC);

	uint64_t elapsed_ms = (curr_ts.tv_sec - data->last_ts.tv_sec) * 1000 +
						  curr_ts.tv_nsec / 1000000 -
						  data->last_ts.tv_nsec / 1000000;
	if (elapsed_ms < data->round_interval_ms) {
		return;
	}
	memcpy(&data->last_ts, &curr_ts, sizeof(data->last_ts));

	for (uint32_t i = 0; i < data->order_per_round; ++i) {
		NET_MODEL_NEW_STACK_MSG(MSG_TYPE_REQ_ORDER, order_t, req);
		memcpy(req, data->orders + i, sizeof(order_t));
		req->user_id = data->user_id;
		req->coid = data->curr_idx;

		timespec_get(&curr_ts, TIME_UTC);
		req->client_snd_ts.sec = curr_ts.tv_sec;
		req->client_snd_ts.nsec = curr_ts.tv_nsec;

		NET_MODEL_SND_MSG(data->ctx, req);

		LOG_DEBUG("client send order: "
				  "uid=%u, coid=%llu, ts=%llu.%lu",
				  req->user_id, (unsigned long long)req->coid,
				  (unsigned long long)req->client_snd_ts.sec,
				  (unsigned long)req->client_snd_ts.nsec);

		++data->curr_idx;
	}
}
void on_close(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx)
{
	MUGGLE_UNUSED(evloop);
	MUGGLE_UNUSED(ctx);

	LOG_INFO("on close");
}
void on_release(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx)
{
	LOG_INFO("on release");

	muggle_bytes_buffer_t *bytes_buf = muggle_socket_ctx_get_data(ctx);
	if (bytes_buf) {
		muggle_bytes_buffer_destroy(bytes_buf);
		free(bytes_buf);
		muggle_socket_ctx_set_data(ctx, NULL);
	}

	muggle_evloop_exit(evloop);
}
