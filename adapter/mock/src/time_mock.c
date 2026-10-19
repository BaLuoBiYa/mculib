/* SPDX-License-Identifier: MIT */

/* clock_gettime() is POSIX; the macro must precede the first system header. */
#define _POSIX_C_SOURCE 200809L

#include "config/backend.h"

#if defined(TIME_BACKEND_HOST_MOCK)

#include <time.h>

#include "common/time.h"

/*
 * Ticks are nanoseconds of the monotonic clock, an arbitrary epoch.
 * time_now_ms() is its millisecond truncation.
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
