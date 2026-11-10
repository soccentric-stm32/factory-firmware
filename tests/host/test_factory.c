/*
 * test_factory.c - host tests for the factory command handler
 *
 * Author:  Sandesh Ghimire <sandesh@soccentric.com>
 * Date:    2026-11-09
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/factory_proto.h"

void sim_reset(void);
void sim_fail_next_write(void);
void sim_set_hardware(uint32_t, uint32_t, uint32_t, uint32_t);
size_t sim_handle(const uint8_t *, size_t, uint8_t *, size_t);

static int failures;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static uint32_t crc32_sw(const void *data, size_t len)
{
    const uint8_t *p = data;
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
        }
    }
    return ~crc;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static size_t make_file(uint8_t *out, uint32_t magic, const uint8_t *payload,
                        size_t len, const uint8_t *uid)
{
    memset(out, 0, 32);
    put32(&out[0], magic);
    out[4] = 1;
    put32(&out[8], (uint32_t)len);
    memcpy(&out[32], payload, len);
    put32(&out[12], crc32_sw(payload, len));
    if (uid != NULL) {
        memcpy(&out[16], uid, 12);
    }
    put32(&out[28], crc32_sw(out, 28));
    return 32 + len;
}

static uint8_t rpl[1024];

static uint8_t send(const uint8_t *req, size_t len)
{
    size_t n = sim_handle(req, len, rpl, sizeof(rpl));
    return n >= 2 ? rpl[1] : 0xFF;
}

static uint8_t write_file(uint32_t address, const uint8_t *file, size_t total,
                          uint32_t declared_crc)
{
    uint8_t req[600];
    req[0] = FACTORY_CMD_WRITE_BEGIN;
    put32(&req[1], address);
    req[5] = (uint8_t)total; req[6] = (uint8_t)(total >> 8);
    put32(&req[7], declared_crc);
    uint8_t st = send(req, 11);
    if (st != FACTORY_OK) {
        return st;
    }
    for (size_t off = 0; off < total; off += 200) {
        size_t n = total - off < 200 ? total - off : 200;
        req[0] = FACTORY_CMD_WRITE_DATA;
        req[1] = (uint8_t)off; req[2] = (uint8_t)(off >> 8);
        memcpy(&req[3], &file[off], n);
        st = send(req, 3 + n);
        if (st != FACTORY_OK) {
            return st;
        }
    }
    req[0] = FACTORY_CMD_WRITE_END;
    return send(req, 1);
}

/* Result of check `id` in the last SELFTEST reply, or 0xFF when absent. */
static uint8_t check_result(uint8_t id)
{
    for (size_t at = 3; at + 6 <= 3 + (size_t)rpl[2] * 6; at += 6) {
        if (rpl[at] == id) {
            return rpl[at + 1];
        }
    }
    return 0xFF;
}

static void test_selftest(void)
{
    uint8_t req[] = { FACTORY_CMD_SELFTEST };
    uint8_t begin[11] = { FACTORY_CMD_WRITE_BEGIN, 0, 0x50, 0x10, 0x08, 32, 0, 0, 0, 0, 0 };

    sim_reset();
    CHECK(send(req, 1) == FACTORY_OK);
    CHECK(rpl[0] == (FACTORY_CMD_SELFTEST | 0x80));
    CHECK(rpl[2] == FACTORY_CHECK_COUNT);
    for (uint8_t id = 1; id <= FACTORY_CHECK_COUNT; id++) {
        CHECK(check_result(id) == FACTORY_CHECK_PASS);
    }
    /* The CRC known-answer value is reported. */
    for (size_t at = 3; at + 6 <= 3 + (size_t)rpl[2] * 6; at += 6) {
        if (rpl[at] == FACTORY_CHECK_CRC) {
            CHECK(rpl[at + 2] == 0x26 && rpl[at + 3] == 0x39 &&
                  rpl[at + 4] == 0xF4 && rpl[at + 5] == 0xCB);
        }
    }

    sim_set_hardware(0x10006450UL, 1024UL, 168000000UL, 0x2FFFAAE4UL);
    CHECK(send(req, 1) == FACTORY_OK);
    CHECK(check_result(FACTORY_CHECK_DEVICE_ID) == FACTORY_CHECK_FAIL);
    CHECK(check_result(FACTORY_CHECK_FLASH_SIZE) == FACTORY_CHECK_FAIL);
    CHECK(check_result(FACTORY_CHECK_CLOCK) == FACTORY_CHECK_FAIL);
    CHECK(check_result(FACTORY_CHECK_OPTION) == FACTORY_CHECK_FAIL);
    CHECK(check_result(FACTORY_CHECK_RAM) == FACTORY_CHECK_PASS);

    /* RDP level 2 is reported as a failure even when the bank mode is right. */
    sim_set_hardware(0x10006451UL, 2048UL, 216000000UL, 0x0FFFCCE4UL);
    CHECK(send(req, 1) == FACTORY_OK);
    CHECK(check_result(FACTORY_CHECK_OPTION) == FACTORY_CHECK_FAIL);

    /* Refused while a write is in progress (the RAM test needs the buffer). */
    sim_reset();
    CHECK(send(begin, 11) == FACTORY_OK);
    CHECK(send(req, 1) == FACTORY_ERR_SEQUENCE);
    factory_reset_session();
    CHECK(send(req, 1) == FACTORY_OK);
}

int main(void)
{
    uint8_t file[512];
    const uint8_t payload[] = { 0x01, 4, 0, 'S', 'N', '0', '1' };
    uint8_t uid[12] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    uint8_t bad_uid[12] = { 9 };
    size_t total;

    sim_reset();
    uint8_t info[] = { FACTORY_CMD_INFO };
    CHECK(send(info, 1) == FACTORY_OK);
    CHECK(rpl[0] == (FACTORY_CMD_INFO | 0x80));
    CHECK(rpl[2] == 27 && memcmp(&rpl[3], "soccentric-factory-firmware", 27) == 0);

    total = make_file(file, FACTORY_MAGIC_SYS, payload, sizeof(payload), NULL);
    CHECK(write_file(0x08105000UL, file, total, crc32_sw(file, total)) == FACTORY_OK);
    CHECK(rpl[2] == (uint8_t)crc32_sw(file, total));

    uint8_t read[] = { FACTORY_CMD_READ, 0, 0x50, 0x10, 0x08, 0, 0 };
    read[5] = (uint8_t)total;
    CHECK(send(read, 7) == FACTORY_OK);
    CHECK(memcmp(&rpl[2], file, total) == 0);

    /* Wrong declared CRC. */
    CHECK(write_file(0x08105000UL, file, total, 0x1234) == FACTORY_ERR_BAD_CRC);
    /* Address outside the window and unaligned. */
    CHECK(write_file(0x08104000UL, file, total, crc32_sw(file, total)) == FACTORY_ERR_BAD_RANGE);
    CHECK(write_file(0x08105100UL, file, total, crc32_sw(file, total)) == FACTORY_ERR_BAD_RANGE);
    CHECK(write_file(0x08108000UL, file, total, crc32_sw(file, total)) == FACTORY_ERR_BAD_RANGE);
    /* Header corrupted: magic wrong. */
    memcpy(file, "XXXX", 4);
    CHECK(write_file(0x08106000UL, file, total, crc32_sw(file, total)) == FACTORY_ERR_BAD_HEADER);
    /* UID binding. */
    total = make_file(file, FACTORY_MAGIC_CAL, payload, sizeof(payload), uid);
    CHECK(write_file(0x08106000UL, file, total, crc32_sw(file, total)) == FACTORY_OK);
    total = make_file(file, FACTORY_MAGIC_CAL, payload, sizeof(payload), bad_uid);
    CHECK(write_file(0x08106000UL, file, total, crc32_sw(file, total)) == FACTORY_ERR_BAD_UID);
    /* Payload CRC broken (header CRC recomputed so only the payload fails). */
    total = make_file(file, FACTORY_MAGIC_CAL, payload, sizeof(payload), NULL);
    file[34] ^= 0xFF;
    CHECK(write_file(0x08106000UL, file, total, crc32_sw(file, total)) == FACTORY_ERR_BAD_CRC);
    /* Flash failure surfaces. */
    total = make_file(file, FACTORY_MAGIC_SYS, payload, sizeof(payload), NULL);
    sim_fail_next_write();
    CHECK(write_file(0x08105000UL, file, total, crc32_sw(file, total)) == FACTORY_ERR_FLASH);
    /* Out-of-order chunk and early WRITE_END. */
    uint8_t begin[11] = { FACTORY_CMD_WRITE_BEGIN, 0, 0x50, 0x10, 0x08, 64, 0, 0, 0, 0, 0 };
    CHECK(send(begin, 11) == FACTORY_OK);
    uint8_t chunk[8] = { FACTORY_CMD_WRITE_DATA, 40, 0, 1, 2, 3, 4, 5 };
    CHECK(send(chunk, 8) == FACTORY_ERR_SEQUENCE);
    uint8_t end[1] = { FACTORY_CMD_WRITE_END };
    CHECK(send(end, 1) == FACTORY_ERR_SEQUENCE);

    /* DIAG_INIT writes a valid clean map. */
    uint8_t init[5] = { FACTORY_CMD_DIAG_INIT, 0, 0x70, 0x10, 0x08 };
    CHECK(send(init, 5) == FACTORY_OK);
    uint8_t rd[7] = { FACTORY_CMD_READ, 0, 0x70, 0x10, 0x08, 64, 0 };
    CHECK(send(rd, 7) == FACTORY_OK);
    CHECK(memcmp(&rpl[2], "DIAG", 4) == 0);
    uint8_t bad_init[5] = { FACTORY_CMD_DIAG_INIT, 0, 0x71, 0x10, 0x08 };
    CHECK(send(bad_init, 5) == FACTORY_ERR_BAD_RANGE);

    /* Unknown command and out-of-window read. */
    uint8_t unknown[1] = { 0x7F };
    CHECK(send(unknown, 1) == FACTORY_ERR_BAD_COMMAND);
    uint8_t rd_bad[7] = { FACTORY_CMD_READ, 0, 0x00, 0x00, 0x00, 4, 0 };
    CHECK(send(rd_bad, 7) == FACTORY_ERR_BAD_RANGE);

    test_selftest();

    if (failures == 0) {
        printf("factory: all tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
