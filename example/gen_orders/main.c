#include "netmodel/net_model_type.h"
#include "netmodel/net_model_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

SHFE_INSTRUMENTS;
DCE_INSTRUMENTS;
CZCE_INSTRUMENTS;
CFFEX_INSTRUMENTS;
INE_INSTRUMENTS;
GFEX_INSTRUMENTS;
SSE_INSTRUMENTS;
SZSE_INSTRUMENTS;

uint64_t *load_numbers(const char *filepath, int *cnt)
{
	uint64_t *arr = NULL;
	FILE *fp = NULL;

	fp = fopen(filepath, "rb");
	if (fp == NULL) {
		fprintf(stderr, "failed read %s, use bin/gen_numbers generate first\n",
				filepath);
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	long num_bytes = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	*cnt = num_bytes / sizeof(uint64_t);
	arr = (uint64_t *)malloc(sizeof(uint64_t) * (*cnt));
	if (arr == NULL) {
		fclose(fp);
		fprintf(stderr, "failed allocate %d of uint64_t\n", *cnt);
		return NULL;
	}

	unsigned long read_cnt = fread(arr, sizeof(uint64_t), *cnt, fp);
	if (read_cnt != (unsigned long)*cnt) {
		fclose(fp);
		free(arr);
		fprintf(stderr, "read n not equal cnt\n");
		return NULL;
	}

	fclose(fp);

	return arr;
}

void gen_market_instrument(order_t *p_order)
{
	const char *instrument = NULL;
	p_order->market = rand() % (MAX_MARKET - 1) + 1;
	switch (p_order->market) {
	case MARKET_SHFE: {
		instrument = SHFE_instruments[rand() % (sizeof(SHFE_instruments) /
												sizeof(SHFE_instruments[0]))];
	} break;
	case MARKET_DCE: {
		instrument = DCE_instruments[rand() % (sizeof(DCE_instruments) /
											   sizeof(DCE_instruments[0]))];
	} break;
	case MARKET_CZCE: {
		instrument = CZCE_instruments[rand() % (sizeof(CZCE_instruments) /
												sizeof(CZCE_instruments[0]))];
	} break;
	case MARKET_CFFEX: {
		instrument = CFFEX_instruments[rand() % (sizeof(CFFEX_instruments) /
												 sizeof(CFFEX_instruments[0]))];
	} break;
	case MARKET_INE: {
		instrument = INE_instruments[rand() % (sizeof(INE_instruments) /
											   sizeof(INE_instruments[0]))];
	} break;
	case MARKET_GFEX: {
		instrument = GFEX_instruments[rand() % (sizeof(GFEX_instruments) /
												sizeof(GFEX_instruments[0]))];
	} break;
	case MARKET_SSE: {
		instrument = SSE_instruments[rand() % (sizeof(SSE_instruments) /
											   sizeof(SSE_instruments[0]))];
	} break;
	case MARKET_SZSE: {
		instrument = SZSE_instruments[rand() % (sizeof(SZSE_instruments) /
												sizeof(SZSE_instruments[0]))];
	} break;
	default: {
		fprintf(stderr, "invalid market: %d\n", p_order->market);
		exit(EXIT_FAILURE);
	} break;
	}
	strncpy(p_order->instrument, instrument, sizeof(p_order->instrument) - 1);
}

void dump_orders(order_t *orders, int total_order)
{
	FILE *fp = NULL;

	fp = fopen("orders.bin", "wb");
	if (fp == NULL) {
		fprintf(stderr, "failed open orders.bin for write\n");
		exit(EXIT_FAILURE);
	}

	unsigned long n = fwrite(orders, sizeof(order_t), total_order, fp);
	if (n != (unsigned long)total_order) {
		fprintf(stderr, "failed write orders\n");
		exit(EXIT_FAILURE);
	}
	fprintf(stdout, "write %d orders\n", total_order);

	fclose(fp);
}

int main(int argc, char *argv[])
{
	uint64_t *arr = NULL;
	int cnt = 0;

	arr = load_numbers("numbers.bin", &cnt);
	if (arr == NULL) {
		fprintf(stderr, "failed load numbers\n");
		exit(EXIT_FAILURE);
	}

	int total_orders = 10000;
	if (argc > 1) {
		total_orders = atoi(argv[0]);
		if (total_orders <= 0) {
			fprintf(stderr, "invalid order number\n");
			exit(EXIT_FAILURE);
		}
	}

	order_t *orders = (order_t *)malloc(sizeof(order_t) * total_orders);
	if (orders == NULL) {
		fprintf(stderr, "failed allocate memory for orders\n");
		exit(EXIT_FAILURE);
	}

	srand(time(NULL));

	size_t cnt_cal_num =
		sizeof(orders[0].cal_num) / sizeof(orders[0].cal_num[0]);
	for (int i = 0; i < total_orders; ++i) {
		memset(&orders[i], 0, sizeof(order_t));
		gen_market_instrument(&orders[i]);
		orders[i].order_action = rand() % (MAX_ORDER_ACTION - 1) + 1;
		orders[i].order_dir = rand() % (MAX_ORDER_DIR - 1) + 1;
		orders[i].price = rand() % 100 + 100;
		orders[i].qty = rand() % 100 + 1;
		for (size_t j = 0; j < cnt_cal_num; ++j) {
			orders[i].cal_num[j] = arr[rand() % cnt];
		}
	}

	dump_orders(orders, total_orders);

	free(orders);
	free(arr);

	return 0;
}
