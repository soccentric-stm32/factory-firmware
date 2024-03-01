/**
 * @file    app_error.c
 * @brief   Latched application error bits; see app_error.h.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI
 * @srs     LED-005, DGEN-004, DGEN-005, HLT-003
 */

#include "stm32f7xx_hal.h"
#include "app_error.h"

volatile uint32_t app_error_flags; /**< Latched APP_ERROR_* bits. */

/**
 * @brief Set bits in the latch with interrupts masked.
 * @param[in] flags APP_ERROR_* mask to OR in.
 */
void app_error_set(uint32_t flags)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    app_error_flags |= flags;
    __set_PRIMASK(primask);
}

/** @brief Clear the whole latch with interrupts masked. */
void app_error_clear(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    app_error_flags = 0U;
    __set_PRIMASK(primask);
}

/** @brief Clear every bit except APP_ERROR_HEALTH (LED-005). */
void app_error_clear_fault(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    app_error_flags &= APP_ERROR_HEALTH;
    __set_PRIMASK(primask);
}
