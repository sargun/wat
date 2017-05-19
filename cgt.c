#include <stdlib.h>
#include <stdio.h>
#include <ck_spinlock.h>
#include <time.h>

#include "rdtscp.h"
#include "util.h"

#define ITERATIONS 5000000
uint64_t times[ITERATIONS];

int main(int argc, char *argv[]) {
	struct timespec t;
	uint64_t t1, t2, total_cycles;
	int i;

	memset(times, 0, sizeof(times));

	t1 = rdtscp();
	for (i = 0; i < ITERATIONS; i++)
		clock_gettime(CLOCK_MONOTONIC, &t);
	t2 = rdtscp();
	total_cycles = t2 - t1;

	t1 = rdtscp();
	for (i = 0; i < ITERATIONS; i++) {
		clock_gettime(CLOCK_MONOTONIC, &t);
		t2 = rdtscp();
		times[i] = t2 - t1;
		t1 = t2;
	}

	sort_uint64_t_array(times, ARRAY_SIZE(times));
	printf("Average cycles per clock_gettime: %lu\n", total_cycles / ITERATIONS);

	printf("Minimum cycles per iteration: %lu\n", times[0]);
	printf("Median cycles per iteration: %lu\n", times[ARRAY_SIZE(times) / 2]);
	printf("95th percentile cycles per iteration: %lu\n", times[P(99, times)]);
}

