#ifndef LISTEN_HANDLE_H_
#define LISTEN_HANDLE_H_

#include "muggle/c/muggle_c.h"
#include "netmodel/net_model_type.h"
#include "server_context.h"

typedef struct evloop_listen_data {
	muggle_event_loop_t *evloop;
	server_context_t *server_ctx;

	uint32_t num_worker;
	muggle_event_loop_t **workers;
} evloop_listen_data_t;

void on_listen_add_ctx(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx);
void on_listen_connect(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx);
void on_listen_message(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx);
void on_listen_close(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx);
void on_listen_release(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx);

#endif // !LISTEN_HANDLE_H_
