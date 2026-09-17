/* SPDX-License-Identifier: MIT */
#ifndef MCULIB_COMMON_RINGBUF_H
#define MCULIB_COMMON_RINGBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Typed ring buffers.
 *
 * RINGBUF_DEFINE() instantiates a ring buffer for one element type, so the
 * same code carries bytes, words or user-defined structures without runtime
 * type information, indirection or dynamic memory: the storage is a member of
 * the generated structure and the capacity is a compile-time constant.
 *
 * Both counters are monotonic and an element index is the counter masked with
 * cap - 1, which works because the capacity is a power of two. The whole
 * capacity is usable, no slot is sacrificed to tell "full" from "empty". The
 * counters are only ever compared as a difference, so they may wrap as long
 * as the capacity stays within half of the counter range; that, and the
 * power-of-two requirement, are checked at compile time.
 *
 * Default model: single producer, single consumer (SPSC), lock-free. With
 * multiple producers or multiple consumers the caller must serialize access
 * with critical_enter() / critical_exit().
 *
 * The producer only advances head and only reads tail; the consumer only
 * advances tail and only reads head, and the two sides never touch the same
 * element. No lock or barrier is needed as long as each side is driven by a
 * single execution context.
 */

/**
 * RINGBUF_DEFINE() - declare a typed ring buffer and its API
 * @name: name of the generated type, also used as the API prefix
 * @type: element type
 * @cap: capacity in elements, a power of two and at least %2
 *
 * Declares `struct @name`, which holds the @cap elements of @type and the two
 * counters, plus a set of static inline functions operating on it. Defining
 * the macro in a header is safe because every generated function is static
 * inline. The macro must be terminated with a semicolon.
 *
 * A capacity below %2, a capacity that is not a power of two, and a capacity
 * beyond half of the counter range are rejected at compile time.
 *
 * The generated API is:
 *
 *   name_init(rb)          initialize the counters, discarding everything
 *   name_reset(rb)         discard all buffered elements
 *   name_push(rb, val)     store one element, returns %false if full
 *   name_pop(rb, val)      fetch one element, returns %false if empty
 *   name_write(rb, in, n)  store up to @n elements, returns how many fit
 *   name_read(rb, out, n)  fetch up to @n elements, returns how many came out
 *   name_used(rb)          number of elements currently stored
 *   name_free(rb)          number of elements that can still be stored
 *   name_is_empty(rb)      %true if nothing can be fetched
 *   name_is_full(rb)       %true if nothing can be stored
 *
 * name_write() and name_read() return a short count when the buffer is full
 * or empty; buffered elements are never overwritten or skipped.
 *
 * Context: name_init(), name_reset(), name_push(), name_write(),
 * name_is_full() and name_free() belong to the producer side, name_pop() and
 * name_read() to the consumer side, name_used() and name_is_empty() may be
 * called from either side. name_init() and name_reset() must not run
 * concurrently with the other side.
 */
#define RINGBUF_DEFINE(name, type, cap)                                            \
	struct name {                                                              \
		type buf[cap];                                                     \
		volatile size_t head;                                              \
		volatile size_t tail;                                              \
	};                                                                         \
                                                                                   \
	static inline void name##_init(struct name *rb)                            \
	{                                                                          \
		rb->head = 0;                                                      \
		rb->tail = 0;                                                      \
	}                                                                          \
                                                                                   \
	static inline void name##_reset(struct name *rb)                           \
	{                                                                          \
		rb->head = rb->tail;                                               \
	}                                                                          \
                                                                                   \
	static inline size_t name##_used(const struct name *rb)                    \
	{                                                                          \
		return (size_t)(rb->head - rb->tail);                              \
	}                                                                          \
                                                                                   \
	static inline size_t name##_free(const struct name *rb)                    \
	{                                                                          \
		return (size_t)(cap) - name##_used(rb);                            \
	}                                                                          \
                                                                                   \
	static inline bool name##_is_empty(const struct name *rb)                  \
	{                                                                          \
		return rb->head == rb->tail;                                       \
	}                                                                          \
                                                                                   \
	static inline bool name##_is_full(const struct name *rb)                   \
	{                                                                          \
		return name##_used(rb) == (size_t)(cap);                           \
	}                                                                          \
                                                                                   \
	static inline bool name##_push(struct name *rb, type val)                  \
	{                                                                          \
		size_t head = rb->head;                                            \
                                                                                   \
		/* A consumer advancing tail only makes this check pessimistic. */ \
		if (head - rb->tail == (size_t)(cap)) {                            \
			return false;                                              \
		}                                                                  \
                                                                                   \
		rb->buf[head & ((cap) - 1)] = val;                                 \
		rb->head = head + 1;                                               \
                                                                                   \
		return true;                                                       \
	}                                                                          \
                                                                                   \
	static inline bool name##_pop(struct name *rb, type *val)                  \
	{                                                                          \
		size_t tail = rb->tail;                                            \
                                                                                   \
		if (rb->head == tail) {                                            \
			return false;                                              \
		}                                                                  \
                                                                                   \
		*val = rb->buf[tail & ((cap) - 1)];                                \
		rb->tail = tail + 1;                                               \
                                                                                   \
		return true;                                                       \
	}                                                                          \
                                                                                   \
	static inline size_t name##_write(struct name *rb, const type *in,         \
					  size_t n)                                \
	{                                                                          \
		size_t head = rb->head;                                            \
		size_t index = head & ((cap) - 1);                                 \
		size_t space = name##_free(rb);                                    \
		size_t first;                                                      \
                                                                                   \
		if (n > space) {                                                   \
			n = space;                                                 \
		}                                                                  \
		if (n == 0) {                                                      \
			return 0;                                                  \
		}                                                                  \
                                                                                   \
		first = (size_t)(cap) - index;                                     \
		if (first > n) {                                                   \
			first = n;                                                 \
		}                                                                  \
                                                                                   \
		memcpy(&rb->buf[index], in, first * sizeof(type));                 \
		if (n > first) {                                                   \
			memcpy(rb->buf, in + first,                                \
			       (n - first) * sizeof(type));                        \
		}                                                                  \
		rb->head = head + n;                                               \
                                                                                   \
		return n;                                                          \
	}                                                                          \
                                                                                   \
	static inline size_t name##_read(struct name *rb, type *out, size_t n)     \
	{                                                                          \
		size_t tail = rb->tail;                                            \
		size_t index = tail & ((cap) - 1);                                 \
		size_t used = name##_used(rb);                                     \
		size_t first;                                                      \
                                                                                   \
		if (n > used) {                                                    \
			n = used;                                                  \
		}                                                                  \
		if (n == 0) {                                                      \
			return 0;                                                  \
		}                                                                  \
                                                                                   \
		first = (size_t)(cap) - index;                                     \
		if (first > n) {                                                   \
			first = n;                                                 \
		}                                                                  \
                                                                                   \
		memcpy(out, &rb->buf[index], first * sizeof(type));                \
		if (n > first) {                                                   \
			memcpy(out + first, rb->buf,                               \
			       (n - first) * sizeof(type));                        \
		}                                                                  \
		rb->tail = tail + n;                                               \
                                                                                   \
		return n;                                                          \
	}                                                                          \
                                                                                   \
	_Static_assert((cap) >= 2 && (((cap) & ((cap) - 1)) == 0) &&               \
			       (cap) <= SIZE_MAX / 2,                              \
		       #name " capacity must be a power of two, 2 or more");

#endif /* MCULIB_COMMON_RINGBUF_H */
