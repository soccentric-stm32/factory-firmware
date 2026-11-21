/**
 * @file    crc32.h
 * @brief   CRC-32/ISO-HDLC (the zlib polynomial), no table in RAM.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     HWLINK LNK-003/004, BOOT Appendix B
 */

#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Compute the CRC-32/ISO-HDLC of a buffer.
 * @param[in] data Buffer to hash; may be NULL when @p len is 0.
 * @param[in] len  Length, bytes.
 * @return CRC-32 value, as the station and the bootloader compute it.
 * @note Bit-serial without a lookup table to save the 1 KB of RAM (LNK-003).
 */
uint32_t app_crc32(const void *data, size_t len);

#endif /* CRC32_H */
