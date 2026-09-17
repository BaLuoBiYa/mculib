#ifndef SUBSYSTEM_GPIO_H
#define SUBSYSTEM_GPIO_H

#include <stdint.h>

/*
 * GPIO 子系统契约
 *
 * 设备模型：一个 gpio_dev 实例代表一个已绑定的 GPIO 引脚。
 * 线程安全：是。同一 gpio_dev 上的并发调用由适配层串行化。
 * 返回值：成功返回 0，失败返回负值。
 *
 * 中断触发方式：在板级初始化时由适配层固定，运行时不可变。
 *
 * GPIO 中断回调
 *
 * 上下文：中断上下文（ISR 内直接执行）。
 *
 * 严格模式：回调只做轻量级置位。
 * 若 ctx 指向 volatile bool，回调只允许：
 *     *(volatile bool *)ctx = true;
 * 若 ctx 指向其他类型，使用者自行承担后果。
 *
 * 回调必须满足：
 *   - 不阻塞、不加锁、不等待、不动态分配；
 *   - 不调用子系统 API；
 *   - 执行时间有上限（建议 <= 1µs 或 <= 50 周期）。
 *
 * 违约后果由使用者承担，框架不提供运行时保护。
 *
 * 回调在 ISR 内执行，可能与任务上下文中的 GPIO 操作并发。
 * 适配层需保证 API 原子性，或约定回调只置位、不操作 GPIO。
 */

struct gpio_dev {
	const char *name; /* 调试用 */
	void *priv; /* 适配层私有数据，子系统层不解释 */
};

typedef void (*gpio_irq_cb_t)(struct gpio_dev *dev, void *ctx);

int gpio_set(struct gpio_dev *dev);
int gpio_reset(struct gpio_dev *dev);
int gpio_toggle(struct gpio_dev *dev);

int gpio_read(struct gpio_dev *dev, uint8_t *value);

int gpio_irq_enable(struct gpio_dev *dev, gpio_irq_cb_t cb, void *ctx);
int gpio_irq_disable(struct gpio_dev *dev);

#endif