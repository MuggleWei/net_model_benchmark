#ifndef UPSTREAM_HANDLE_H_
#define UPSTREAM_HANDLE_H_

#include "server_context.h"

muggle_socket_context_t *connect_upstream(server_config_t *cfg);

void on_upstream_add_ctx(muggle_event_loop_t *evloop,
						 muggle_socket_context_t *ctx);
void on_upstream_message(muggle_event_loop_t *evloop,
						 muggle_socket_context_t *ctx);
void on_upstream_close(muggle_event_loop_t *evloop,
					   muggle_socket_context_t *ctx);
void on_upstream_release(muggle_event_loop_t *evloop,
						 muggle_socket_context_t *ctx);

#endif // !UPSTREAM_HANDLE_H_
