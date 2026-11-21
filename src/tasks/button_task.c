/**
 * @file    button_task.c
 * @brief   User button B1 (PC13).
 * @details B1 is high when pressed.  EXTI13 wakes the task on both edges; a
 *          30 ms software debounce filters the contact.  A short press
 *          (< 1 s) selects the next pattern, a long press (>= 2 s) restores
 *          the defaults.  The LED task owns the patterns; this task only
 *          reports the press through its queue.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI
 * @srs     BTN-001..BTN-004
 */

#include "app_log.h"
#include "task_report.h"
#include "stm32f7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app_board.h"
#include "health_task.h"
#include "led_task.h"
#include "button_task.h"

#define BUTTON_TASK_STACK_WORDS  384U   /**< Room for app_log() formatting. */
#define BUTTON_TASK_PRIORITY     (tskIDLE_PRIORITY + 3U) /**< Task priority. */
#define BUTTON_DEBOUNCE_MS       30U   /**< Contact debounce, ms. */
#define BUTTON_SHORT_MAX_MS      1000U /**< Short press below this, ms. */
#define BUTTON_LONG_MS           2000U /**< Long press from this, ms. */
#define BUTTON_CHECKIN_MS        500U  /**< Health check-in period, ms. */
#define BUTTON_POLL_MS           10U   /**< Wake-up when no edge arrives, ms. */

static TaskHandle_t button_task_handle; /**< Set by button_task_create(). */

/** @brief Read B1's level, true when pressed. */
static bool button_raw(void)
{
    return (GPIOC->IDR & APP_BTN1_PIN) != 0U;
}

/** @brief Configure PC13 as a pulled-down input and enable EXTI13 on both edges. */
static void button_exti_init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = APP_BTN1_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLDOWN;   /* B1 is high when pressed */
    HAL_GPIO_Init(APP_BTN1_PORT, &gpio);

    /* EXTI13 -> port C (SYSCFG_EXTICR4, field 1). */
    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~(0xFUL << 4U)) | (0x2UL << 4U);
    EXTI->IMR  |= (1UL << 13U);
    EXTI->RTSR |= (1UL << 13U);
    EXTI->FTSR |= (1UL << 13U);
    EXTI->PR    = (1UL << 13U);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/**
 * @brief EXTI15_10 handler for B1.
 * @note Clears the pending bit and wakes the Button task; the press length is
 *       measured in task context.
 */
void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR & (1UL << 13U)) != 0U) {
        EXTI->PR = (1UL << 13U);
        if (button_task_handle != NULL) {
            BaseType_t woken = pdFALSE;
            vTaskNotifyGiveFromISR(button_task_handle, &woken);
            portYIELD_FROM_ISR(woken);
        }
    }
}

/**
 * @brief Button task body: debounce B1 and report presses to the LED task.
 * @param[in] arg Unused.
 */
static void button_task(void *arg)
{
    (void)arg;
    uint32_t now = xTaskGetTickCount();
    bool stable = button_raw();
    bool candidate = stable;
    uint32_t candidate_since = now;
    uint32_t press_start = now;
    bool pressed = stable;
    bool long_sent = false;
    uint32_t last_checkin = now;
    task_report_t report = {0U, 0U};

    for (;;) {
        task_report(&report, 2U, "Button", "debounces B1, short press cycles the LED pattern, long press resets it");
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BUTTON_POLL_MS));
        now = xTaskGetTickCount();
        bool raw = button_raw();

        if (raw != candidate) {
            candidate = raw;
            candidate_since = now;
        } else if (candidate != stable &&
                   (now - candidate_since) >= BUTTON_DEBOUNCE_MS) {
            stable = candidate;
            if (stable) {
                pressed = true;
                long_sent = false;
                press_start = now;
            } else {
                uint32_t duration = now - press_start;
                if (pressed && !long_sent) {
                    if (duration < BUTTON_SHORT_MAX_MS) {
                        LOG_I("button: short press, next LED pattern");
                        (void)led_post_button(1U);
                    } else if (duration >= BUTTON_LONG_MS) {
                        (void)led_post_button(2U);
                    }
                }
                pressed = false;
            }
        }

        if (pressed && !long_sent &&
            (now - press_start) >= BUTTON_LONG_MS) {
            long_sent = true;
            LOG_I("button: long press, default LED patterns");
            (void)led_post_button(2U);
        }

        if ((now - last_checkin) >= BUTTON_CHECKIN_MS) {
            health_checkin(HEALTH_TASK_BUTTON);
            last_checkin = now;
        }
    }
}

bool button_task_create(void)
{
    if (xTaskCreate(button_task, "Button", BUTTON_TASK_STACK_WORDS, NULL,
                    BUTTON_TASK_PRIORITY, &button_task_handle) != pdPASS) {
        return false;
    }
    button_exti_init();
    return true;
}
