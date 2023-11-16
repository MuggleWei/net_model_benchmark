#include "upstream_handle.h"

typedef struct args {
	char host[16];
	char port[16];
	uint64_t timer_interval;
	uint32_t is_performance;
} args_t;

bool parse_args(int argc, char **argv, args_t *args)
{
	int c;

	memset(args, 0, sizeof(*args));
	strncpy(args->host, "127.0.0.1", sizeof(args->host) - 1);
	strncpy(args->port, "16666", sizeof(args->port) - 1);
	args->timer_interval = 1;
	args->is_performance = 1;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "help", no_argument, NULL, 'h' },
			{ "host", required_argument, NULL, 'H' },
			{ "port", required_argument, NULL, 'P' },
			{ "interval", required_argument, NULL, 't' },
			{ "cpu-freq", required_argument, NULL, 'c' },
			{ NULL, 0, NULL, 0 }
		};

		c = getopt_long(argc, argv, "hH:P:t:c:", long_options, &option_index);
		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h': {
			fprintf(stdout,
					"Usage: %s <options>\n"
					"    -H, --host       server host\n"
					"    -P, --port       server port\n"
					"    -t, --interval   timer interval\n"
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
		case 't': {
			unsigned long long ull;
			if (muggle_str_toull(optarg, &ull, 10) == 0) {
				LOG_ERROR("invalid 'interval' value: %s", optarg);
				return false;
			}
			args->timer_interval = ull;
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
		default: {
			fprintf(stderr, "invalid option: %d\n", c);
		} break;
		}
	}

	fprintf(stdout,
			"Launch Upstream\n"
			"----------------\n"
			"host: %s\n"
			"port: %s\n"
			"interval: %llu ms\n"
			"cpu-freq: %s\n"
			"----------------\n"
			"",
			args->host, args->port, (unsigned long long)args->timer_interval,
			args->is_performance ? "performance" : "powersave");

	return true;
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

static muggle_thread_ret_t tcp_listen_routine(void *p_args)
{
	evloop_data_t *args = (evloop_data_t *)p_args;

	muggle_socket_t fd = MUGGLE_INVALID_SOCKET;
	do {
		LOG_INFO("try tcp listen: %s %s", args->host, args->serv);
		fd = muggle_tcp_listen(args->host, args->serv, 512);
		if (fd == MUGGLE_INVALID_SOCKET) {
			// LOG_SYS_ERR(LOG_LEVEL_ERROR, "failed listen");
			LOG_ERROR("failed listen: %s %s", args->host, args->serv);
			muggle_msleep(3000);
			continue;
		}
		break;
	} while (1);

	LOG_INFO("success tcp listen");

	muggle_socket_context_t *ctx =
		(muggle_socket_context_t *)malloc(sizeof(muggle_socket_context_t));
	muggle_socket_ctx_init(ctx, fd, NULL, MUGGLE_SOCKET_CTX_TYPE_TCP_LISTEN);

	muggle_socket_evloop_add_ctx(args->evloop, ctx);

	return 0;
}

void run_tcp_listen(evloop_data_t *args)
{
	if (args->host && args->serv) {
		muggle_thread_t th;
		muggle_thread_create(&th, tcp_listen_routine, args);
		muggle_thread_detach(&th);
	}
}

void init_log()
{
	static haclog_file_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	if (haclog_file_handler_init(&handler, "logs/upstream.log", "a") != 0) {
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

	// init event loop
	muggle_event_loop_t *evloop = create_evloop();

	muggle_socket_evloop_handle_t handle;
	muggle_socket_evloop_handle_init(&handle);
	if (args.is_performance) {
		muggle_socket_evloop_handle_set_timer_interval(&handle, 0);
	} else {
		muggle_socket_evloop_handle_set_timer_interval(&handle,
													   args.timer_interval);
	}
	muggle_socket_evloop_handle_set_cb_add_ctx(&handle, on_add_ctx);
	muggle_socket_evloop_handle_set_cb_conn(&handle, on_connect);
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
	data.evloop = evloop;
	muggle_linked_list_init(&data.order_list, 0);
	timespec_get(&data.last_ts, TIME_UTC);
	data.timer_interval_ms = args.timer_interval;

	muggle_evloop_set_data(evloop, &data);

	// run listen
	run_tcp_listen(&data);

	// run evloop
	muggle_evloop_run(evloop);

	// cleanup
	muggle_socket_evloop_handle_destroy(&handle);
	muggle_evloop_delete(evloop);

	muggle_linked_list_destroy(&data.order_list, free_order, NULL);

	return 0;
}
