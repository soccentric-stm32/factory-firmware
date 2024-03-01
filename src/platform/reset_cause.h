/**
 * @file    reset_cause.h
 * @brief   RCC reset flags captured at start-up.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DGEN-005, LED-005
 */

#ifndef RESET_CAUSE_H
#define RESET_CAUSE_H

#include <stdbool.h>
#include <stdint.h>

/** RCC_CSR reset flags captured at start-up. */
extern uint32_t app_reset_cause_flags;
/** Most specific reset cause, e.g. "power-on"; static string. */
extern const char *app_reset_cause_text;

/** @brief Save RCC_CSR reset flags for Status, then clear them. */
void app_reset_cause_capture(void);

/**
 * @brief Check whether the last reset was an IWDG or WWDG reset.
 * @return true for a watchdog reset; LD3 is latched for it (LED-005).
 */
bool app_reset_was_error(void);

#endif /* RESET_CAUSE_H */
