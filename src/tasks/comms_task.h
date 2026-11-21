/**
 * @file    comms_task.h
 * @brief   HWLINK Comms task.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     COM-001..COM-007, BTN-004, section 8
 */

#ifndef COMMS_TASK_H
#define COMMS_TASK_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Create the Comms task and its event queue.
 * @return true when the task and queue were created.
 * @note Before the scheduler starts; the task owns USART3 and the frame
 *       decoder.
 */
bool comms_task_create(void);

/**
 * @brief Wake the Comms task after received data or a receive event.
 * @note Called from the USART3/DMA ISRs, so it must stay ISR-safe.
 */
void comms_post_rx_from_isr(void);

/**
 * @brief Report a button press to the station.
 * @param[in] press   Press length: 1 = short, 2 = long.
 * @param[in] pattern Pattern the press selected (APP_PATTERN_*).
 * @param[in] period  Period of that pattern, ms.
 */
void comms_post_button_event(uint8_t press, uint8_t pattern, uint16_t period);

#endif /* COMMS_TASK_H */
