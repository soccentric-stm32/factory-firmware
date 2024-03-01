/**
 * @file    app_error.h
 * @brief   Latched application error bits shown on LD3 and in Status.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI
 * @srs     LED-005, DGEN-004, DGEN-005, HLT-003
 */

#ifndef APP_ERROR_H
#define APP_ERROR_H

#include <stdint.h>

/**
 * @defgroup app_error Application error latch
 * @brief Latched error bits reported on LD3 and in Status.
 * @{
 */

#define APP_ERROR_FAULT    (1UL << 0U) /**< Saved fault or assert. */
#define APP_ERROR_WATCHDOG (1UL << 1U) /**< Last reset was IWDG or WWDG. */
#define APP_ERROR_HEALTH   (1UL << 2U) /**< Pending-slot health check failed. */

/**
 * @brief Latched APP_ERROR_* bits, read by the LED task and Status.
 * @note Only the owning module sets a bit; ClearFault clears the latch.
 */
extern volatile uint32_t app_error_flags;

/**
 * @brief Set bits in the latch.
 * @param[in] flags APP_ERROR_* mask to OR in.
 * @note Safe from task and ISR context (masks interrupts briefly).
 */
void app_error_set(uint32_t flags);

/**
 * @brief Clear the whole latch.
 * @note Used at start-up and by tests.
 */
void app_error_clear(void);

/**
 * @brief Clear FAULT and WATCHDOG, keep HEALTH.
 * @note ClearFault must not hide a pending-slot health failure (LED-005).
 */
void app_error_clear_fault(void);

/** @} */

#endif /* APP_ERROR_H */
