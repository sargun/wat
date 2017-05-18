#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <sched.h>
#include <sys/mman.h>


#define ITERATIONS 500000

int main(int argc, char *argv[]) {
	struct timespec start, end;
	long long total_time;
	int i;

	clock_gettime(CLOCK_MONOTONIC, &start);

	for (i = 0; i < ITERATIONS; i++) {
		sched_yield();
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	total_time = 1000000000 * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
	printf("NS per iteration: %lld\n", total_time / ITERATIONS);
}

