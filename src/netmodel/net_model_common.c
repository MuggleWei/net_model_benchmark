#include "net_model_common.h"
#include "haclog/haclog.h"

#define RECV_UNIT_SIZE 4096

static const char *s_magic = NET_MODEL_HDR_MAGIC_WORD;

bool netmodel_read_bytes(muggle_socket_context_t *ctx,
						 muggle_bytes_buffer_t *bytes_buf)
{
	while (true) {
		void *p = muggle_bytes_buffer_writer_fc(bytes_buf, RECV_UNIT_SIZE);
		if (p == NULL) {
			char buf[64];
			muggle_event_fd fd = muggle_socket_ctx_get_fd(ctx);
			muggle_socket_remote_addr(fd, buf, sizeof(buf), 0);
			LOG_WARNING("bytes buffer full: addr=%s", buf);
			muggle_ev_ctx_set_flag((muggle_event_context_t *)ctx,
								   MUGGLE_EV_CTX_FLAG_CLOSED);
			return false;
		}

		int n = muggle_socket_ctx_read(ctx, p, RECV_UNIT_SIZE);
		if (n > 0) {
			muggle_bytes_buffer_writer_move(bytes_buf, n);
		}
		if (n < RECV_UNIT_SIZE) {
			break;
		}
	}

	return true;
}

bool netmodel_decode(muggle_event_loop_t *evloop, muggle_socket_context_t *ctx,
					 muggle_bytes_buffer_t *bytes_buf, fn_netmode_on_msg cb)
{
	// read bytes
	if (!netmodel_read_bytes(ctx, bytes_buf)) {
		return false;
	}

	// parse message
	msg_hdr_t hdr;
	while (true) {
		if (!muggle_bytes_buffer_fetch(bytes_buf, (int)sizeof(hdr), &hdr)) {
			break;
		}

		// check magic
		if (*(uint32_t *)hdr.magic != *(uint32_t *)s_magic) {
			char buf[64];
			muggle_event_fd fd = muggle_socket_ctx_get_fd(ctx);
			muggle_socket_remote_addr(fd, buf, sizeof(buf), 0);
			LOG_WARNING("invalid magic word: addr=%s", buf);
			muggle_ev_ctx_set_flag((muggle_event_context_t *)ctx,
								   MUGGLE_EV_CTX_FLAG_CLOSED);
			return false;
		}

		// check message length
		// NOTE:
		//   Cause this project just for test, without check message length.
		//   So do not copy this decode into product
		int total_bytes = (int)sizeof(hdr) + hdr.payload_len;

		int readable = muggle_bytes_buffer_readable(bytes_buf);
		if (readable < total_bytes) {
			break;
		}

		// handle message
		msg_hdr_t *p_msg = (msg_hdr_t *)muggle_bytes_buffer_reader_fc(
			bytes_buf, (int)total_bytes);
		if (p_msg) {
			cb(evloop, ctx, p_msg, (void *)(p_msg + 1));
			muggle_bytes_buffer_reader_move(bytes_buf, (int)total_bytes);
		} else {
			// discontinuous memory
			p_msg = (msg_hdr_t *)malloc(total_bytes);
			muggle_bytes_buffer_read(bytes_buf, (int)total_bytes,
									 (void *)p_msg);

			cb(evloop, ctx, p_msg, (void *)(p_msg + 1));

			free(p_msg);
		}
	}

	return true;
}
