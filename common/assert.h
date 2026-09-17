#ifndef MCULIB_COMMON_ASSERT_H
#define MCULIB_COMMON_ASSERT_H

#include <stddef.h>

/*
 * 断言与编译期检查
 *
 * MCULIB_ASSERT 仅在 Debug 构建生效。
 * 发布构建中表达式完全不求值，不产生代码。
 */

#ifdef MCULIB_DEBUG
void mculib_assert_failed(const char *file, int line, const char *expr);
#define MCULIB_ASSERT(expr) \
	((expr) ? (void)0 : mculib_assert_failed(__FILE__, __LINE__, #expr))
#else
#define MCULIB_ASSERT(expr) ((void)0)
#endif

#define MCULIB_UNUSED(x) ((void)(x))

#endif