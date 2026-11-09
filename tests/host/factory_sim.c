/*
 * factory_sim.c - factory_proto.c against a simulated identity sector
 *
 * Author:  Sandesh Ghimire <sandesh@soccentric.com>
 * Date:    2026-11-09
 *
 * Built as a shared library so the station's Python tests can drive the real
 * firmware command handler (tests/host/Makefile, factory-station tests).
 */

#include <string.h>
#include <stdint.h>
#include "../../src/factory_proto.h"

#define SIM_BASE   0x08104000UL
#define SIM_SIZE   0x4000UL

static uint8_t flash[SIM_SIZE];
static uint8_t sim_uid[12] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
static int fail_next_write;

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

static bool sim_read(uint32_t address, uint8_t *out, uint16_t len)
{
    if (address < SIM_BASE || address + len > SIM_BASE + SIM_SIZE) {
        return false;
    }
    memcpy(out, &flash[address - SIM_BASE], len);
    return true;
}

static bool sim_write_block(uint32_t address, const uint8_t *data)
{
    if (fail_next_write) {
        fail_next_write = 0;
        return false;
    }
    if (address < SIM_BASE || address + FACTORY_BLOCK_SIZE > SIM_BASE + SIM_SIZE) {
        return false;
    }
    memcpy(&flash[address - SIM_BASE], data, FACTORY_BLOCK_SIZE);
    return true;
}

static void sim_chip_uid(uint8_t uid[12])
{
    memcpy(uid, sim_uid, 12);
}

static const factory_ops_t ops = {
    sim_read, sim_write_block, sim_chip_uid, crc32_sw,
    "soccentric-factory-firmware", "0.1.0", "host-sim",
};

void sim_reset(void)
{
    memset(flash, 0xFF, sizeof(flash));
    factory_reset_session();
    fail_next_write = 0;
}

void sim_fail_next_write(void)
{
    fail_next_write = 1;
}

size_t sim_handle(const uint8_t *req, size_t req_len, uint8_t *reply, size_t cap)
{
    return factory_handle(&ops, req, req_len, reply, cap);
}

void sim_poke(uint32_t address, uint8_t value)
{
    flash[address - SIM_BASE] = value;
}
