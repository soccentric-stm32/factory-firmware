/**
 * @file    handover.h
 * @brief   BOOT hand-over record (RTC_BKP2R) and stay-in-BOOT request.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DBT-004, DBT-005
 */

#ifndef HANDOVER_H
#define HANDOVER_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Decoded hand-over record. */
typedef struct {
    bool present;         /**< Valid magic and slot. */
    uint8_t slot;         /**< APP_SLOT_*. */
    uint8_t trial;        /**< 0 unknown, 1 confirmed, 2 trial pending. */
    uint8_t trial_count;  /**< BOOT trial counter, 0..3. */
    uint32_t raw;         /**< Raw RTC_BKP2R value. */
} app_handover_t;

/** Hand-over record decoded at start-up by app_handover_read(). */
extern app_handover_t g_handover;

/**
 * @brief Decode RTC_BKP2R into @ref g_handover.
 * @note A missing or invalid record gives slot UNKNOWN, trial 0 (DBT-004).
 *       Read-only: backup-domain write access stays off.
 */
void app_handover_read(void);

/** @brief Write the "stay in BOOT" magic to RTC_BKP1R (DBT-005). */
void app_handover_request_bootloader(void);

#endif /* HANDOVER_H */
