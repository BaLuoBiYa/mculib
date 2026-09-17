#ifndef MCULIB_COMMON_CRITICAL_H
#define MCULIB_COMMON_CRITICAL_H

#include <stdint.h>

/*
 * 临界区
 *
 * 用于保护多指令读-改-写、多变量一致性。
 *
 * 支持嵌套：每次 enter 必须配对一次 exit。
 * 上下文：任务上下文可用；不可在中断内使用。
 *
 * 实现由适配层提供，无原生原子指令时降级为关中断。
 */

uint32_t critical_enter(void);
void critical_exit(uint32_t state);

#endif