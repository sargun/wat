/* Every great project has to either have a util.h, or a misc.h */

#ifndef _UTIL_H
#define _UTIL_H 1
#include <assert.h>
#include <stdlib.h>
#include <ck_spinlock.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define P(percentile, arr)	((ARRAY_SIZE(arr)/20)*19)

#define likely(x)	__builtin_expect((x),1)
#define unlikely(expr)	__builtin_expect(!!(expr), 0)

#define compare_fn(TYPE) 				\
int compare_fn_##TYPE (const void *a, const void *b) { 	\
	TYPE val1 = *((TYPE*)a); 			\
	TYPE val2 = *((TYPE*)b); 			\
	if (val1 > val2) return 1;			\
	if (val1 < val2) return -1;			\
	else return 0;					\
}


#define sort_array(TYPE) 				\
compare_fn(TYPE) 					\
static inline void					\
sort_##TYPE##_array(TYPE arr[], int length) {		\
	qsort(arr, length,				\
	      sizeof(TYPE), compare_fn_##TYPE); 	\
}

sort_array(uint64_t)

#endif
