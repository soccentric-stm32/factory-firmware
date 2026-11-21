/**
 * @file    factory_sim.c
 * @brief   factory_proto.c against a simulated identity sector.
 * @details Built as a shared library so the station's Python tests can drive
 *          the real firmware command handler (tests/host/Makefile,
 *          factory-station tests).
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-09
 * @target  host (x86-64)
 * @srs     docs/FACTORY_SPEC.md section 5
 */

#include <string.h>
#include <stdint.h>
#include "factory_fb.h"
#include "factory_proto.h"

#define SIM_BASE   0x08104000UL /**< Base of the simulated sector. */
#define SIM_SIZE   0x4000UL     /**< Size of the simulated sector, bytes. */

static uint8_t flash[SIM_SIZE];              /**< Simulated sector. */
static uint8_t sim_uid[12] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 }; /**< Fake UID. */
static int fail_next_write;                  /**< Fault injection flag. */

/** @brief Reference CRC-32/ISO-HDLC, independent of the firmware copy. */
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

/** @brief flash_read hook: bounds-checked copy out of the simulated sector. */
static bool sim_read(uint32_t address, uint8_t *out, uint16_t len)
{
    if (address < SIM_BASE || address + len > SIM_BASE + SIM_SIZE) {
        return false;
    }
    memcpy(out, &flash[address - SIM_BASE], len);
    return true;
}

/** @brief flash_write_block hook; can fail once on demand. */
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

/** @brief chip_uid hook. */
static void sim_chip_uid(uint8_t uid[12])
{
    memcpy(uid, sim_uid, 12);
}

static uint32_t sim_idcode = 0x10006451UL;     /* REV_ID 0x1000, DEV_ID 0x451 */
static uint32_t sim_flash_kb = 2048UL;
static uint32_t sim_clock_hz = 216000000UL;
static uint32_t sim_optcr = 0x0FFFAAE4UL;      /* nDBANK = 0, RDP level 0 */

/** @brief SELFTEST readers; sim_set_hardware() can change what they report. */
static uint32_t sim_device_id(void) { return sim_idcode; }
static uint32_t sim_flash_size_kb(void) { return sim_flash_kb; }
static uint32_t sim_core_clock_hz(void) { return sim_clock_hz; }
static uint32_t sim_option_bytes(void) { return sim_optcr; }

/** @brief Operations table backed by the simulation. */
static const factory_ops_t ops = {
    sim_read, sim_write_block, sim_chip_uid, crc32_sw,
    "soccentric-factory-firmware", "0.1.0", "host-sim",
    sim_device_id, sim_flash_size_kb, sim_core_clock_hz, sim_option_bytes,
};

/** @brief Erase the simulated sector and restore the default hardware. */
void sim_reset(void)
{
    memset(flash, 0xFF, sizeof(flash));
    factory_reset_session();
    fail_next_write = 0;
    sim_idcode = 0x10006451UL;
    sim_flash_kb = 2048UL;
    sim_clock_hz = 216000000UL;
    sim_optcr = 0x0FFFAAE4UL;
}

/**
 * @brief Change what the simulated hardware readers report (fault injection).
 * @param[in] idcode   DBGMCU_IDCODE value.
 * @param[in] flash_kb Flash size, KB.
 * @param[in] clock_hz System clock, Hz.
 * @param[in] optcr    FLASH_OPTCR value.
 */
void sim_set_hardware(uint32_t idcode, uint32_t flash_kb, uint32_t clock_hz, uint32_t optcr)
{
    sim_idcode = idcode;
    sim_flash_kb = flash_kb;
    sim_clock_hz = clock_hz;
    sim_optcr = optcr;
}

/** @brief Make the next flash_write_block() call fail. */
void sim_fail_next_write(void)
{
    fail_next_write = 1;
}

/**
 * @brief Run the firmware command handler against the simulation.
 * @return Reply length, 0 when the request cannot be answered.
 */
size_t sim_handle(const uint8_t *req, size_t req_len, uint8_t *reply, size_t cap)
{
    return factory_handle(&ops, req, req_len, reply, cap);
}

/**
 * @brief FlatBuffers entry point used by the station's cross-language tests.
 * @return Reply length, 0 when the request cannot be answered.
 */
size_t sim_fb_handle(const uint8_t *req, size_t req_len, uint8_t *reply,
                     size_t cap, uint16_t seq)
{
    return factory_fb_handle(&ops, req, req_len, reply, cap, seq);
}

/**
 * @brief Write one byte into the simulated sector, to corrupt a record.
 * @param[in] address Address inside SIM_BASE..SIM_BASE+SIM_SIZE.
 * @param[in] value   Byte to store.
 */
void sim_poke(uint32_t address, uint8_t value)
{
    flash[address - SIM_BASE] = value;
}
