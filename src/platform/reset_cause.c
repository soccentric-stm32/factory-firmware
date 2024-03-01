/**
 * @file    reset_cause.c
 * @brief   RCC reset flags captured at start-up.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DGEN-005, LED-005
 */

#include "stm32f7xx_hal.h"
#include "reset_cause.h"

uint32_t app_reset_cause_flags;                 /**< Captured RCC_CSR flags. */
const char *app_reset_cause_text = "unknown";   /**< Decoded reset cause. */

/** @brief Capture and decode RCC_CSR reset flags, then clear them. */
void app_reset_cause_capture(void)
{
    app_reset_cause_flags = RCC->CSR & (RCC_CSR_LPWRRSTF |
                                        RCC_CSR_WWDGRSTF |
                                        RCC_CSR_IWDGRSTF |
                                        RCC_CSR_SFTRSTF |
                                        RCC_CSR_PORRSTF |
                                        RCC_CSR_PINRSTF |
                                        RCC_CSR_BORRSTF);

    /* Most specific first. Power-on sets POR, BOR and PIN, so test POR, then
     * BOR, then PIN (RM0410 6.3.17). */
    if ((app_reset_cause_flags & RCC_CSR_IWDGRSTF) != 0U) {
        app_reset_cause_text = "independent watchdog";
    } else if ((app_reset_cause_flags & RCC_CSR_WWDGRSTF) != 0U) {
        app_reset_cause_text = "window watchdog";
    } else if ((app_reset_cause_flags & RCC_CSR_LPWRRSTF) != 0U) {
        app_reset_cause_text = "low power";
    } else if ((app_reset_cause_flags & RCC_CSR_SFTRSTF) != 0U) {
        app_reset_cause_text = "software";
    } else if ((app_reset_cause_flags & RCC_CSR_PORRSTF) != 0U) {
        app_reset_cause_text = "power-on";
    } else if ((app_reset_cause_flags & RCC_CSR_BORRSTF) != 0U) {
        app_reset_cause_text = "brown-out";
    } else if ((app_reset_cause_flags & RCC_CSR_PINRSTF) != 0U) {
        app_reset_cause_text = "reset pin";
    } else {
        app_reset_cause_text = "unknown";
    }

    __HAL_RCC_CLEAR_RESET_FLAGS();
}

/** @brief Check for an IWDG or WWDG reset. @return true if so. */
bool app_reset_was_error(void)
{
    return (app_reset_cause_flags &
            (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF)) != 0U;
}
