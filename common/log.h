#ifndef MCULIB_COMMON_LOG_H
#define MCULIB_COMMON_LOG_H

/*
 * 日志
 *
 * 编译期裁剪：
 *   通过 MCULIB_LOG_LEVEL 控制，未达级别的调用完全展开为空。
 *   发布构建通常裁掉 DEBUG / INFO。
 *
 * 上下文：
 *   任务上下文可用。
 *   中断上下文只允许 log_isr，且只写无锁环形缓冲区，不阻塞。
 *
 * 实现由适配层提供，后端可以是 UART、SWO、RTT、空实现。
 */

#define MCULIB_LOG_NONE 0
#define MCULIB_LOG_ERROR 1
#define MCULIB_LOG_WARN 2
#define MCULIB_LOG_INFO 3
#define MCULIB_LOG_DEBUG 4

#ifndef MCULIB_LOG_LEVEL
#define MCULIB_LOG_LEVEL MCULIB_LOG_INFO
#endif

void log_error(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_debug(const char *fmt, ...);

/* 中断内可用：只写无锁缓冲区，不阻塞 */
void log_isr(const char *fmt, ...);

#endif