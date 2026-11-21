/**
 * @file    clock.h
 * @brief   216 MHz core clock set-up.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6, HSE bypass from the ST-LINK MCO
 * @srs     DGEN-003, DBT-003
 */

#ifndef CLOCK_H
#define CLOCK_H

/**
 * @brief Configure HSE bypass + PLL for 216 MHz SYSCLK, APB1 54, APB2 108 MHz.
 * @note Starts from the reset clock state BOOT leaves (HND-001). Resets with
 *       APP_FAULT_STARTUP on failure.
 */
void app_clock_init(void);

#endif /* CLOCK_H */
