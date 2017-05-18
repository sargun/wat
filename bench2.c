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

#include "util.h"
#include "rdtscp.h"

#define RING_SIZE 1024
#define ITERATIONS 5000

struct payload {
	ck_spinlock_fas_t spinlock;
	__u8 val;
} __attribute__((aligned(64)));

struct shm_mem {
	// Even entries are used to send, odds are used to reply
	struct payload	payloads[RING_SIZE * 2];
} __attribute__((aligned(64)));

#define SHM_SIZE sizeof(struct shm_mem)

int setup_fd(const char *name) {
	int fd;

	fd = shm_open(name, O_RDWR|O_CREAT, 0777);
	if (fd == -1) {
		perror("shm_open");
		return -1;
	}

	return fd;
}

uint64_t times[RING_SIZE * ITERATIONS] = {};

void bench2(struct shm_mem *ptr) {
	struct rusage usage_start, usage_end;
	struct payload *snd_slot, *rcv_slot;
	uint64_t start, end;
	int numbers[RING_SIZE];
	long long sum = 0, retsum = 0;
	long long total_cycles = 0;
	int i, x;

	/* Zero-out all the memory to avoid page faults */
	memset(ptr, 0, sizeof(struct shm_mem));

	for (i = 0; i < RING_SIZE * 2; i++) {
		ck_spinlock_fas_init(&ptr->payloads[i].spinlock);
		ck_spinlock_fas_lock(&ptr->payloads[i].spinlock);
	}

	for (i = 0; i < RING_SIZE; i++) {
		x = rand() % 100;
		sum = sum + x;
		numbers[i] = x;
	}
	sum = sum * ITERATIONS;

	assert(!getrusage(RUSAGE_SELF, &usage_start));
	start = rdtscp();
	for (i = 0; i < (ITERATIONS * RING_SIZE); i++) {
		snd_slot = &ptr->payloads[(i % RING_SIZE) * 2];
		rcv_slot = &ptr->payloads[((i % RING_SIZE) * 2) + 1];
		snd_slot->val = numbers[i % RING_SIZE];
		ck_spinlock_fas_unlock(&snd_slot->spinlock);

		/* Receive Side */
		ck_spinlock_fas_lock(&snd_slot->spinlock);
		rcv_slot->val = snd_slot->val * 2;
		ck_spinlock_fas_unlock(&rcv_slot->spinlock);
		/* Receive Side */

		ck_spinlock_fas_lock(&rcv_slot->spinlock);
		retsum = retsum + rcv_slot->val;

		/* Avoid calling rdtscp twice, so just move the value over */
		end = rdtscp();
		times[i] = end - start;
		start = end;
	}
	assert(!getrusage(RUSAGE_SELF, &usage_end));

	sort_uint64_t_array(times, ARRAY_SIZE(times));

	for (int i = 0; i < ARRAY_SIZE(times); i++)
		total_cycles = total_cycles + times[i];

	printf("Average cycles: %llu\n", total_cycles/ARRAY_SIZE(times));
	printf("Median Iteration Cycles: %lu\n", times[ARRAY_SIZE(times)/2]);
	printf("Min Cycles: %lu\n", times[0]);
	printf("Invol Ctx Switches: %ld\nVoluntary Ctx Switches: %ld\n", usage_end.ru_nivcsw - usage_start.ru_nivcsw, usage_end.ru_nvcsw - usage_start.ru_nvcsw);
	if (retsum != (sum * 2)) {
		printf("Something broke\n");
		printf("Sum: %llu\n", sum);
		printf("RetSum: %llu\n", retsum);
	}
}

int main(int argc, char *argv[]) {
	struct shm_mem *ptr;

	ptr = (struct shm_mem*) malloc(SHM_SIZE);
	if (!ptr) {
		perror("malloc");
		return 1;
	}

	bench2(ptr);

	return 0;
}
