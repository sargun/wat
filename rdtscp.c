#include <sched.h>
#include <sys/mman.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <signal.h>
#include <ck_spinlock.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>


#define ITERATIONS 500000

static inline uint64_t rdtscp(void) {
  uint32_t lo, hi;
  __asm__ volatile ("rdtscp"
      : /* outputs */ "=a" (lo), "=d" (hi)
      : /* no inputs */
      : /* clobbers */ "%rcx");
  return (uint64_t)lo | (((uint64_t)hi) << 32);
}


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
	printf("Time per iteration: %lld\n", total_time / ITERATIONS);
	printf("Total Time: %lld\n", total_time);
	printf("Total ticks: %lu\n", t2 - t1);
}

