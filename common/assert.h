/* SPDX-License-Identifier: MIT */
#ifndef MCULIB_COMMON_ASSERT_H
#define MCULIB_COMMON_ASSERT_H

#include <stddef.h>

/*
 * Assertions and compile-time checks.
 *
 * MCULIB_ASSERT is only active in Debug builds. In release builds the
 * expression is not evaluated and no code is generated.
 */

/**
 * MCULIB_ASSERT() - check an invariant
 * @expr: expression expected to be true
 *
 * The expression is not evaluated when MCULIB_DEBUG is undefined, so it must
 * not have side effects.
 *
 * Context: any context.
 */
#ifdef MCULIB_DEBUG
void mculib_assert_failed(const char *file, int line, const char *expr);

#define MCULIB_ASSERT(expr) \
	((expr) ? (void)0 : mculib_assert_failed(__FILE__, __LINE__, #expr))
#else
#define MCULIB_ASSERT(expr) ((void)0)
#endif

/* Mark a parameter or variable as intentionally unused */
#define MCULIB_UNUSED(x) ((void)(x))

#endif /* MCULIB_COMMON_ASSERT_H */