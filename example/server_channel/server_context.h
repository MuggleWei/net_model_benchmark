#ifndef SERVER_CONTEXT_H_
#define SERVER_CONTEXT_H_

#include "user_context.h"

typedef struct server_config {
	char host[16];
	char port[16];
	char upstream_host[16];
	char upstream_port[16];
	uint32_t num_worker;
	uint32_t is_performance;
} server_config_t;

typedef struct server_context {
	server_config_t *cfg;
	muggle_channel_t *upstream_chan;
	muggle_channel_t *client_chan;
	user_context_t user_ctxs[256];
} server_context_t;

enum {
	CHAN_MSG_TYPE_NULL = 0,
	CHAN_MSG_TYPE_ADD_CTX = MSG_TYPE_REQ_LOGIN,
	CHAN_MSG_TYPE_DEL_CTX = MSG_TYPE_RSP_LOGIN,
};

typedef struct chan_msg {
	uint32_t msg_type;
	uint32_t num_bytes;
	muggle_socket_context_t *ctx;
} chan_msg_t;

bool server_context_init(server_context_t *ctx, server_config_t *cfg,
						 muggle_channel_t *upstream_chan,
						 muggle_channel_t *client_chan);

void server_context_run(server_context_t *ctx);

void server_context_push(muggle_channel_t *chan,
						 muggle_socket_context_t *ctx, uint32_t msg_type,
						 uint32_t num_bytes, void *data);

void server_context_del_ctx(server_context_t *server_ctx,
							muggle_socket_context_t *ctx);

#endif // !SERVER_CONTEXT_H_
