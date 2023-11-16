#ifndef WORKER_HANDLE_H_
#define WORKER_HANDLE_H_

#include "server_context.h"

typedef struct worker_evloop_data {
	muggle_event_loop_t *evloop;
	server_context_t *server_ctx;
} worker_evloop_data_t;

void on_worker_add_ctx(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx);
void on_worker_message(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx);
void on_worker_close(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx);
void on_worker_release(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx);

#endif // !WORKER_HANDLE_H_
