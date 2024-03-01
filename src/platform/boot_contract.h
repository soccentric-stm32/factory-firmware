/**
 * @file    boot_contract.h
 * @brief   Flash layout, hand-over and metadata contract with BOOT.
 * @details Mirrors SRS-BOOT-F767 LAY-001..LAY-005, HND-003 and Appendix B.
 *          The application only reads slot metadata and programs the one
 *          CONFIRMED record.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6, dual-bank flash (nDBANK = 0)
 * @srs     DBT-001, DBT-004, DBT-005, DBT-006, HLT-002, DSAFE-002
 * @note    Must match BOOT and the "Boot contract" values in Jenkinsfile.
 */

#ifndef BOOT_CONTRACT_H
#define BOOT_CONTRACT_H

#include <stdint.h>

/**
 * @name Slot layout (LAY-002)
 * Every image links at the bank-1 slot offset; BOOT maps the active bank at
 * 0x08000000 with SYSCFG_MEMRMP.SWP_FB, so one image runs from either slot.
 * @{
 */
#define APP_SLOT_BASE           0x0800C000UL   /**< Application link address. */
#define APP_SLOT_SIZE           0x000F4000UL   /**< Slot size, 976 KB. */

#define APP_BANK_SIZE           0x00100000UL   /**< Flash bank size, 1 MB. */
#define APP_META_OFFSET         0x00008000UL   /**< Metadata sector offset in a bank. */
#define APP_META_SIZE           0x00004000UL   /**< Metadata sector size, 16 KB. */

#define APP_META_A_ADDR         0x08008000UL   /**< Slot A metadata, no bank swap. */
#define APP_META_B_ADDR         0x08108000UL   /**< Slot B metadata, no bank swap. */

/** Running slot's metadata: SWP_FB maps the running bank at 0x08000000. */
#define APP_RUNNING_META_ADDR   0x08008000UL
/** @} */

/**
 * @name Metadata records (Appendix B)
 * Record offsets in the metadata sector and their little-endian magics.
 * @{
 */
#define APP_META_HEADER_OFFSET    0x000U /**< Image header offset. */
#define APP_META_VERIFIED_OFFSET  0x100U /**< VERIFIED record offset. */
#define APP_META_ACTIVATED_OFFSET 0x120U /**< ACTIVATED record offset. */
#define APP_META_CONFIRMED_OFFSET 0x140U /**< CONFIRMED record offset. */
#define APP_META_REJECTED_OFFSET  0x160U /**< REJECTED record offset. */
#define APP_META_RECORD_SIZE      32U    /**< Record size, bytes. */

#define APP_META_VERIFIED_MAGIC   0x56455249UL  /**< "VERI" */
#define APP_META_ACTIVATED_MAGIC  0x41435456UL  /**< "ACTV" */
#define APP_META_CONFIRMED_MAGIC  0x434F4E46UL  /**< "CONF" */
#define APP_META_REJECTED_MAGIC   0x52454A45UL  /**< "REJE" */

/** Running slot's CONFIRMED record address. */
#define APP_CONFIRMED_ADDR \
    (APP_RUNNING_META_ADDR + APP_META_CONFIRMED_OFFSET)
/** @} */

/**
 * @name Hand-over record in RTC_BKP2R (HND-003)
 * Bits 0..23 magic "HND", 24..25 slot (1 A, 2 B, 0 unknown), 26 trial
 * pending, 27..28 trial counter. BOOT writes the same word; a missing record
 * (no BOOT) is reported as slot UNKNOWN (DBT-004).
 * @{
 */
#define APP_HANDOVER_MAGIC        0x00444E48UL  /**< Bytes "HND", 0. */
#define APP_HANDOVER_MAGIC_MASK   0x00FFFFFFUL  /**< Magic field mask. */
#define APP_HANDOVER_SLOT_SHIFT   24U           /**< Slot field position. */
#define APP_HANDOVER_SLOT_MASK    0x3UL         /**< Slot field mask after shift. */
#define APP_HANDOVER_PENDING_BIT  26U           /**< Trial-pending bit position. */
#define APP_HANDOVER_TRIAL_SHIFT  27U           /**< Trial counter position. */
#define APP_HANDOVER_TRIAL_MASK   0x3UL         /**< Trial counter mask after shift. */
/** @} */

/** RTC_BKP1R "stay in BOOT" request, bytes "STAY" (ENT-002). */
#define APP_STAY_IN_BOOT_MAGIC    0x59415453UL

/**
 * @name No-init window (DGEN-004, A-9)
 * Top 1 KB of DTCM. BOOT uses 0x20000000..0x2000EFFF and never writes here;
 * .noinit lives here with the main stack just below, so the fault record
 * survives every reset. The linker script and app_fault.c assert these.
 * @{
 */
#define APP_NOINIT_ADDR           0x2001FC00UL /**< Window start. */
#define APP_NOINIT_SIZE           0x00000400UL /**< Window size, 1 KB. */
#define APP_NOINIT_END            0x20020000UL /**< Window end (exclusive). */
/** @} */

/**
 * @name Slot identifiers shared with Status
 * @{
 */
#define APP_SLOT_UNKNOWN          0U /**< No or invalid hand-over. */
#define APP_SLOT_A                1U /**< Slot A. */
#define APP_SLOT_B                2U /**< Slot B. */
/** @} */

#endif /* BOOT_CONTRACT_H */
