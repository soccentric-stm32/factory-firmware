/**
 * @file    iwdg.h
 * @brief   Independent watchdog service.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DBT-002, DRT-012, HLT-003, HND-004
 */

#ifndef IWDG_H
#define IWDG_H

/**
 * @brief Refresh the IWDG that BOOT started with a 4 s timeout (HND-004).
 * @note Safe before the HAL and clocks are up.
 */
void app_iwdg_early_kick(void);

/** @brief Start the IWDG at 4 s unless already running, then refresh. */
void app_iwdg_init(void);

/**
 * @brief Refresh the IWDG.
 * @note Health calls this only while every task has checked in.
 */
void app_iwdg_kick(void);

#endif /* IWDG_H */
