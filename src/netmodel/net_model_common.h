#ifndef NET_MODEL_COMMON_H_
#define NET_MODEL_COMMON_H_

#include "netmodel/net_model_type.h"
#include "muggle/c/muggle_c.h"

EXTERN_C_BEGIN

#define NET_MODEL_NEW_STACK_MSG(msgtype, msg_struct, var)               \
	char msg_placeholder_##var[sizeof(msg_hdr_t) + sizeof(msg_struct)]; \
	memset(msg_placeholder_##var, 0, sizeof(msg_placeholder_##var));    \
	msg_hdr_t *hdr_##var = (msg_hdr_t *)msg_placeholder_##var;          \
	memcpy(hdr_##var->magic, NET_MODEL_HDR_MAGIC_WORD,                  \
		   sizeof(hdr_##var->magic));                                   \
	hdr_##var->msg_type = msgtype;                                      \
	hdr_##var->payload_len = sizeof(msg_struct);                        \
	msg_struct *var = (msg_struct *)(hdr_##var + 1);

#define NET_MODEL_SND_MSG(ctx, var) \
	muggle_socket_ctx_write(ctx, hdr_##var, sizeof(msg_hdr_t) + sizeof(*var));

typedef void (*fn_netmode_on_msg)(muggle_event_loop_t *evloop,
								  muggle_socket_context_t *ctx, msg_hdr_t *hdr,
								  void *payload);

/**
 * @brief decode message
 *
 * @param evloop     event loop
 * @param ctx        socket context
 * @param bytes_buf  bytes buffer
 * @param cb         message callback
 *
 * @return 
 */
bool netmodel_decode(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx,
					 muggle_bytes_buffer_t *bytes_buf, fn_netmode_on_msg cb);

EXTERN_C_END

#endif // !NET_MODEL_COMMON_H_
