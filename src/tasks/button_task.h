/**
 * @file    button_task.h
 * @brief   User button B1 (PC13).
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI
 * @srs     BTN-001..BTN-004
 */

#ifndef BUTTON_TASK_H
#define BUTTON_TASK_H

#include <stdbool.h>

/**
 * @brief Create the Button task and configure EXTI13.
 * @return true when the task was created.
 * @note Before the scheduler starts; the task debounces B1 and reports short
 *       and long presses to the LED task (BTN-002..BTN-004).
 */
bool button_task_create(void);

#endif /* BUTTON_TASK_H */
