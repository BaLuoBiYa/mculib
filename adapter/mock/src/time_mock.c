/* SPDX-License-Identifier: MIT */

/*
 * clock_gettime() is POSIX and strict C11 mode hides it, so the feature macro
 * is defined before the first system header.
 */
#define _POSIX_C_SOURCE 200809L

#include "config/backend.h"

#if defined(TIME_BACKEND_HOST_MOCK)

#include <time.h>

#include "common/time.h"

/*
 * Ticks are nanoseconds of the monotonic clock, an arbitrary epoch that never
 * goes backwards. time_now_ms() is its truncation, so both always agree.
 * There is no binding step, so this backend has no header of its own.
 */

uint64_t time_now_ticks(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ((uint64_t)ts.tv_sec * 1000000000ull) + (uint64_t)ts.tv_nsec;
}

uint32_t time_now_ms(void)
{
	return (uint32_t)(time_now_ticks() / 1000000ull);
}

#endif /* TIME_BACKEND_HOST_MOCK */
