/* SPDX-License-Identifier: MIT */
#ifndef MCULIB_COMMON_LOG_H
#define MCULIB_COMMON_LOG_H

/*
 * Logging.
 *
 * Compile-time trimming: MCULIB_LOG_LEVEL selects the compile-time level,
 * and calls below it expand to nothing. Release builds normally trim DEBUG
 * and INFO.
 *
 * Context: the log_error() / log_warn() / log_info() / log_debug() helpers
 * are for task context. Interrupt context may only use log_isr(), which
 * writes to a lock-free ring buffer and never blocks.
 *
 * The implementation is provided by the adapter layer; the backend may be
 * UART, SWO, RTT, or a stub.
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

/**
 * log_isr() - log a message from interrupt context
 * @fmt: printf-style format string
 * @...: format arguments
 *
 * Only writes to a lock-free ring buffer and never blocks. The buffer is
 * flushed by task context.
 *
 * Context: interrupt context.
 */
void log_isr(const char *fmt, ...);

#endif /* MCULIB_COMMON_LOG_H */