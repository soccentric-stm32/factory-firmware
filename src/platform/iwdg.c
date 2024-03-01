/**
 * @file    iwdg.c
 * @brief   Independent watchdog service.
 * @details BOOT starts the IWDG at 4 s (TRY-004); without BOOT the firmware
 *          starts the same timeout. LSI 32 kHz / 64 with reload 1999 = 4.0 s.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DBT-002, DRT-012, HLT-003, HND-004
 */

#include <stdbool.h>
#include "stm32f7xx_hal.h"
#include "iwdg.h"

#define IWDG_KEY_ACCESS       0x00005555UL /**< KR value unlocking PR/RLR. */
#define IWDG_RELOAD_4S        1999U        /**< RLR for 4 s at LSI/64. */

/** @brief Wait until PR, RLR and WINR updates have completed. */
static void iwdg_wait_ready(void)
{
    while ((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU | IWDG_SR_WVU)) != 0U) {
        __NOP();
    }
}

/** @brief Reload the IWDG counter. */
void app_iwdg_early_kick(void)
{
    IWDG->KR = IWDG_KEY_RELOAD;
}

/** @brief Configure and start the IWDG at 4 s if not running, then refresh. */
void app_iwdg_init(void)
{
    /* PR 0 / RLR 0xFFF is the reset state; anything else means an earlier
     * stage already started the watchdog. */
    bool running = (IWDG->PR != 0U) || (IWDG->RLR != 0x0FFFU);

    if (!running) {
        IWDG->KR = IWDG_KEY_ACCESS;
        IWDG->PR = IWDG_PRESCALER_64;
        IWDG->RLR = IWDG_RELOAD_4S;
        iwdg_wait_ready();
        IWDG->KR = IWDG_KEY_RELOAD;
        IWDG->KR = IWDG_KEY_ENABLE;
    }

    app_iwdg_kick();
}

/** @brief Reload the IWDG counter. */
void app_iwdg_kick(void)
{
    IWDG->KR = IWDG_KEY_RELOAD;
}
