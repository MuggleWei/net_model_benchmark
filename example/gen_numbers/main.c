#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/**
 * @brief detect n is prime
 *
 * @param n    number of detected
 * @param cnt  count of calculate
 *
 * @return  boolean value
 */
bool is_prime(uint64_t n, uint64_t *cnt)
{
	if (n <= 3) {
		return n > 1;
	}

	if (n % 6 != 1 && n % 6 != 5) {
		return false;
	}

	uint64_t sqrt_val = (uint64_t)sqrt((double)n);
	for (uint64_t i = 5; i < sqrt_val; i += 6) {
		++(*cnt);
		if ((n % i == 0) || (n % (i + 2) == 0)) {
			return false;
		}
	}

	return true;
}

typedef struct num_cal_cnt {
	uint64_t n;
	uint64_t cnt;
	bool is_prime;
} num_cal_cnt_t;

int cmp_num_cal_cnt(const void *a, const void *b)
{
	const num_cal_cnt_t *p1 = (const num_cal_cnt_t *)a;
	const num_cal_cnt_t *p2 = (const num_cal_cnt_t *)b;

	if (p1->cnt > p2->cnt) {
		return -1;
	}
	if (p1->cnt < p2->cnt) {
		return 1;
	}
	return 0;
}

int main()
{
	uint64_t start = 100000000000;
	uint64_t num = 1000000;
	uint64_t accept_cal_cnt = 0;

	num_cal_cnt_t *arr = NULL;
	FILE *fp = NULL;
	char buf[128];

	arr = (num_cal_cnt_t *)malloc(sizeof(num_cal_cnt_t) * num);
	for (uint64_t i = 0; i < num; ++i) {
		arr[i].n = start + i;
		arr[i].is_prime = is_prime(arr[i].n, &arr[i].cnt);
	}

	qsort(arr, num, sizeof(num_cal_cnt_t), cmp_num_cal_cnt);

	fp = fopen("prime_cal_cnt.txt", "wb");
	fprintf(stdout, "generate prime_cal_cnt.txt\n");
	for (uint64_t i = 0; i < num; ++i) {
		int n = snprintf(buf, sizeof(buf), "%llu,%llu,%s\n",
						 (unsigned long long)arr[i].n,
						 (unsigned long long)arr[i].cnt,
						 arr[i].is_prime ? "yes" : "no");
		fwrite(buf, 1, n, fp);
	}
	fclose(fp);

	fp = fopen("numbers.bin", "wb");
	fprintf(stdout, "generate numbers.bin\n");
	accept_cal_cnt = arr[0].cnt - 2000;
	for (uint64_t i = 0; i < num; ++i) {
		if (arr[i].cnt >= accept_cal_cnt) {
			fwrite(&arr[i].n, 1, sizeof(arr[0].n), fp);
		} else {
			fprintf(stdout, "total write %llu numbers\n",
					(unsigned long long)i);
			break;
		}
	}
	fclose(fp);

	free(arr);

	return 0;
}
