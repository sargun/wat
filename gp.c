#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <inttypes.h>
#include "rdtscp.h"
#include "util.h"

#define ITERATIONS 500000
uint64_t times[ITERATIONS];


int main(int argc, char *argv[]) {
	uint64_t t1, t2, total_cycles;
	int i;

	memset(times, 0, sizeof(times));

	t1 = rdtscp();
	for (i = 0; i < ITERATIONS; i++) {
		syscall(SYS_getpid);
	}
	t2 = rdtscp();
	total_cycles = t2 - t1;

	t1 = rdtscp();
	for (i = 0; i < ITERATIONS; i++) {
		syscall(SYS_getpid);
		t2 = rdtscp();
		times[i] = t2 - t1;
		t1 = t2;
	}

	sort_uint64_t_array(times, ARRAY_SIZE(times));
	printf("Average cycles per getpid: %lu\n", total_cycles / ITERATIONS);

	printf("Minimum cycles per iteration: %lu\n", times[0]);
	printf("Median cycles per iteration: %lu\n", times[ARRAY_SIZE(times) / 2]);
	printf("95th percentile cycles per iteration: %lu\n", times[P(95, times)]);
}

