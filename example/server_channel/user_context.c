#include "user_context.h"

bool user_context_init(user_context_t *user_ctx)
{
	memset(user_ctx, 0, sizeof(*user_ctx));

	muggle_spinlock_init(&user_ctx->lock);
	user_ctx->head.session = NULL;
	user_ctx->head.next = NULL;

	return true;
}

bool user_context_add(user_context_t *user_ctx, session_t *session)
{
	if (session->user_id == 0) {
		return false;
	}

	muggle_spinlock_lock(&user_ctx->lock);

	user_session_linked_node_t *node = (user_session_linked_node_t *)malloc(
		sizeof(user_session_linked_node_t));
	node->session = session;
	node->next = user_ctx->head.next;
	user_ctx->head.next = node;

	muggle_spinlock_unlock(&user_ctx->lock);

	return true;
}
void user_context_del(user_context_t *user_ctx, session_t *session)
{
	if (session->user_id == 0) {
		return;
	}

	muggle_spinlock_lock(&user_ctx->lock);

	user_session_linked_node_t *prev = &user_ctx->head;
	user_session_linked_node_t *node = user_ctx->head.next;
	while (node) {
		if (node->session == session) {
			prev->next = node->next;
			free(node);
			break;
		} else {
			prev = node;
			node = node->next;
		}
	}

	muggle_spinlock_unlock(&user_ctx->lock);
}
