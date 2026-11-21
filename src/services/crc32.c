/**
 * @file    crc32.c
 * @brief   CRC-32/ISO-HDLC (the zlib polynomial).
 * @details Reflected bit-serial CRC with a 0xEDB88320 polynomial, so the
 *          firmware needs no 1 KB table and matches the station and the
 *          bootloader bit for bit.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     HWLINK LNK-003/004, BOOT Appendix B
 */

#include "crc32.h"

uint32_t app_crc32(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFUL;

    for (size_t i = 0U; i < len; i++) {
        crc ^= p[i];
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            crc = (crc & 1U) != 0U ? (crc >> 1U) ^ 0xEDB88320UL : crc >> 1U;
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}
