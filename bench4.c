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
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>

#include <sys/socket.h>
#include <sys/un.h>

#include "rdtscp.h"
#include "util.h"

#define ITERATIONS	500000
#define RING_SIZE	8

struct payload {
	__u64 val;
};

uint64_t times[ITERATIONS * RING_SIZE];
int do_send(int fd) {
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
		send(fd, &p, sizeof(p), 0);
		recv(fd, &p, sizeof(p), 0);
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
	printf("95th percentile Time: %lu\n", times2[P(95, times)]);

	if (retsum != (sum * 2)) {
		printf("Something broke\n");
		printf("Sum: %llu\n", sum);
		printf("RetSum: %llu\n", retsum);
		return 1;
	}
	return 0;
}

int do_recv(int fd) {
	struct payload p;
	int i;

	for (i = 0; i < (ITERATIONS * RING_SIZE); i++) {
		recv(fd, &p, sizeof(p), 0);
		p.val = p.val * 2;
		send(fd, &p, sizeof(p), 0);
	}
	return 0;
}

#define MODE_SEND 0
#define MODE_RECV 1

int main(int argc, char *argv[]) {
	struct sockaddr_un sockaddr = {
		.sun_family = AF_UNIX,
		.sun_path = "ipc",
	};

	struct rusage usage_start, usage_end;
	int mode;
	int ret;
	int fd, fd2;

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

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd <= 0) {
		perror("socket");
		return 1;
	}


	getrusage(RUSAGE_SELF, &usage_start);
	switch (mode) {
	case MODE_SEND:
		unlink(sockaddr.sun_path);
		if (bind(fd, (const struct sockaddr *)&sockaddr, sizeof(struct sockaddr))) {
			perror("bind");
			goto send_fail;
		}
		if (listen(fd, 1)) {
			perror("listen");
			goto send_fail;
		}
		fd2 = accept(fd, NULL, NULL);
		ret = do_send(fd2);
		break;
	case MODE_RECV:
		if (connect(fd, (const struct sockaddr *)&sockaddr,  sizeof(struct sockaddr))) {
			perror("connect");
			return 1;
		}
		ret = do_recv(fd);
		break;
	}
	getrusage(RUSAGE_SELF, &usage_end);
	printf("Invol Ctx Switches: %ld\nVoluntary Ctx Switches: %ld\n", usage_end.ru_nivcsw - usage_start.ru_nivcsw, usage_end.ru_nvcsw - usage_start.ru_nvcsw);

	return ret;
send_fail:
		unlink(sockaddr.sun_path);
	return 1;
}
