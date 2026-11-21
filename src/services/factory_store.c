/**
 * @file    factory_store.c
 * @brief   Flash access for the factory data window.
 * @details The factory data blocks live in bank 2 sector 13 (0x08104000, or
 *          0x08004000 while the bank swap is active).  A sector erases as a
 *          unit, so a block write copies the 16 KB sector to RAM, patches one
 *          4 KB block, erases the sector and programs it back.  Only this
 *          window is ever touched.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-09
 * @target  STM32F767ZIT6, dual-bank flash
 */

#include <string.h>
#include "stm32f7xx_hal.h"
#include "crc32.h"
#include "factory_store.h"

#ifndef APP_FIRMWARE_NAME
#define APP_FIRMWARE_NAME "soccentric-factory-firmware"
#endif
#ifndef APP_VERSION_STR
#define APP_VERSION_STR "0.2.0"
#endif
#ifndef APP_BUILD_DATE
#define APP_BUILD_DATE "unknown"
#endif

#define STORE_SECTOR_BASE      0x08104000UL /**< Unswapped sector address. */
#define STORE_SECTOR_SWAPPED   0x08004000UL /**< Address while bank 2 is swapped. */
#define STORE_SECTOR_SIZE      0x4000UL     /**< Sector copy size in RAM, bytes. */

/** Scratch copy of the whole identity sector during a block write. */
static uint8_t sector_copy[STORE_SECTOR_SIZE];

/**
 * @brief Map a logical factory address to the address the flash is at now.
 * @param[in] logical Unswapped address.
 * @return Address to use, honouring SYSCFG_MEMRMP_SWP_FB.
 */
static uint32_t physical(uint32_t logical)
{
    if ((SYSCFG->MEMRMP & SYSCFG_MEMRMP_SWP_FB) != 0U) {
        return logical - STORE_SECTOR_BASE + STORE_SECTOR_SWAPPED;
    }
    return logical;
}

/**
 * @brief Read flash through the D-cache.
 * @param[in]  address Logical address.
 * @param[out] out     Destination buffer.
 * @param[in]  len     Length, bytes.
 * @return Always true; the flash controller is not checked.
 */
static bool store_read(uint32_t address, uint8_t *out, uint16_t len)
{
    uint32_t source = physical(address);

    SCB_InvalidateDCache_by_Addr((uint32_t *)(source & ~31UL), (int32_t)len + 32);
    memcpy(out, (const void *)source, len);
    return true;
}

/**
 * @brief Replace one 4 KB block by rewriting the whole sector.
 * @param[in] address Block-aligned logical address.
 * @param[in] data    New block contents, FACTORY_BLOCK_SIZE bytes.
 * @return true when the erase and every program step succeeded.
 * @note Halts nothing: the Comms task is the only writer, and interrupts
 *       stay on, so only the caller stalls while the sector is erased.
 */
static bool store_write_block(uint32_t address, const uint8_t *data)
{
    uint32_t sector = physical(STORE_SECTOR_BASE);
    uint32_t block = physical(address) - sector;

    SCB_InvalidateDCache_by_Addr((uint32_t *)sector, (int32_t)STORE_SECTOR_SIZE);
    memcpy(sector_copy, (const void *)sector, STORE_SECTOR_SIZE);
    memcpy(&sector_copy[block], data, FACTORY_BLOCK_SIZE);

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return false;
    }
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_ALL_ERRORS);

    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_SECTORS,
        .Sector = FLASH_SECTOR_13,
        .NbSectors = 1U,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
    };
    uint32_t sector_error = 0U;
    bool ok = HAL_FLASHEx_Erase(&erase, &sector_error) == HAL_OK;

    for (uint32_t i = 0U; ok && i < STORE_SECTOR_SIZE; i += 4U) {
        uint32_t word = 0U;
        memcpy(&word, &sector_copy[i], 4U);
        if (word == 0xFFFFFFFFUL) {
            continue;
        }
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, sector + i, word) == HAL_OK;
    }
    HAL_FLASH_Lock();
    SCB_InvalidateDCache_by_Addr((uint32_t *)sector, (int32_t)STORE_SECTOR_SIZE);
    return ok;
}

/** @brief Copy the 96-bit chip UID into @p uid. */
static void store_uid(uint8_t uid[12])
{
    uint32_t words[3] = { HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2() };

    memcpy(uid, words, 12U);
}

/** @brief CRC-32 of a buffer, forwarded to app_crc32(). */
static uint32_t store_crc32(const void *data, size_t len)
{
    return app_crc32(data, len);
}

/** @brief DBGMCU_IDCODE (SELFTEST check FACTORY_CHECK_DEVICE_ID). */
static uint32_t store_device_id(void)
{
    return DBGMCU->IDCODE;
}

/** @brief Flash size register, KB (SELFTEST check FACTORY_CHECK_FLASH_SIZE). */
static uint32_t store_flash_size_kb(void)
{
    return (uint32_t)(*(const volatile uint16_t *)FLASHSIZE_BASE);
}

/** @brief System clock, Hz (SELFTEST check FACTORY_CHECK_CLOCK). */
static uint32_t store_core_clock_hz(void)
{
    return HAL_RCC_GetSysClockFreq();
}

/** @brief FLASH_OPTCR (SELFTEST check FACTORY_CHECK_OPTION). */
static uint32_t store_option_bytes(void)
{
    return FLASH->OPTCR;
}

/** Operations table handed to factory_handle(). */
static const factory_ops_t ops = {
    .flash_read = store_read,
    .flash_write_block = store_write_block,
    .chip_uid = store_uid,
    .crc32 = store_crc32,
    .name = APP_FIRMWARE_NAME,
    .version = APP_VERSION_STR,
    .build = APP_BUILD_DATE,
    .device_id = store_device_id,
    .flash_size_kb = store_flash_size_kb,
    .core_clock_hz = store_core_clock_hz,
    .option_bytes = store_option_bytes,
};

const factory_ops_t *factory_store_ops(void)
{
    return &ops;
}
