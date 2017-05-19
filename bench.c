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

#include "rdtscp.h"
#include "util.h"

#define RING_SIZE 8
#define ITERATIONS 5000000


struct payload {
	ck_spinlock_fas_t spinlock;
	__u8 val;
} __attribute__((aligned(8)));

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

uint64_t times[RING_SIZE * ITERATIONS];
int do_send(struct shm_mem *ptr) {
	struct payload *snd_slot, *rcv_slot;
	uint64_t start, end;
	int numbers[RING_SIZE];
	long long sum = 0, retsum = 0;
	long long i, x;

	// Zero-out all the memory
	memset(ptr, 0, sizeof(struct shm_mem));
	memset(times, 0, sizeof(times));

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

	start = rdtscp();
	for (i = 0; i < (ITERATIONS * RING_SIZE); i++) {
		snd_slot = &ptr->payloads[(i % RING_SIZE) * 2];
		rcv_slot = &ptr->payloads[((i % RING_SIZE) * 2) + 1];
		snd_slot->val = numbers[i % RING_SIZE];
		// Concurrency kit should insert a memory barrier here for us
		ck_spinlock_fas_unlock(&snd_slot->spinlock);

		// Now do the receive
		ck_spinlock_fas_lock(&rcv_slot->spinlock);
		retsum = retsum + rcv_slot->val;
		end = rdtscp();
		times[i] = end - start;
		start = end;
	}

	/* Ignore the first sample */
	uint64_t *times2 = &times[1];
	sort_uint64_t_array(times2, ARRAY_SIZE(times) - 1);
	printf("Median Iteration Time: %lu\n", times[(ARRAY_SIZE(times) - 1)/2]);
	printf("Min Time: %lu\n", times2[0]);
	printf("95th percentile Time: %lu\n", times2[P(95, times)]);

	if (retsum != (sum * 2)) {
		printf("Something broke\n");
		printf("Sum: %llu\n", sum);
		printf("RetSum: %llu\n", retsum);
		return 1;
	}
	return 0;
}

int do_recv(struct shm_mem *ptr) {
	struct payload *snd_slot, *rcv_slot;
	int i;

	for (i = 0; i < (ITERATIONS * RING_SIZE); i++) {
		snd_slot = &ptr->payloads[(i % RING_SIZE) * 2];
		rcv_slot = &ptr->payloads[((i % RING_SIZE) * 2) + 1];
		// Wait for the "message"
		ck_spinlock_fas_lock(&snd_slot->spinlock);
		rcv_slot->val = snd_slot->val * 2;
		// Now do the receive
		ck_spinlock_fas_unlock(&rcv_slot->spinlock);
	}
	return 0;
}

#define MODE_SEND 0
#define MODE_RECV 1

int main(int argc, char *argv[]) {
	struct rusage usage_start, usage_end;
	struct shm_mem *ptr;
	int mode;
	int ret;
	int fd;

	memset(times, 0, sizeof(times));
	if (argc != 2) {
		printf("Mode?\n");
		return 1;
	}
	switch (argv[1][0]) {
		case 's':
			mode = MODE_SEND;
			break;
		case 'r':
			mode = MODE_RECV;
			break;
		default:
			printf("Wat?\n");
			return 1;
	}


	fd = setup_fd("bench");
	if (mode == MODE_SEND) {
		ret = ftruncate(fd, SHM_SIZE);
		if (ret == -1) {
			perror("ftruncate");
			return -1;
		}
	}

	ptr = (struct shm_mem*) mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return 1;
	}
	getrusage(RUSAGE_SELF, &usage_start);
	switch (mode) {
	case MODE_SEND:
		ret = do_send(ptr);
		break;
	case MODE_RECV:
		ret = do_recv(ptr);
		break;
	}
	getrusage(RUSAGE_SELF, &usage_end);
	printf("Invol Ctx Switches: %ld\nVoluntary Ctx Switches: %ld\n", usage_end.ru_nivcsw - usage_start.ru_nivcsw, usage_end.ru_nvcsw - usage_start.ru_nvcsw);

	return ret;
}
