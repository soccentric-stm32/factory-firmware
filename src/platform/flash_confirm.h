/**
 * @file    flash_confirm.h
 * @brief   Program the running slot's CONFIRMED metadata record.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6, dual-bank flash
 * @srs     HLT-002, HLT-005, DBT-006, DSAFE-002
 */

#ifndef FLASH_CONFIRM_H
#define FLASH_CONFIRM_H

#include <stdbool.h>

/**
 * @brief Program the running slot's CONFIRMED record if it is erased.
 * @return true when a valid record is present afterwards.
 * @note The firmware's only flash write. Fixed address, never erases; a torn
 *       record is left alone and returns false.
 */
bool app_flash_confirm_running_slot(void);

#endif /* FLASH_CONFIRM_H */
