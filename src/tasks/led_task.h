/**
 * @file    led_task.h
 * @brief   LED patterns and LD3 error display (section 6).
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI
 * @srs     LED-001..LED-007, BTN-002, BTN-003
 */

#ifndef LED_TASK_H
#define LED_TASK_H

#include <stdbool.h>
#include <stdint.h>

#define APP_LED_ID_1       1U /**< LD1, green. */
#define APP_LED_ID_2       2U /**< LD2, blue. */
#define APP_LED_ID_3       3U /**< LD3, red (error display). */

#define APP_PATTERN_OFF        0U /**< Steady off. */
#define APP_PATTERN_ON         1U /**< Steady on. */
#define APP_PATTERN_BLINK      2U /**< 50 % blink at the period. */
#define APP_PATTERN_HEARTBEAT  3U /**< Double flash, then pause. */
#define APP_PATTERN_FAST       4U /**< Fast blink, errors running. */
#define APP_PATTERN_CHASE      5U /**< All three LEDs chase. */
#define APP_PATTERN_MORSE_SOS  6U /**< Morse SOS, fatal error. */
#define APP_PATTERN_COUNT      7U /**< Number of patterns. */

#define APP_LED_MIN_PERIOD     50U   /**< Shortest accepted period, ms. */
#define APP_LED_MAX_PERIOD     5000U /**< Longest accepted period, ms. */
#define APP_LED_DEFAULT_PERIOD 1000U /**< Period restored on a long press. */

/** @brief Configure LD1..LD3 as outputs, all off. */
void app_leds_init(void);

/**
 * @brief Drive LD3 directly, for use before the LED task runs.
 * @param[in] on true lights LD3.
 */
void app_led3_set(bool on);

/**
 * @brief Create the LED task and its command queue.
 * @return true when the task was created.
 * @note Before the scheduler starts.
 */
bool led_task_create(void);

/**
 * @brief Queue a pattern for one LED (DRT-011: no direct state writes).
 * @param[in] led     APP_LED_ID_1..APP_LED_ID_3.
 * @param[in] pattern APP_PATTERN_*, clamped to APP_PATTERN_COUNT.
 * @param[in] period  Period, ms; clamped to APP_LED_MIN/MAX_PERIOD.
 * @param[in] force   true replaces an active error display.
 * @return true when the command was queued.
 */
bool led_post_set(uint8_t led, uint8_t pattern, uint16_t period, bool force);

/**
 * @brief Queue one pattern for all three LEDs.
 * @param[in] pattern APP_PATTERN_*.
 * @param[in] period  Period, ms.
 * @return true when the command was queued.
 */
bool led_post_set_all(uint8_t pattern, uint16_t period);

/**
 * @brief Queue a button press.
 * @param[in] press 1 = short (next pattern), 2 = long (defaults).
 * @return true when the command was queued.
 */
bool led_post_button(uint8_t press);

/** @brief Queue a ClearFault: drop the error display and return to normal. */
bool led_post_clear_error(void);

/** @brief Queue the health-failed pattern for LD3 (LED-005). */
bool led_post_health_failed(void);

#endif /* LED_TASK_H */
