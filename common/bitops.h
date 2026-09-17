#ifndef MCULIB_COMMON_BITOPS_H
#define MCULIB_COMMON_BITOPS_H

#include <stddef.h>
#include <stdint.h>

#define BIT(n) (1UL << (n))
#define BIT_MASK(n) (BIT(n) - 1u)

#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))

static inline int is_power_of_two(size_t x)
{
	return x != 0 && (x & (x - 1)) == 0;
}

static inline uint32_t bit_find_first(uint32_t x)
{
#if defined(__GNUC__) || defined(__clang__)
	return x ? (uint32_t)__builtin_ctz(x) : 32u;
#else
	uint32_t n = 0;
	if (x == 0)
		return 32u;
	while ((x & 1u) == 0) {
		x >>= 1;
		n++;
	}
	return n;
#endif
}

static inline uint32_t bit_find_last(uint32_t x)
{
#if defined(__GNUC__) || defined(__clang__)
	return x ? (uint32_t)(31 - __builtin_clz(x)) : 32u;
#else
	uint32_t n = 0;
	if (x == 0)
		return 32u;
	while (x >>= 1)
		n++;
	return n;
#endif
}

#endif