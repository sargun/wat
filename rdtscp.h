#ifndef _RDTSCP_H
#define _RDTSCP_H

static inline uint64_t rdtscp(void) {
	uint32_t lo, hi;
	__asm__ volatile ("rdtscp"
		: /* outputs */ "=a" (lo), "=d" (hi)
		: /* no inputs */
		: /* clobbers */ "%rcx");
	return (uint64_t)lo | (((uint64_t)hi) << 32);
}

#endif
