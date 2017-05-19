#include <stdlib.h>
#include <stdio.h>
#include <ck_spinlock.h>
#include <time.h>

#include "rdtscp.h"

#define ITERATIONS 50000000

int main(int argc, char *argv[]) {
	struct timespec start, end;
	uint64_t t1, t2;
	long long total_time;
	int i;

	clock_gettime(CLOCK_MONOTONIC, &start);

	t1 = rdtscp();
	for (i = 0; i < ITERATIONS; i++) {
		rdtscp();
	}
	t2 = rdtscp();

	clock_gettime(CLOCK_MONOTONIC, &end);
	total_time = 1000000000 * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
	printf("Nanoseconds per rdtscp: %lld\n", total_time / (ITERATIONS + 2));
	printf("Total Nanoseconds: %lld\n", total_time);
	printf("Total Counter increases: %lu\n", t2 - t1);
	printf("Nanoseconds per cycle: %f\n", (double)total_time / (double)(t2 - t1));
	printf("Cycles per nanosecond: %f\n", (double)(t2 - t1) / (double)total_time);
}

