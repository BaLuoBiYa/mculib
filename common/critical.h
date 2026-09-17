/* SPDX-License-Identifier: MIT */
#ifndef MCULIB_COMMON_CRITICAL_H
#define MCULIB_COMMON_CRITICAL_H

#include <stdint.h>

/*
 * Critical sections.
 *
 * Used to protect multi-instruction read-modify-write sequences and
 * multi-variable invariants.
 *
 * Nesting is supported: every critical_enter() must be paired with exactly
 * one critical_exit().
 *
 * Context: task context only. Must not be used from interrupt context, and
 * must not wrap calls that block, sleep, or wait.
 *
 * The implementation is provided by the adapter layer; it degrades to
 * disabling interrupts on targets without atomic instructions.
 */

/**
 * critical_enter() - enter a critical section
 *
 * Context: task context. Interrupts may be disabled on return.
 *
 * Return: state to pass to the matching critical_exit().
 */
uint32_t critical_enter(void);

/**
 * critical_exit() - leave a critical section
 * @state: state returned by the matching critical_enter()
 *
 * Context: task context. Must be called exactly once per critical_enter(),
 * and in reverse nesting order.
 */
void critical_exit(uint32_t state);

#endif /* MCULIB_COMMON_CRITICAL_H */