/**
 * @file    app_fault.h
 * @brief   No-init fault record and fatal error paths.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DGEN-004, DRT-002, LED-005
 */

#ifndef APP_FAULT_H
#define APP_FAULT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup app_fault Fault record
 * @brief Fault record that survives a reset and is reported in Status.
 * @{
 */

#define APP_FAULT_MAGIC    0x4641554CUL /**< "FAUL": record is valid. */
#define APP_FAULT_TASK_LEN 16U          /**< Task name field size, incl. NUL. */
#define APP_FAULT_FILE_LEN 32U          /**< File name field size, incl. NUL. */

/** @brief Reason stored in the fault record. */
typedef enum {
    APP_FAULT_NONE = 0,       /**< No fault recorded. */
    APP_FAULT_HARDFAULT,      /**< HardFault exception. */
    APP_FAULT_MEMMANAGE,      /**< MemManage exception. */
    APP_FAULT_BUSFAULT,       /**< BusFault exception. */
    APP_FAULT_USAGEFAULT,     /**< UsageFault exception. */
    APP_FAULT_ASSERT,         /**< Failed assertion. */
    APP_FAULT_STACK_OVERFLOW, /**< FreeRTOS stack overflow hook. */
    APP_FAULT_MALLOC_FAILED,  /**< FreeRTOS malloc-failed hook. */
    APP_FAULT_STARTUP         /**< Start-up failure before the scheduler. */
} app_fault_reason_t;

/**
 * @brief Fault record kept in .noinit across a system reset.
 * @note Persistent layout: do not reorder fields.
 */
typedef struct {
    uint32_t magic;                    /**< APP_FAULT_MAGIC when valid. */
    uint32_t reason;                   /**< One of @ref app_fault_reason_t. */
    uint32_t cfsr;                     /**< SCB->CFSR at the fault. */
    uint32_t hfsr;                     /**< SCB->HFSR at the fault. */
    uint32_t mmfar;                    /**< SCB->MMFAR at the fault. */
    uint32_t bfar;                     /**< SCB->BFAR at the fault. */
    uint32_t pc;                       /**< Stacked PC, 0 if unknown. */
    uint32_t lr;                       /**< Stacked LR, 0 if unknown. */
    uint32_t psr;                      /**< Stacked xPSR, 0 if unknown. */
    uint32_t line;                     /**< Source line, 0 if unknown. */
    uint32_t boot_id;                  /**< Boot id when the record was saved. */
    char     task[APP_FAULT_TASK_LEN]; /**< NUL-terminated task name. */
    char     file[APP_FAULT_FILE_LEN]; /**< NUL-terminated source basename. */
} app_fault_record_t;

/**
 * @brief The fault record.
 * @note Placed in .noinit so start-up code leaves it intact (DGEN-004).
 */
extern app_fault_record_t g_app_fault;

/**
 * @brief Increment the no-init boot id.
 * @note Call first thing in main().
 */
void app_fault_boot_start(void);

/**
 * @brief Check the record's magic and reason.
 * @return true when the record is valid.
 */
bool app_fault_is_valid(void);

/**
 * @brief Check for a valid record saved during this boot.
 * @return true for a valid record from the current boot.
 */
bool app_fault_is_current_boot(void);

/**
 * @brief Check for a valid record saved during the previous boot.
 * @return true for a valid record from the previous boot.
 */
bool app_fault_is_previous_boot(void);

/**
 * @brief Map a reason to its Status name.
 * @param[in] reason Fault reason.
 * @return Static name, or "unknown" when out of range.
 */
const char *app_fault_reason_text(app_fault_reason_t reason);

/**
 * @brief Get the recorded task name.
 * @return NUL-terminated name inside the record.
 */
const char *app_fault_task(void);

/**
 * @brief Get the recorded source file basename.
 * @return NUL-terminated name inside the record.
 */
const char *app_fault_file(void);

/**
 * @brief Save a fault record.
 * @param[in] reason Fault reason.
 * @param[in] cfsr   SCB->CFSR value.
 * @param[in] hfsr   SCB->HFSR value.
 * @param[in] mmfar  SCB->MMFAR value.
 * @param[in] bfar   SCB->BFAR value.
 * @param[in] pc     Stacked PC.
 * @param[in] lr     Stacked LR.
 * @param[in] psr    Stacked xPSR.
 * @param[in] task   Task name, or NULL.
 * @param[in] file   Source path, or NULL; only the basename is kept.
 * @param[in] line   Source line.
 * @note Safe from task, ISR and fault context, with interrupts off.
 */
void app_fault_save(app_fault_reason_t reason, uint32_t cfsr, uint32_t hfsr,
                    uint32_t mmfar, uint32_t bfar, uint32_t pc, uint32_t lr,
                    uint32_t psr, const char *task, const char *file,
                    uint32_t line);

/**
 * @brief Flash the fault pattern on LD3 for about 2 s, then reset.
 * @note Works before the HAL and from fault context; never returns.
 */
void app_fault_show_and_reset(void) __attribute__((noreturn));

/**
 * @brief Record a fatal software error and reset.
 * @param[in] reason Fault reason.
 * @param[in] task   Task name, or NULL.
 * @param[in] file   Source path, or NULL.
 * @param[in] line   Source line.
 */
void app_fault_fatal(app_fault_reason_t reason, const char *task,
                     const char *file, uint32_t line) __attribute__((noreturn));

/** @brief Invalidate the fault record. */
void app_fault_clear(void);

/** @} */

#endif /* APP_FAULT_H */
