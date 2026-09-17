#ifndef MCULIB_COMMON_RINGBUF_H
#define MCULIB_COMMON_RINGBUF_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * 环形缓冲区
 *
 * 默认模型：单生产者单消费者（SPSC），无锁。
 * 多生产者或多消费者：调用者需用 critical_enter/exit 保护。
 *
 * head：写指针，仅生产者修改。
 * tail：读指针，仅消费者修改。
 * 缓冲区由调用者提供，静态分配，不使用动态内存。
 */

struct ringbuf {
	uint8_t *buf;
	size_t size;
	volatile size_t head;
	volatile size_t tail;
};

void ringbuf_init(struct ringbuf *rb, uint8_t *buf, size_t size);
void ringbuf_reset(struct ringbuf *rb);

size_t ringbuf_write(struct ringbuf *rb, const uint8_t *data, size_t len);
size_t ringbuf_read(struct ringbuf *rb, uint8_t *data, size_t len);

size_t ringbuf_used(const struct ringbuf *rb);
size_t ringbuf_free(const struct ringbuf *rb);

bool ringbuf_is_empty(const struct ringbuf *rb);
bool ringbuf_is_full(const struct ringbuf *rb);

#endif