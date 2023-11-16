#ifndef NET_MODEL_TYPE_H_
#define NET_MODEL_TYPE_H_

#include "muggle/c/base/macro.h"
#include <stdint.h>

EXTERN_C_BEGIN

	; // for avoid LSP complain
#pragma pack(push)
#pragma pack(1)

/**
 * @brief message type
 */
enum {
	MSG_TYPE_NULL = 0,
	MSG_TYPE_REQ_LOGIN,
	MSG_TYPE_RSP_LOGIN,
	MSG_TYPE_REQ_ORDER,
	MSG_TYPE_RSP_ORDER,
};

/**
 * @brief order status
 */
enum {
	ORDER_STATUS_NULL = 0,
	ORDER_STATUS_NEW,
	ORDER_STATUS_REJECT,
	ORDER_STATUS_PART_DEAL,
	ORDER_STATUS_TOTAL_DEAL,
	MAX_ORDER_STATUS,
};

enum {
	ORDER_ACTION_NULL = 0,
	ORDER_ACTION_OPEN,
	ORDER_ACTION_CLOSE,
	ORDER_ACTION_CLOSE_HISTORY,
	MAX_ORDER_ACTION,
};

enum {
	ORDER_DIR_NULL = 0,
	ORDER_DIR_LONG,
	ORDER_DIR_SHORT,
	MAX_ORDER_DIR,
};

enum {
	MARKET_NULL = 0,
	MARKET_SHFE,
	MARKET_DCE,
	MARKET_CZCE,
	MARKET_CFFEX,
	MARKET_INE,
	MARKET_GFEX,
	MARKET_SSE,
	MARKET_SZSE,
	MAX_MARKET,
};

#define NET_MODEL_HDR_MAGIC_WORD "NETM"

/**
 * @brief message head
 */
typedef struct msg_hdr {
	char magic[4]; //!< magic word
	char flags[4]; //!< flags
	uint32_t msg_type; //!< message type
	uint32_t payload_len; //!< payload length
} msg_hdr_t;

/**
 * @brief message - req login
 */
typedef struct msg_req_login {
	// NOTE: assume always login success, so this without password
	uint32_t user_id;
} msg_req_login_t;

/**
 * @brief message - rsp login
 */
typedef struct msg_rsp_login {
	uint32_t user_id;
	uint32_t is_success;
} msg_rsp_login_t;

/**
 * @brief order id
 */
typedef union {
	uint64_t id; //!< order id
	struct {
		uint16_t server_id; //!< server id
		uint8_t thread_id; //!< thread id
		uint8_t reserved; //!< reserved field
		uint32_t seq_id; //!< order sequence id
	};
} order_id_t;

typedef struct order_ts {
	uint64_t sec; //!< second
	uint32_t nsec; //!< nano-second
	uint32_t align_placeholder; //!< memory align placeholder
} order_ts_t;

#define MAX_ORDER_RSP 3

/**
 * @brief order
 */
typedef struct order {
	uint32_t user_id; //!< user id
	uint32_t session_id; //!< server session id
	order_id_t oid; //!< order id
	uint64_t coid; //!< client order id
	union {
		uint64_t order_info_placeholder;
		struct {
			uint32_t market; //!< MARKET_*
			uint8_t order_status; //!< ORDER_STATUS_*
			uint8_t order_action; //!< ORDER_ACTION_*
			uint8_t order_dir; //!< ORDER_DIR_*
			uint8_t order_info_reserved; //!< reserved field
		};
	};
	char instrument[16]; //!< instrument
	double price; //!< order price
	uint64_t qty; //!< quantity
	uint64_t cal_num[8]; //!< calculate number is prime
	order_ts_t client_snd_ts; //!< client send timestamp
	order_ts_t server_rcv_ts; //!< server receive timestamp
	order_ts_t server_snd_ts; //!< server send timestamp
	order_ts_t upstream_rcv_ts; //!< upstream receive timestamp
	order_ts_t upstream_snd_ts; //!< upstream send rsp timestamp
	order_ts_t server_rcv_rsp_ts; //!< server receive rsp timestamp
	order_ts_t server_snd_rsp_ts; //!< server send rsp timestamp
	order_ts_t client_rcv_rsp_ts; //!< client receive rsp timestamp
	uint64_t rsp_cnt; //!< response count
} order_t;

#pragma pack(pop)

EXTERN_C_END

#endif // !NET_MODEL_TYPE_H_
