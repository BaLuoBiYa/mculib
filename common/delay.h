#ifndef MCULIB_COMMON_DELAY_H
#define MCULIB_COMMON_DELAY_H

#include <stdint.h>

/*
 * 延时工具
 *
 * 上下文约束：
 *   delay_ms：仅任务上下文。中断内禁止。
 *   delay_us：任务或中断上下文。中断内必须极短，且不依赖任何中断。
 *
 * 临界区内禁止调用。
 *
 * 返回值：成功返回 0，失败返回负值。
 * 线程安全：是。可重入。
 */

int delay_us(uint32_t us);
int delay_ms(uint32_t ms);

#endif