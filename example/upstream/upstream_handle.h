#ifndef UPSTREAM_HANDLE_H_
#define UPSTREAM_HANDLE_H_

#include "muggle/c/muggle_c.h"
#include "haclog/haclog.h"
#include "netmodel/net_model_type.h"

typedef struct evloop_data {
	const char *host;
	const char *serv;

	muggle_event_loop_t *evloop;

	muggle_linked_list_t order_list;
	struct timespec last_ts;
	uint64_t timer_interval_ms;
} evloop_data_t;

typedef struct order_store {
	muggle_socket_context_t *ctx;
	order_t order;
} order_store_t;

void on_add_ctx(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx);
void on_connect(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx);
void on_message(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx);
void on_timer(muggle_event_loop_t *evloop);
void on_close(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx);
void on_release(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx);

void free_order(void *pool, void *data);

#endif // !UPSTREAM_HANDLE_H_
