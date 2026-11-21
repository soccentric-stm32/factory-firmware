/**
 * @file    task_report.h
 * @brief   Once-a-second "task <n> <name>: <purpose>" line on the debug UART.
 * @details Each task calls @ref task_report every loop; it prints at most one
 *          line a second per task, so the log shows all four tasks running.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-06
 * @target  STM32F767ZIT6
 * @srs     DLOG-002
 */
#ifndef TASK_REPORT_H
#define TASK_REPORT_H

#include <stdint.h>

/** @brief Per-task report state; zero-initialise it. */
typedef struct
{
    uint32_t last_ms; /**< Time of the last printed line, ms. */
    uint8_t  primed;  /**< Non-zero once the first line was printed. */
} task_report_t;

/**
 * @brief Print the task's status line if a second has passed.
 * @param[in,out] state   Calling task's own report state.
 * @param[in]     number  Task number, 1 to 4.
 * @param[in]     name    Task name as given to xTaskCreate().
 * @param[in]     purpose One-sentence task purpose.
 * @note Call from task context only (uses app_log).
 */
void task_report(task_report_t *state, unsigned number, const char *name, const char *purpose);

#endif /* TASK_REPORT_H */
