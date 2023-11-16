#include "client_handle.h"

typedef struct args {
	char host[16];
	char port[16];

	uint32_t user_id;
	uint32_t round;
	uint32_t order_per_round;
	uint32_t round_interval_ms;

	uint32_t is_performance;
} args_t;

bool parse_args(int argc, char **argv, args_t *args)
{
	int c;

	memset(args, 0, sizeof(*args));
	strncpy(args->host, "127.0.0.1", sizeof(args->host) - 1);
	strncpy(args->port, "10102", sizeof(args->port) - 1);
	args->user_id = 1;
	args->round = 1000;
	args->order_per_round = 1;
	args->round_interval_ms = 1;
	args->is_performance = 1;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "help", no_argument, NULL, 'h' },
			{ "host", required_argument, NULL, 'H' },
			{ "port", required_argument, NULL, 'P' },
			{ "user", required_argument, NULL, 'u' },
			{ "round", required_argument, NULL, 'r' },
			{ "order_per_round", required_argument, NULL, 'o' },
			{ "interval", required_argument, NULL, 't' },
			{ "cpu-freq", required_argument, NULL, 'c' },
			{ NULL, 0, NULL, 0 }
		};

		c = getopt_long(argc, argv, "hH:P:u:r:o:t:c:", long_options,
						&option_index);
		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h': {
			fprintf(stdout,
					"Usage: %s <options>\n"
					"    -H, --host            server host\n"
					"    -P, --port            server port\n"
					"    -u, --user            user id\n"
					"    -r, --round           round of order insert\n"
					"    -o, --order_per_round order of per round\n"
					"    -t, --interval        timer interval between round\n"
					"    -c, --cpu-freq   cpu frequency set\n"
					"        * performance: maximum performance\n"
					"        * powersave: energy saving\n"
					"",
					argv[0]);
			exit(EXIT_SUCCESS);
		} break;
		case 'H': {
			strncpy(args->host, optarg, sizeof(args->host) - 1);
		} break;
		case 'P': {
			strncpy(args->port, optarg, sizeof(args->port) - 1);
		} break;
		case 'u': {
			unsigned long ul;
			if (muggle_str_toul(optarg, &ul, 10) == 0) {
				LOG_ERROR("invalid 'user' value: %s", optarg);
				return false;
			}
			args->user_id = ul;
		} break;
		case 'r': {
			unsigned long ul;
			if (muggle_str_toul(optarg, &ul, 10) == 0) {
				LOG_ERROR("invalid 'round' value: %s", optarg);
				return false;
			}
			args->round = ul;
		} break;
		case 'o': {
			unsigned long ul;
			if (muggle_str_toul(optarg, &ul, 10) == 0) {
				LOG_ERROR("invalid 'order_per_round' value: %s", optarg);
				return false;
			}
			args->order_per_round = ul;
		} break;
		case 't': {
			unsigned long ul;
			if (muggle_str_toul(optarg, &ul, 10) == 0) {
				LOG_ERROR("invalid 'interval' value: %s", optarg);
				return false;
			}
			args->round_interval_ms = ul;
		} break;
		case 'c': {
			if (strcmp(optarg, "performance") == 0) {
				args->is_performance = 1;
			} else if (strcmp(optarg, "powersave") == 0) {
				args->is_performance = 0;
			} else {
				LOG_ERROR("invalid 'cpu-freq' value: %s", optarg);
				return false;
			}
		} break;
		}
	}

	fprintf(stdout,
			"Launch Upstream\n"
			"----------------\n"
			"host: %s\n"
			"port: %s\n"
			"user_id: %u\n"
			"round: %lu\n"
			"order_per_round: %lu\n"
			"interval: %lu ms\n"
			"cpu-freq: %s\n"
			"----------------\n"
			"",
			args->host, args->port, args->user_id, (unsigned long)args->round,
			(unsigned long)args->order_per_round,
			(unsigned long)args->round_interval_ms,
			args->is_performance ? "performance" : "powersave");

	return true;
}

order_t *load_orders(uint32_t *p_total_orders)
{
	order_t *orders = NULL;

	FILE *fp = fopen("orders.bin", "rb");
	if (fp == NULL) {
		LOG_ERROR("failed open orders.bin\n");
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	long num_bytes = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	*p_total_orders = num_bytes / sizeof(order_t);
	orders = (order_t *)malloc(sizeof(order_t) * (*p_total_orders));
	if (orders == NULL) {
		fclose(fp);
		LOG_ERROR("failed allocate memory space for orders");
		return NULL;
	}

	unsigned long n = fread(orders, sizeof(order_t), *p_total_orders, fp);
	if (n != *p_total_orders) {
		free(orders);
		fclose(fp);
		LOG_ERROR("failed read orders");
		return NULL;
	}

	fclose(fp);

	LOG_INFO("load orders: %u", *p_total_orders);

	return orders;
}

muggle_event_loop_t *create_evloop()
{
	// new event loop
	muggle_event_loop_init_args_t ev_init_args;
	memset(&ev_init_args, 0, sizeof(ev_init_args));
	ev_init_args.evloop_type = MUGGLE_EVLOOP_TYPE_NULL;
	ev_init_args.hints_max_fd = 32;
	ev_init_args.use_mem_pool = 0;

	muggle_event_loop_t *evloop = muggle_evloop_new(&ev_init_args);
	if (evloop == NULL) {
		LOG_ERROR("failed new event loop");
		exit(EXIT_FAILURE);
	}
	LOG_INFO("success new event loop");
	muggle_evloop_set_data(evloop, NULL);

	return evloop;
}

static muggle_thread_ret_t tcp_client_routine(void *p_args)
{
	evloop_data_t *args = (evloop_data_t *)p_args;

	muggle_socket_t fd = MUGGLE_INVALID_SOCKET;
	do {
		LOG_INFO("try tcp connect: %s %s", args->host, args->serv);
		fd = muggle_tcp_connect(args->host, args->serv, 3);
		if (fd == MUGGLE_INVALID_SOCKET) {
			// LOG_SYS_ERR(LOG_LEVEL_ERROR, "failed connect");
			LOG_ERROR("failed connect to: %s %s", args->host, args->serv);
			muggle_msleep(3000);
			continue;
		}

		int enable = 1;
		muggle_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&enable,
						  sizeof(enable));

		break;
	} while (1);

	LOG_INFO("success tcp connect");

	muggle_socket_context_t *ctx =
		(muggle_socket_context_t *)malloc(sizeof(muggle_socket_context_t));
	muggle_socket_ctx_init(ctx, fd, NULL, MUGGLE_SOCKET_CTX_TYPE_TCP_CLIENT);

	muggle_socket_evloop_add_ctx(args->evloop, ctx);

	return 0;
}

void run_tcp_client(evloop_data_t *args)
{
	if (args->host && args->serv) {
		muggle_thread_t th;
		muggle_thread_create(&th, tcp_client_routine, args);
		muggle_thread_detach(&th);
	}
}

uint64_t get_elapsed_ns(order_ts_t *t1, order_ts_t *t2)
{
	return (t2->sec - t1->sec) * 1000000000 + t2->nsec - t1->nsec;
}

int cmp_uint64(const void *a, const void *b)
{
	uint64_t *p1 = (uint64_t *)a;
	uint64_t *p2 = (uint64_t *)b;
	if (*p1 < *p2) {
		return -1;
	} else if (*p1 > *p2) {
		return 1;
	} else {
		return 0;
	}
}

void output_percentile(uint64_t *arr, uint64_t cnt)
{
	for (uint64_t i = 0; i <= 100; i += 10) {
		uint64_t pos = i * (cnt / 100);
		if (pos >= cnt) {
			pos = cnt - 1;
		}
		fprintf(stdout, "%llu%%: %llu\n", (unsigned long long)i,
				(unsigned long long)arr[pos]);
	}
}

void dump_orders(uint32_t user_id, order_t *orders, uint32_t cnt)
{
	char filename[64];
	snprintf(filename, sizeof(filename), "cli_orders_%u.csv", user_id);

	FILE *fp = fopen(filename, "wb");
	if (fp == NULL) {
		LOG_ERROR("failed open %s for write", filename);
		return;
	}

	uint64_t *elapsed_arr[8];
	for (size_t i = 0; i < sizeof(elapsed_arr) / sizeof(elapsed_arr[0]); i++) {
		elapsed_arr[i] = (uint64_t *)malloc(sizeof(uint64_t) * cnt);
	}

	for (uint32_t i = 0; i < cnt; i++) {
		order_t *order = &orders[i];
		uint64_t oid = (uint64_t)user_id << 32 | order->coid;
		fprintf(fp, "%llu,client.snd,%llu.%lu\n", (unsigned long long)oid,
				(unsigned long long)order->client_snd_ts.sec,
				(unsigned long)order->client_snd_ts.nsec);
		fprintf(fp, "%llu,server.rcv,%llu.%lu\n", (unsigned long long)oid,
				(unsigned long long)order->server_rcv_ts.sec,
				(unsigned long)order->server_rcv_ts.nsec);
		fprintf(fp, "%llu,server.snd,%llu.%lu\n", (unsigned long long)oid,
				(unsigned long long)order->server_snd_ts.sec,
				(unsigned long)order->server_snd_ts.nsec);
		fprintf(fp, "%llu,upstream.rcv,%llu.%lu\n", (unsigned long long)oid,
				(unsigned long long)order->upstream_rcv_ts.sec,
				(unsigned long)order->upstream_rcv_ts.nsec);
		fprintf(fp, "%llu,upstream.snd,%llu.%lu\n", (unsigned long long)oid,
				(unsigned long long)order->upstream_snd_ts.sec,
				(unsigned long)order->upstream_snd_ts.nsec);
		fprintf(fp, "%llu,server.rcv_rsp,%llu.%lu\n", (unsigned long long)oid,
				(unsigned long long)order->server_rcv_rsp_ts.sec,
				(unsigned long)order->server_rcv_rsp_ts.nsec);
		fprintf(fp, "%llu,server.snd_rsp,%llu.%lu\n", (unsigned long long)oid,
				(unsigned long long)order->server_snd_rsp_ts.sec,
				(unsigned long)order->server_snd_rsp_ts.nsec);
		fprintf(fp, "%llu,client.rcv_rsp,%llu.%lu\n", (unsigned long long)oid,
				(unsigned long long)order->client_rcv_rsp_ts.sec,
				(unsigned long)order->client_rcv_rsp_ts.nsec);

		elapsed_arr[0][i] =
			get_elapsed_ns(&order->client_snd_ts, &order->server_rcv_ts);
		elapsed_arr[1][i] =
			get_elapsed_ns(&order->server_rcv_ts, &order->server_snd_ts);
		elapsed_arr[2][i] =
			get_elapsed_ns(&order->server_snd_ts, &order->upstream_rcv_ts);
		elapsed_arr[3][i] =
			get_elapsed_ns(&order->upstream_snd_ts, &order->server_rcv_rsp_ts);
		elapsed_arr[4][i] = get_elapsed_ns(&order->server_rcv_rsp_ts,
										   &order->server_snd_rsp_ts);
		elapsed_arr[5][i] = get_elapsed_ns(&order->server_snd_rsp_ts,
										   &order->client_rcv_rsp_ts);
	}

	for (size_t i = 0; i < sizeof(elapsed_arr) / sizeof(elapsed_arr[0]); i++) {
		qsort(elapsed_arr[i], cnt, sizeof(uint64_t), cmp_uint64);
	}

	fprintf(stdout, "client.snd -> server.rcv\n");
	output_percentile(elapsed_arr[0], cnt);
	fprintf(stdout, "server.rcv-> server.snd\n");
	output_percentile(elapsed_arr[1], cnt);
	fprintf(stdout, "server.snd -> upstream.rcv\n");
	output_percentile(elapsed_arr[2], cnt);
	fprintf(stdout, "upstream.snd_rsp -> server.rcv_rsp\n");
	output_percentile(elapsed_arr[3], cnt);
	fprintf(stdout, "server.rcv_rsp -> server.snd_rsp\n");
	output_percentile(elapsed_arr[4], cnt);
	fprintf(stdout, "server.snd_rsp -> client.rcv_rsp\n");
	output_percentile(elapsed_arr[5], cnt);

	for (size_t i = 0; i < sizeof(elapsed_arr) / sizeof(elapsed_arr[0]); i++) {
		free(elapsed_arr[i]);
	}

	fclose(fp);
}

void init_log()
{
	static haclog_file_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	if (haclog_file_handler_init(&handler, "logs/client.log", "a") != 0) {
		fprintf(stderr, "failed init haclog file handler");
		exit(EXIT_FAILURE);
	}
	haclog_handler_set_level((haclog_handler_t *)&handler, HACLOG_LEVEL_DEBUG);
	haclog_context_add_handler((haclog_handler_t *)&handler);

	haclog_backend_run();
}

int main(int argc, char *argv[])
{
	// init log
	init_log();

	// init socket library
	if (muggle_socket_lib_init() != 0) {
		LOG_ERROR("failed init socket library");
		exit(EXIT_FAILURE);
	}

	// parse arguments
	args_t args;
	if (!parse_args(argc, argv, &args)) {
		LOG_ERROR("failed parse arguments");
		exit(EXIT_FAILURE);
	}

	// load orders
	uint32_t total_orders = 0;
	order_t *orders = load_orders(&total_orders);
	if (orders == NULL) {
		LOG_ERROR("failed load orders");
		exit(EXIT_FAILURE);
	}

	if (args.round * args.order_per_round > total_orders) {
		LOG_ERROR("number of order < need order");
		exit(EXIT_FAILURE);
	}

	// init event loop
	muggle_event_loop_t *evloop = create_evloop();

	muggle_socket_evloop_handle_t handle;
	muggle_socket_evloop_handle_init(&handle);
	if (args.is_performance) {
		muggle_socket_evloop_handle_set_timer_interval(&handle, 0);
	} else {
		muggle_socket_evloop_handle_set_timer_interval(&handle,
													   args.round_interval_ms);
	}
	muggle_socket_evloop_handle_set_cb_add_ctx(&handle, on_add_ctx);
	muggle_socket_evloop_handle_set_cb_msg(&handle, on_message);
	muggle_socket_evloop_handle_set_cb_close(&handle, on_close);
	muggle_socket_evloop_handle_set_cb_timer(&handle, on_timer);
	muggle_socket_evloop_handle_set_cb_release(&handle, on_release);
	muggle_socket_evloop_handle_attach(&handle, evloop);
	LOG_INFO("socket handle attached to event loop");

	// evloop user data
	evloop_data_t data;
	memset(&data, 0, sizeof(data));
	data.host = args.host;
	data.serv = args.port;
	data.ctx = NULL;
	data.evloop = evloop;
	data.user_id = args.user_id;
	data.is_login = 0;
	data.orders = orders;
	data.total_orders = total_orders;
	data.curr_idx = 0;
	data.max_idx = args.round * args.order_per_round;
	data.round = args.round;
	data.order_per_round = args.order_per_round;
	data.round_interval_ms = args.round_interval_ms;
	timespec_get(&data.last_ts, TIME_UTC);
	data.rsp_pos = 0;

	muggle_evloop_set_data(evloop, &data);

	// run client
	run_tcp_client(&data);

	// run evloop
	muggle_evloop_run(evloop);

	// cleanup
	muggle_socket_evloop_handle_destroy(&handle);
	muggle_evloop_delete(evloop);

	// dump orders
	dump_orders(args.user_id, orders, data.max_idx);

	return 0;
}
