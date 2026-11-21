/**
 * @file    factory_proto.h
 * @brief   Factory command handler (docs/FACTORY_SPEC.md section 5).
 * @details Pure logic: every hardware access goes through factory_ops_t, so
 *          the same code runs on the board and against a simulated flash in
 *          the host tests.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-09
 * @target  STM32F767ZIT6 (also builds on the host for tests)
 * @srs     docs/FACTORY_SPEC.md section 5
 */

#ifndef FACTORY_PROTO_H
#define FACTORY_PROTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FACTORY_CMD_INFO          0x01U /**< Get firmware info. */
#define FACTORY_CMD_READ          0x02U /**< Read memory. */
#define FACTORY_CMD_WRITE_BEGIN   0x03U /**< Start a block write. */
#define FACTORY_CMD_WRITE_DATA    0x04U /**< Send a chunk. */
#define FACTORY_CMD_WRITE_END     0x05U /**< Commit the block. */
#define FACTORY_CMD_DIAG_INIT     0x06U /**< Write a clean .diag block. */
#define FACTORY_CMD_SELFTEST      0x07U /**< Run the self-test. */
#define FACTORY_REPLY_FLAG        0x80U /**< OR-ed into the command byte of a reply. */

#define FACTORY_OK                0U /**< Request succeeded. */
#define FACTORY_ERR_BAD_COMMAND   1U /**< Unknown command or too short. */
#define FACTORY_ERR_BAD_RANGE     2U /**< Address, length or size outside the window. */
#define FACTORY_ERR_BAD_HEADER    3U /**< File header, length or header CRC is wrong. */
#define FACTORY_ERR_BAD_CRC       4U /**< Payload CRC does not match. */
#define FACTORY_ERR_BAD_UID       5U /**< File is bound to another chip UID. */
#define FACTORY_ERR_FLASH         6U /**< Flash write or read-back failed. */
#define FACTORY_ERR_SEQUENCE      7U /**< WRITE_* out of order or incomplete. */

/* SELFTEST check ids and results (docs/FACTORY_SPEC.md section 5.1). */
#define FACTORY_CHECK_DEVICE_ID   1U   /**< DBGMCU DEV_ID is 0x451 (STM32F76x/77x). */
#define FACTORY_CHECK_FLASH_SIZE  2U   /**< Flash size register reads 2048 KB. */
#define FACTORY_CHECK_CLOCK       3U   /**< System clock runs at 216 MHz. */
#define FACTORY_CHECK_RAM         4U   /**< Walking-bit test of 4 KB of SRAM. */
#define FACTORY_CHECK_CRC         5U   /**< CRC-32 known-answer test. */
#define FACTORY_CHECK_FLASH_READ  6U   /**< The identity sector reads back. */
#define FACTORY_CHECK_OPTION      7U   /**< Dual-bank option bytes, RDP not level 2. */
#define FACTORY_CHECK_COUNT       7U   /**< Number of checks in a SELFTEST reply. */

#define FACTORY_CHECK_PASS        0U /**< Check passed. */
#define FACTORY_CHECK_FAIL        1U /**< Check failed. */
#define FACTORY_CHECK_SKIP        2U /**< Reader not wired, check skipped. */

#define FACTORY_DEVICE_ID         0x451UL       /**< DEV_ID of STM32F76x/77x. */
#define FACTORY_FLASH_SIZE_KB     2048UL        /**< Expected flash size, KB. */
#define FACTORY_CORE_CLOCK_HZ     216000000UL   /**< Expected SYSCLK, Hz. */

/* Factory data window inside the identity sector (unswapped addresses). */
#define FACTORY_WINDOW_BASE       0x08105000UL /**< First writable address. */
#define FACTORY_WINDOW_END        0x08108000UL /**< End of the writable window. */
#define FACTORY_READ_BASE         0x08104000UL /**< First readable address (PRV1). */
#define FACTORY_BLOCK_SIZE        0x1000U      /**< Flash block, bytes. */
#define FACTORY_MAX_CHUNK         512U         /**< Largest chunk, bytes. */

#define FACTORY_FILE_HEADER_SIZE  32U /**< Magic, version, CRCs, UID, header CRC. */
#define FACTORY_MAGIC_SYS         0x31535953UL /**< "SYS1" system info block. */
#define FACTORY_MAGIC_CAL         0x314C4143UL /**< "CAL1" calibration block. */
#define FACTORY_MAGIC_DIAG        0x47414944UL /**< "DIAG" diagnostic block. */

/**
 * @brief Hardware and build-information hooks the handler calls.
 * @note Pointers that are NULL are either unused or, for the SELFTEST
 *       readers, reported as FACTORY_CHECK_SKIP.
 */
typedef struct {
    /** Read @p len bytes at a logical (unswapped) address. */
    bool (*flash_read)(uint32_t address, uint8_t *out, uint16_t len);
    /** Replace one 4 KB block (address is block aligned) and verify it. */
    bool (*flash_write_block)(uint32_t address, const uint8_t *data);
    /** Copy the 96-bit chip UID. */
    void (*chip_uid)(uint8_t uid[12]);
    /** CRC-32/ISO-HDLC of a buffer. */
    uint32_t (*crc32)(const void *data, size_t len);
    const char *name;          /**< Firmware name for INFO. */
    const char *version;       /**< Version string for INFO. */
    const char *build;         /**< Build date for INFO. */
    /* Hardware readers for SELFTEST; a NULL reader skips its check. */
    uint32_t (*device_id)(void);        /**< DBGMCU_IDCODE. */
    uint32_t (*flash_size_kb)(void);    /**< Flash size register, KB. */
    uint32_t (*core_clock_hz)(void);    /**< System clock, Hz. */
    uint32_t (*option_bytes)(void);     /**< FLASH_OPTCR. */
} factory_ops_t;

/**
 * @brief Handle one request payload and build the reply payload.
 * @param[in]  ops            Hardware hooks, non-NULL.
 * @param[in]  request        Request bytes; request[0] is the command.
 * @param[in]  request_len    Request length, bytes.
 * @param[out] reply          Reply buffer.
 * @param[in]  reply_capacity Capacity of @p reply, bytes.
 * @return Reply length, or 0 when the request is too short to answer.
 * @note A WRITE is staged in RAM and reaches flash only at WRITE_END.
 */
size_t factory_handle(const factory_ops_t *ops, const uint8_t *request,
                      size_t request_len, uint8_t *reply, size_t reply_capacity);

/**
 * @brief Drop any partly received write.
 * @note Call after a link reset, before the host starts a new write.
 */
void factory_reset_session(void);

#endif /* FACTORY_PROTO_H */
