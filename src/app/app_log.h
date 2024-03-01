/**
 * @file    app_log.h
 * @brief   Non-blocking debug log on USART6 TX (PG14, Arduino D1), 115200 8N1.
 * @details USART3 carries HWLINK only. BOOT and every application share this
 *          port and line format, e.g. `[   5012] C-FRT I health: slot A
 *          confirmed`.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-06
 * @target  STM32F767ZIT6
 * @srs     DLOG-001..DLOG-004
 */

#ifndef APP_LOG_H
#define APP_LOG_H

#include <stdint.h>

/**
 * @brief Configure PG14 as USART6_TX and enable the TXE-driven output.
 * @note Lines logged before this call are discarded.
 */
void app_log_init(void);

/**
 * @brief Queue one log line.
 * @param[in] level 'E', 'W', 'I' or 'D'.
 * @param[in] fmt   printf-style format, no trailing newline.
 * @note Never blocks; a full ring drops the line and counts it. Safe from
 *       tasks, ISRs and before the scheduler starts.
 */
void app_log(char level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/**
 * @brief Busy-wait until every queued character has been sent.
 * @param[in] timeout_ms Upper bound, ms (approximate, loop-counted).
 * @note Call before a deliberate reset; works with interrupts disabled.
 */
void app_log_flush(uint32_t timeout_ms);

/**
 * @brief Get the number of lines dropped on a full ring (DLOG-003).
 * @return Dropped line count since start-up.
 */
uint32_t app_log_dropped(void);

#define LOG_E(...) app_log('E', __VA_ARGS__) /**< Log an error line. */
#define LOG_W(...) app_log('W', __VA_ARGS__) /**< Log a warning line. */
#define LOG_I(...) app_log('I', __VA_ARGS__) /**< Log an info line. */
#define LOG_D(...) app_log('D', __VA_ARGS__) /**< Log a debug line. */

#endif /* APP_LOG_H */
