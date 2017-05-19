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
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/syscall.h>

#include "rdtscp.h"
#include "util.h"

#define ITERATIONS	500000
#define RING_SIZE	8

struct payload {
	long val; // mtype
};


uint64_t times[ITERATIONS * RING_SIZE];
int do_send(int in, int out) {
	uint64_t start, end;
	int numbers[RING_SIZE];
	long long sum = 0, retsum = 0;
	struct payload p;
	long long i, x;

	// Zero-out all the memory
	memset(times, 0, sizeof(times));

	for (i = 0; i < RING_SIZE; i++) {
		x = rand() % 100;
		sum = sum + x;
		numbers[i] = x;
	}
	sum = sum * ITERATIONS;

	start = rdtscp();
	for (i = 0; i < (ITERATIONS * RING_SIZE); i++) {
		p.val = numbers[i % RING_SIZE];
		syscall(SYS_msgsnd, out, &p, sizeof(p) - sizeof(long), 0);
		syscall(SYS_msgrcv, in, &p, sizeof(p) - sizeof(long), 0, 0);
		retsum = retsum + p.val;

		end = rdtscp();
		times[i] = end - start;
		start = end;
	}

	/* Ignore the first sample */
	uint64_t *times2 = &times[1];
	sort_uint64_t_array(times2, ARRAY_SIZE(times) - 1);
	printf("Median Iteration Time: %lu\n", times[(ARRAY_SIZE(times) - 1)/2]);
	printf("Min Time: %lu\n", times2[0]);
	printf("Max Time: %lu\n", times2[(ARRAY_SIZE(times) - 2)]);

	if (retsum != (sum * 2)) {
		printf("Something broke\n");
		printf("Sum: %llu\n", sum);
		printf("RetSum: %llu\n", retsum);
		return 1;
	}
	return 0;
}

int do_recv(int in, int out) {
	struct payload p;
	int i;

	for (i = 0; i < (ITERATIONS * RING_SIZE); i++) {
		syscall(SYS_msgrcv, in, &p, sizeof(p) - sizeof(long), 0, 0);
		p.val = p.val * 2;
		syscall(SYS_msgsnd, out,  &p, sizeof(p) - sizeof(long), 0);
	}
	return 0;
}

#define MODE_SEND 0
#define MODE_RECV 1

int main(int argc, char *argv[]) {
	struct rusage usage_start, usage_end;
	key_t ingress, egress;
	int in, out;
	int mode;
	int ret;

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

	ingress = ftok(".", 'i');
	egress = ftok(".", 'o');

	if (mode == MODE_RECV)
		goto work;

	ret = msgctl(ingress, IPC_RMID, 0);
	if (ret && errno != EINVAL) {
		perror("unlink ingress");
		return ret;
	}
	ret = msgctl(egress, IPC_RMID, 0);
	if (ret && errno != EINVAL) {
		perror("unlink egress");
		return ret;
	}

	work:
	in = msgget(ingress, IPC_CREAT | 0777);
	if (in <= 0) {
		perror("msgget in");
		return 1;
	}
	out = msgget(egress, IPC_CREAT | 0777);
	if (out <= 0) {
		perror("msgget in");
		return 1;
	}
	getrusage(RUSAGE_SELF, &usage_start);
	switch (mode) {
	case MODE_SEND:
		ret = do_send(in, out);
		break;
	case MODE_RECV:
		ret = do_recv(out, in);
		break;
	}
	getrusage(RUSAGE_SELF, &usage_end);
	printf("Invol Ctx Switches: %ld\nVoluntary Ctx Switches: %ld\n", usage_end.ru_nivcsw - usage_start.ru_nivcsw, usage_end.ru_nvcsw - usage_start.ru_nvcsw);

	return ret;
}
