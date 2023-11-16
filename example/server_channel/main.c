#include "server_context.h"
#include "worker_handle.h"
#include "upstream_handle.h"
#include "listen_handle.h"
#include "haclog/haclog.h"

typedef struct worker_thread_args {
	muggle_event_loop_t *evloop;
	server_context_t *server_ctx;
} worker_thread_args_t;

enum {
	OPT_VAL_LONG_ONLY = 1000,
	OPT_VAL_UPSTREAM_HOST,
	OPT_VAL_UPSTREAM_PORT,
};

bool parse_args(int argc, char **argv, server_config_t *cfg)
{
	int c;

	memset(cfg, 0, sizeof(*cfg));
	strncpy(cfg->host, "127.0.0.1", sizeof(cfg->host) - 1);
	strncpy(cfg->port, "10102", sizeof(cfg->port) - 1);
	strncpy(cfg->upstream_host, "127.0.0.1", sizeof(cfg->upstream_host) - 1);
	strncpy(cfg->upstream_port, "16666", sizeof(cfg->upstream_port) - 1);
	cfg->num_worker = 2;
	cfg->is_performance = 1;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "help", no_argument, NULL, 'h' },
			{ "host", required_argument, NULL, 'H' },
			{ "port", required_argument, NULL, 'P' },
			{ "upstream-host", required_argument, NULL, OPT_VAL_UPSTREAM_HOST },
			{ "upstream-port", required_argument, NULL, OPT_VAL_UPSTREAM_PORT },
			{ "worker", required_argument, NULL, 'w' },
			{ "cpu-freq", required_argument, NULL, 'c' },
			{ NULL, 0, NULL, 0 }
		};

		c = getopt_long(argc, argv, "hH:P:w:c:", long_options, &option_index);
		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h': {
			fprintf(stdout,
					"Usage: %s <options>\n"
					"    -H, --host           server host\n"
					"    -P, --port           server port\n"
					"      , --upstream-host  upstream host\n"
					"      , --upstream-port  upstream port\n"
					"    -w, --worker         number of worker\n"
					"    -c, --cpu-freq       cpu frequency set\n"
					"        * performance: maximum performance\n"
					"        * powersave: energy saving\n"
					"",
					argv[0]);
			exit(EXIT_SUCCESS);
		} break;
		case 'H': {
			strncpy(cfg->host, optarg, sizeof(cfg->host) - 1);
		} break;
		case 'P': {
			strncpy(cfg->port, optarg, sizeof(cfg->port) - 1);
		} break;
		case OPT_VAL_UPSTREAM_HOST: {
			strncpy(cfg->upstream_host, optarg, sizeof(cfg->upstream_host) - 1);
		} break;
		case OPT_VAL_UPSTREAM_PORT: {
			strncpy(cfg->upstream_port, optarg, sizeof(cfg->upstream_port) - 1);
		} break;
		case 'w': {
			unsigned long ul;
			if (muggle_str_toul(optarg, &ul, 10) == 0) {
				LOG_ERROR("invalid 'user' value: %s", optarg);
				return false;
			}
			cfg->num_worker = ul;
		} break;
		case 'c': {
			if (strcmp(optarg, "performance") == 0) {
				cfg->is_performance = 1;
			} else if (strcmp(optarg, "powersave") == 0) {
				cfg->is_performance = 0;
			} else {
				LOG_ERROR("invalid 'cpu-freq' value: %s", optarg);
				return false;
			}
		} break;
		}
	}

	fprintf(stdout,
			"Launch Server-multiplexing\n"
			"----------------\n"
			"host: %s\n"
			"port: %s\n"
			"upstream-host: %s\n"
			"upstream-port: %s\n"
			"worker: %u\n"
			"cpu-freq: %s\n"
			"----------------\n"
			"",
			cfg->host, cfg->port, cfg->upstream_host, cfg->upstream_port,
			cfg->num_worker, cfg->is_performance ? "performance" : "powersave");

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

static muggle_thread_ret_t worker_routine(void *p_args)
{
	worker_thread_args_t *thread_args = (worker_thread_args_t *)p_args;
	muggle_event_loop_t *evloop = thread_args->evloop;
	server_context_t *server_ctx = thread_args->server_ctx;
	server_config_t *cfg = server_ctx->cfg;
	free(thread_args);

	// prepare handle
	muggle_socket_evloop_handle_t handle;
	muggle_socket_evloop_handle_init(&handle);
	if (cfg->is_performance) {
		muggle_socket_evloop_handle_set_timer_interval(&handle, 0);
	}
	muggle_socket_evloop_handle_set_cb_add_ctx(&handle, on_worker_add_ctx);
	muggle_socket_evloop_handle_set_cb_msg(&handle, on_worker_message);
	muggle_socket_evloop_handle_set_cb_close(&handle, on_worker_close);
	muggle_socket_evloop_handle_set_cb_release(&handle, on_worker_release);

	muggle_socket_evloop_handle_attach(&handle, evloop);

	// set data
	worker_evloop_data_t *data =
		(worker_evloop_data_t *)malloc(sizeof(worker_evloop_data_t));
	memset(data, 0, sizeof(*data));
	data->evloop = evloop;
	data->server_ctx = server_ctx;
	muggle_evloop_set_data(evloop, data);

	muggle_evloop_run(evloop);

	muggle_socket_evloop_handle_destroy(&handle);
	muggle_evloop_delete(evloop);

	return 0;
}

muggle_event_loop_t **run_workers(server_context_t *server_ctx)
{
	server_config_t *cfg = server_ctx->cfg;

	muggle_event_loop_t **workers = (muggle_event_loop_t **)malloc(
		sizeof(muggle_event_loop_t *) * cfg->num_worker);
	for (uint32_t i = 0; i < cfg->num_worker; i++) {
		workers[i] = create_evloop();

		worker_thread_args_t *thread_args =
			(worker_thread_args_t *)malloc(sizeof(worker_thread_args_t));
		thread_args->evloop = workers[i];
		thread_args->server_ctx = server_ctx;

		muggle_thread_t th;
		muggle_thread_create(&th, worker_routine, thread_args);
		muggle_thread_detach(&th);
	}

	return workers;
}

static muggle_thread_ret_t upstream_routine(void *p_args)
{
	worker_thread_args_t *thread_args = (worker_thread_args_t *)p_args;
	muggle_event_loop_t *evloop = thread_args->evloop;
	server_context_t *server_ctx = thread_args->server_ctx;
	server_config_t *cfg = server_ctx->cfg;
	free(thread_args);

	// prepare handle
	muggle_socket_evloop_handle_t handle;
	muggle_socket_evloop_handle_init(&handle);
	// NOTE: upstream evloop don't need performance
	// muggle_socket_evloop_handle_set_timer_interval(&handle, 0);
	muggle_socket_evloop_handle_set_cb_add_ctx(&handle, on_upstream_add_ctx);
	muggle_socket_evloop_handle_set_cb_msg(&handle, on_upstream_message);
	muggle_socket_evloop_handle_set_cb_close(&handle, on_upstream_close);
	muggle_socket_evloop_handle_set_cb_release(&handle, on_upstream_release);

	muggle_socket_evloop_handle_attach(&handle, evloop);

	// set data
	muggle_evloop_set_data(evloop, server_ctx);

	muggle_socket_context_t *ctx = connect_upstream(cfg);
	muggle_socket_evloop_add_ctx(evloop, ctx);

	muggle_evloop_run(evloop);

	muggle_socket_evloop_handle_destroy(&handle);
	muggle_evloop_delete(evloop);

	return 0;
}

muggle_event_loop_t *run_upstream(server_context_t *server_ctx)
{
	muggle_event_loop_t *upstream_evloop = create_evloop();

	worker_thread_args_t *thread_args =
		(worker_thread_args_t *)malloc(sizeof(worker_thread_args_t));
	thread_args->evloop = upstream_evloop;
	thread_args->server_ctx = server_ctx;

	muggle_thread_t upstream_chan;
	muggle_thread_create(&upstream_chan, upstream_routine, thread_args);
	muggle_thread_detach(&upstream_chan);

	return upstream_evloop;
}

static muggle_thread_ret_t tcp_listen_routine(void *p_args)
{
	evloop_listen_data_t *args = (evloop_listen_data_t *)p_args;
	server_context_t *server_ctx = args->server_ctx;
	server_config_t *cfg = server_ctx->cfg;

	muggle_socket_t fd = MUGGLE_INVALID_SOCKET;
	do {
		LOG_INFO("try tcp listen: %s %s", cfg->host, cfg->port);
		fd = muggle_tcp_listen(cfg->host, cfg->port, 512);
		if (fd == MUGGLE_INVALID_SOCKET) {
			// LOG_SYS_ERR(LOG_LEVEL_ERROR, "failed listen");
			LOG_ERROR("failed listen: %s %s", cfg->host, cfg->port);
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

void run_tcp_listen(evloop_listen_data_t *args)
{
	muggle_thread_t th;
	muggle_thread_create(&th, tcp_listen_routine, args);
	muggle_thread_detach(&th);
}

void init_log()
{
	static haclog_file_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	if (haclog_file_handler_init(&handler, "logs/server.log", "a") != 0) {
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
	server_config_t cfg;
	if (!parse_args(argc, argv, &cfg)) {
		LOG_ERROR("failed parse arguments");
		exit(EXIT_FAILURE);
	}

	// check config
	if (cfg.num_worker < 1) {
		LOG_ERROR("invalid 'worker' value: %u", cfg.num_worker);
		exit(EXIT_FAILURE);
	}

	// prepare channels
	int upstream_chan_flags = MUGGLE_CHANNEL_FLAG_WRITE_SPIN;
	if (cfg.is_performance) {
		upstream_chan_flags |= MUGGLE_CHANNEL_FLAG_READ_BUSY;
	}
	muggle_channel_t upstream_chan;
	muggle_channel_init(&upstream_chan, 2048, upstream_chan_flags);

	int client_chan_flags = MUGGLE_CHANNEL_FLAG_WRITE_SPIN;
	muggle_channel_t client_chan;
	muggle_channel_init(&client_chan, 2048, client_chan_flags);

	// init server context
	server_context_t server_ctx;
	server_context_init(&server_ctx, &cfg, &upstream_chan, &client_chan);
	server_context_run(&server_ctx);

	// run workers
	muggle_event_loop_t **workers = run_workers(&server_ctx);

	// run upstream
	run_upstream(&server_ctx);

	// init event loop
	muggle_event_loop_t *evloop = create_evloop();

	muggle_socket_evloop_handle_t handle;
	muggle_socket_evloop_handle_init(&handle);
	muggle_socket_evloop_handle_set_cb_conn(&handle, on_listen_connect);
	muggle_socket_evloop_handle_set_cb_msg(&handle, on_listen_message);
	muggle_socket_evloop_handle_set_cb_close(&handle, on_listen_close);
	muggle_socket_evloop_handle_set_cb_release(&handle, on_listen_release);
	muggle_socket_evloop_handle_attach(&handle, evloop);
	LOG_INFO("socket handle attached to event loop");

	// evloop data
	evloop_listen_data_t data;
	memset(&data, 0, sizeof(data));
	data.evloop = evloop;
	data.server_ctx = &server_ctx;
	data.num_worker = cfg.num_worker;
	data.workers = workers;

	muggle_evloop_set_data(evloop, &data);

	// run listen
	run_tcp_listen(&data);

	// run evloop
	muggle_evloop_run(evloop);

	// cleanup
	muggle_socket_evloop_handle_destroy(&handle);
	muggle_evloop_delete(evloop);

	return 0;
}
