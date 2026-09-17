/* SPDX-License-Identifier: MIT */
#ifndef MCULIB_COMMON_RINGBUF_H
#define MCULIB_COMMON_RINGBUF_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Ring buffer.
 *
 * Default model: single producer, single consumer (SPSC), lock-free. With
 * multiple producers or multiple consumers the caller must serialize access
 * with critical_enter() / critical_exit().
 *
 * The storage is provided by the caller and is statically allocated; no
 * dynamic memory is used.
 */

/**
 * struct ringbuf - single-producer/single-consumer ring buffer
 * @buf: statically allocated storage
 * @size: storage size in bytes
 * @head: write pointer, modified by the producer only
 * @tail: read pointer, modified by the consumer only
 */
struct ringbuf {
	uint8_t *buf;
	size_t size;
	volatile size_t head;
	volatile size_t tail;
};

/**
 * ringbuf_init() - associate storage with a ring buffer
 * @rb: ring buffer
 * @buf: statically allocated storage
 * @size: storage size in bytes
 */
void ringbuf_init(struct ringbuf *rb, uint8_t *buf, size_t size);

/**
 * ringbuf_reset() - discard all buffered data
 * @rb: ring buffer
 *
 * Context: must not run concurrently with the other side of the buffer.
 */
void ringbuf_reset(struct ringbuf *rb);

/**
 * ringbuf_write() - write as much data as fits into the buffer
 * @rb: ring buffer
 * @data: bytes to write
 * @len: number of bytes to write
 *
 * Context: producer context.
 *
 * Return: number of bytes written; may be less than @len when the buffer is
 * full.
 */
size_t ringbuf_write(struct ringbuf *rb, const uint8_t *data, size_t len);

/**
 * ringbuf_read() - read up to the requested number of bytes
 * @rb: ring buffer
 * @data: buffer to read into
 * @len: maximum number of bytes to read
 *
 * Context: consumer context.
 *
 * Return: number of bytes read; may be less than @len when the buffer is
 * empty.
 */
size_t ringbuf_read(struct ringbuf *rb, uint8_t *data, size_t len);

/**
 * ringbuf_used() - number of bytes currently stored
 * @rb: ring buffer
 *
 * Return: number of bytes readable from the buffer.
 */
size_t ringbuf_used(const struct ringbuf *rb);

/**
 * ringbuf_free() - number of bytes that can still be written
 * @rb: ring buffer
 *
 * Return: number of free bytes in the buffer.
 */
size_t ringbuf_free(const struct ringbuf *rb);

/**
 * ringbuf_is_empty() - test whether the buffer holds no data
 * @rb: ring buffer
 *
 * Return: %true if no byte can be read.
 */
bool ringbuf_is_empty(const struct ringbuf *rb);

/**
 * ringbuf_is_full() - test whether the buffer has no free space
 * @rb: ring buffer
 *
 * Return: %true if no byte can be written.
 */
bool ringbuf_is_full(const struct ringbuf *rb);

#endif /* MCULIB_COMMON_RINGBUF_H */