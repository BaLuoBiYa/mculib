#ifndef MCULIB_COMMON_TIME_H
#define MCULIB_COMMON_TIME_H

#include <stdint.h>

/*
 * 系统时间
 *
 * 单调递增，不受系统时间调整影响。
 * 用于超时、计时、日志时间戳。
 *
 * 上下文：任务与中断上下文均可调用。
 * 线程安全：是。可重入。
 *
 * 溢出语义：
 *   time_now_ms 返回 uint32_t，约 49.7 天回绕。
 *   比较时使用 (int32_t)(a - b) 判断先后。
 */

uint32_t time_now_ms(void);
uint64_t time_now_ticks(void);

#endif