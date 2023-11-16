#ifndef WORKER_HANDLE_H_
#define WORKER_HANDLE_H_

#include "muggle/c/muggle_c.h"
#include "haclog/haclog.h"
#include "netmodel/net_model_type.h"

typedef struct worker_user_data {
	uint32_t is_detached;
	uint32_t user_id;
	uint32_t is_client;
	char remote_addr[64];
	muggle_bytes_buffer_t bytes_buf;
} worker_user_data_t;

typedef struct worker_user_linked_node {
	struct worker_user_linked_node *next;
	muggle_socket_context_t *ctx;
} worker_user_linked_node_t;

typedef struct worker_evloop_data {
	muggle_event_loop_t *evloop;
	const char *upstream_host;
	const char *upstream_port;
	muggle_socket_context_t *upstream_ctx;

	worker_user_linked_node_t user_ctxs[256];
} worker_evloop_data_t;

muggle_socket_context_t *connect_upstream(worker_evloop_data_t *data);

void on_worker_add_ctx(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx);
void on_worker_message(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx);
void on_worker_close(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx);
void on_worker_release(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx);

#endif // !WORKER_HANDLE_H_
