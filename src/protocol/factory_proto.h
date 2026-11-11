/*
 * factory_proto.h - factory command handler (docs/FACTORY_SPEC.md section 5)
 *
 * Author:  Sandesh Ghimire <sandesh@soccentric.com>
 * Date:    2026-11-09
 * Target:  STM32F767ZIT6 (also builds on the host for tests)
 *
 * Pure logic: every hardware access goes through factory_ops_t, so the same
 * code runs on the board and against a simulated flash in the host tests.
 */

#ifndef FACTORY_PROTO_H
#define FACTORY_PROTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FACTORY_CMD_INFO          0x01U
#define FACTORY_CMD_READ          0x02U
#define FACTORY_CMD_WRITE_BEGIN   0x03U
#define FACTORY_CMD_WRITE_DATA    0x04U
#define FACTORY_CMD_WRITE_END     0x05U
#define FACTORY_CMD_DIAG_INIT     0x06U
#define FACTORY_CMD_SELFTEST      0x07U
#define FACTORY_REPLY_FLAG        0x80U

#define FACTORY_OK                0U
#define FACTORY_ERR_BAD_COMMAND   1U
#define FACTORY_ERR_BAD_RANGE     2U
#define FACTORY_ERR_BAD_HEADER    3U
#define FACTORY_ERR_BAD_CRC       4U
#define FACTORY_ERR_BAD_UID       5U
#define FACTORY_ERR_FLASH         6U
#define FACTORY_ERR_SEQUENCE      7U

/* SELFTEST check ids and results (docs/FACTORY_SPEC.md section 5.1). */
#define FACTORY_CHECK_DEVICE_ID   1U   /* DBGMCU DEV_ID is 0x451 (STM32F76x/77x) */
#define FACTORY_CHECK_FLASH_SIZE  2U   /* flash size register reads 2048 KB */
#define FACTORY_CHECK_CLOCK       3U   /* system clock runs at 216 MHz */
#define FACTORY_CHECK_RAM         4U   /* walking-bit test of 4 KB of SRAM */
#define FACTORY_CHECK_CRC         5U   /* CRC-32 known-answer test */
#define FACTORY_CHECK_FLASH_READ  6U   /* the identity sector reads back */
#define FACTORY_CHECK_OPTION      7U   /* dual-bank option bytes, RDP not level 2 */
#define FACTORY_CHECK_COUNT       7U

#define FACTORY_CHECK_PASS        0U
#define FACTORY_CHECK_FAIL        1U
#define FACTORY_CHECK_SKIP        2U

#define FACTORY_DEVICE_ID         0x451UL
#define FACTORY_FLASH_SIZE_KB     2048UL
#define FACTORY_CORE_CLOCK_HZ     216000000UL

/* Factory data window inside the identity sector (unswapped addresses). */
#define FACTORY_WINDOW_BASE       0x08105000UL
#define FACTORY_WINDOW_END        0x08108000UL
#define FACTORY_READ_BASE         0x08104000UL   /* PRV1 is readable */
#define FACTORY_BLOCK_SIZE        0x1000U
#define FACTORY_MAX_CHUNK         512U

#define FACTORY_FILE_HEADER_SIZE  32U
#define FACTORY_MAGIC_SYS         0x31535953UL   /* "SYS1" */
#define FACTORY_MAGIC_CAL         0x314C4143UL   /* "CAL1" */
#define FACTORY_MAGIC_DIAG        0x47414944UL   /* "DIAG" */

typedef struct {
    /* Read len bytes at a logical (unswapped) address. */
    bool (*flash_read)(uint32_t address, uint8_t *out, uint16_t len);
    /* Replace one 4 KB block (address is block aligned) and verify it. */
    bool (*flash_write_block)(uint32_t address, const uint8_t *data);
    void (*chip_uid)(uint8_t uid[12]);
    uint32_t (*crc32)(const void *data, size_t len);
    const char *name;
    const char *version;
    const char *build;
    /* Hardware readers for SELFTEST; a NULL reader skips its check. */
    uint32_t (*device_id)(void);        /* DBGMCU_IDCODE */
    uint32_t (*flash_size_kb)(void);    /* flash size register, KB */
    uint32_t (*core_clock_hz)(void);    /* system clock, Hz */
    uint32_t (*option_bytes)(void);     /* FLASH_OPTCR */
} factory_ops_t;

/*
 * Handle one request payload and build the reply payload.  Returns the reply
 * length, or 0 when the request is too short to answer.
 */
size_t factory_handle(const factory_ops_t *ops, const uint8_t *request,
                      size_t request_len, uint8_t *reply, size_t reply_capacity);

/* Drop any partly received write (call after a link reset). */
void factory_reset_session(void);

#endif /* FACTORY_PROTO_H */
