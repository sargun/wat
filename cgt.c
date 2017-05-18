#include <stdlib.h>
#include <stdio.h>
#include <ck_spinlock.h>
#include <time.h>

#include "rdtscp.h"

#define ITERATIONS 5000000

int main(int argc, char *argv[]) {
	struct timespec t;
	uint64_t t1, t2;
	long long total_time;
	int i;


	t1 = rdtscp();
	for (i = 0; i < ITERATIONS; i++) {
		clock_gettime(CLOCK_MONOTONIC, &t);
	}
	t2 = rdtscp();

	printf("Cycles per clock_gettime: %lld\n", (t2 - t1) / ITERATIONS);
}

