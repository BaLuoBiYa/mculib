/* SPDX-License-Identifier: MIT */
#ifndef MCULIB_COMMON_BYTEORDER_H
#define MCULIB_COMMON_BYTEORDER_H

#include <stdint.h>

/*
 * Byte order conversion.
 *
 * MCULIB_BYTE_ORDER tells the byte order of the target, from the compiler:
 *
 *   #if MCULIB_BYTE_ORDER == MCULIB_BYTE_ORDER_LITTLE
 *
 * The name is prefixed because system headers define BYTE_ORDER, LITTLE_ENDIAN
 * and BIG_ENDIAN.
 *
 * The big-endian variants store the most significant byte first, the
 * little-endian variants the least significant byte first, and put_* writes
 * exactly the width in bytes. A pointer to any byte of a frame is a valid
 * argument, the conversions need no alignment. They do not depend on the macros
 * above.
 */

#define MCULIB_BYTE_ORDER_LITTLE 1
#define MCULIB_BYTE_ORDER_BIG 2

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define MCULIB_BYTE_ORDER MCULIB_BYTE_ORDER_LITTLE
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define MCULIB_BYTE_ORDER MCULIB_BYTE_ORDER_BIG
#else
#error "the byte order of this target is unknown, add a branch above"
#endif

static inline void put_u16be(uint8_t *dst, uint16_t value)
{
	dst[0] = (uint8_t)(value >> 8);
	dst[1] = (uint8_t)value;
}

static inline uint16_t get_u16be(const uint8_t *src)
{
	return (uint16_t)(((uint16_t)src[0] << 8) | src[1]);
}

static inline void put_u16le(uint8_t *dst, uint16_t value)
{
	dst[0] = (uint8_t)value;
	dst[1] = (uint8_t)(value >> 8);
}

static inline uint16_t get_u16le(const uint8_t *src)
{
	return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static inline void put_u32be(uint8_t *dst, uint32_t value)
{
	dst[0] = (uint8_t)(value >> 24);
	dst[1] = (uint8_t)(value >> 16);
	dst[2] = (uint8_t)(value >> 8);
	dst[3] = (uint8_t)value;
}

static inline uint32_t get_u32be(const uint8_t *src)
{
	return ((uint32_t)src[0] << 24) | ((uint32_t)src[1] << 16) |
	       ((uint32_t)src[2] << 8) | (uint32_t)src[3];
}

static inline void put_u32le(uint8_t *dst, uint32_t value)
{
	dst[0] = (uint8_t)value;
	dst[1] = (uint8_t)(value >> 8);
	dst[2] = (uint8_t)(value >> 16);
	dst[3] = (uint8_t)(value >> 24);
}

static inline uint32_t get_u32le(const uint8_t *src)
{
	return (uint32_t)src[0] | ((uint32_t)src[1] << 8) |
	       ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}

static inline void put_u64be(uint8_t *dst, uint64_t value)
{
	dst[0] = (uint8_t)(value >> 56);
	dst[1] = (uint8_t)(value >> 48);
	dst[2] = (uint8_t)(value >> 40);
	dst[3] = (uint8_t)(value >> 32);
	dst[4] = (uint8_t)(value >> 24);
	dst[5] = (uint8_t)(value >> 16);
	dst[6] = (uint8_t)(value >> 8);
	dst[7] = (uint8_t)value;
}

static inline uint64_t get_u64be(const uint8_t *src)
{
	return ((uint64_t)src[0] << 56) | ((uint64_t)src[1] << 48) |
	       ((uint64_t)src[2] << 40) | ((uint64_t)src[3] << 32) |
	       ((uint64_t)src[4] << 24) | ((uint64_t)src[5] << 16) |
	       ((uint64_t)src[6] << 8) | (uint64_t)src[7];
}

static inline void put_u64le(uint8_t *dst, uint64_t value)
{
	dst[0] = (uint8_t)value;
	dst[1] = (uint8_t)(value >> 8);
	dst[2] = (uint8_t)(value >> 16);
	dst[3] = (uint8_t)(value >> 24);
	dst[4] = (uint8_t)(value >> 32);
	dst[5] = (uint8_t)(value >> 40);
	dst[6] = (uint8_t)(value >> 48);
	dst[7] = (uint8_t)(value >> 56);
}

static inline uint64_t get_u64le(const uint8_t *src)
{
	return (uint64_t)src[0] | ((uint64_t)src[1] << 8) |
	       ((uint64_t)src[2] << 16) | ((uint64_t)src[3] << 24) |
	       ((uint64_t)src[4] << 32) | ((uint64_t)src[5] << 40) |
	       ((uint64_t)src[6] << 48) | ((uint64_t)src[7] << 56);
}

#endif /* MCULIB_COMMON_BYTEORDER_H */
