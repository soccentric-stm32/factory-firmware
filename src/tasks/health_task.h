/**
 * @file    health_task.h
 * @brief   Task supervision, health check and CONFIRMED record.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DRT-012, DRT-013, HLT-001..HLT-005
 */

#ifndef HEALTH_TASK_H
#define HEALTH_TASK_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Task slot the Health task supervises. */
typedef enum {
    HEALTH_TASK_LED = 0,    /**< LED task. */
    HEALTH_TASK_BUTTON,     /**< Button task. */
    HEALTH_TASK_COMMS,      /**< Comms task. */
    HEALTH_TASK_COUNT       /**< Number of supervised tasks. */
} health_task_id_t;

/**
 * @brief Create the highest-priority Health task ahead of the others.
 * @return true when the task was created.
 * @note Before the scheduler starts.
 */
bool health_task_create(void);

/**
 * @brief Report that a supervised task is still alive.
 * @param[in] task Task slot; out-of-range values are ignored.
 * @note Called by every task except Health at least once per second; the
 *       Health task drops the watchdog kick when a slot goes quiet for 2 s.
 */
void health_checkin(health_task_id_t task);

/**
 * @brief Get the pending-slot health state carried in Status.
 * @return One of the HealthState values (0 not required, 1 running, 2 passed,
 *         3 failed).
 */
uint8_t health_state_get(void);

/**
 * @brief Check whether the pending-slot health check failed.
 * @return true once the check failed, so the LED shows the error pattern.
 */
bool health_is_failed(void);

#endif /* HEALTH_TASK_H */
