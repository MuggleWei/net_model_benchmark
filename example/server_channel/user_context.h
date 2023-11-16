#ifndef USER_CONTEXT_H_
#define USER_CONTEXT_H_

#include "muggle/c/muggle_c.h"
#include "netmodel/net_model_type.h"

typedef struct session {
	muggle_socket_context_t *ctx;
	uint32_t is_detached;
	uint32_t user_id;
	uint32_t is_client;
	char remote_addr[64];
	muggle_bytes_buffer_t bytes_buf;
} session_t;

typedef struct user_session_linked_node {
	struct user_session_linked_node *next;
	session_t *session;
} user_session_linked_node_t;

typedef struct user_context {
	muggle_spinlock_t lock;
	user_session_linked_node_t head;
} user_context_t;

bool user_context_init(user_context_t *user_ctx);

bool user_context_add(user_context_t *user_ctx, session_t *session);
void user_context_del(user_context_t *user_ctx, session_t *session);

#endif // !USER_CONTEXT_H_
