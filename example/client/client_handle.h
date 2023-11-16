#ifndef CLIENT_HANDLE_H_
#define CLIENT_HANDLE_H_

#include "muggle/c/muggle_c.h"
#include "haclog/haclog.h"
#include "netmodel/net_model_type.h"

typedef struct evloop_data {
	const char *host;
	const char *serv;

	muggle_event_loop_t *evloop;
	muggle_socket_context_t *ctx;

	uint32_t user_id;
	uint32_t is_login;

	order_t *orders;
	uint32_t total_orders;

	uint32_t curr_idx;
	uint32_t max_idx;

	uint32_t round;
	uint32_t order_per_round;
	uint32_t round_interval_ms;

	struct timespec last_ts;
	uint32_t rsp_pos;

	struct timespec finish_ts;
} evloop_data_t;

void on_add_ctx(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx);
void on_message(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx);
void on_timer(muggle_event_loop_t *evloop);
void on_close(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx);
void on_release(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx);

#endif // !CLIENT_HANDLE_H_
