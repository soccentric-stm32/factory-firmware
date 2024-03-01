/**
 * @file    rtos_hooks.c
 * @brief   FreeRTOS assert, stack-overflow and malloc-failed hooks.
 * @details Each hook saves a fault record (DGEN-004) and resets, so the next
 *          Status reports it.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DRT-002 (assert, stack overflow, malloc failed)
 */

#include "FreeRTOS.h"
#include "task.h"
#include "app_fault.h"

/**
 * @brief Get the running task's name.
 * @return Task name, or "startup" before the scheduler runs.
 */
static const char *current_task_name(void)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        return pcTaskGetName(NULL);
    }
    return "startup";
}

/**
 * @brief configASSERT handler: record APP_FAULT_ASSERT and reset.
 * @param[in] file Source path.
 * @param[in] line Source line.
 */
void app_fault_assert(const char *file, int line)
{
    app_fault_fatal(APP_FAULT_ASSERT, current_task_name(), file,
                     (uint32_t)line);
}

/**
 * @brief FreeRTOS stack overflow hook: record the task and reset.
 * @param[in] task Overflowing task (unused).
 * @param[in] name Overflowing task name.
 */
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    app_fault_fatal(APP_FAULT_STACK_OVERFLOW, name, "FreeRTOS", 0U);
}

/**
 * @brief FreeRTOS malloc-failed hook: record the fault and reset.
 * @note The line field holds the free heap size, bytes.
 */
void vApplicationMallocFailedHook(void)
{
    app_fault_fatal(APP_FAULT_MALLOC_FAILED, current_task_name(),
                    "malloc", (uint32_t)xPortGetFreeHeapSize());
}
