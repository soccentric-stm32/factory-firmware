/**
 * @file    factory_proto.c
 * @brief   Factory command handler (docs/FACTORY_SPEC.md section 5).
 * @details A write is staged in RAM (WRITE_BEGIN, WRITE_DATA, WRITE_END),
 *          checked against the file header and CRCs, then handed to
 *          flash_write_block as one erased and re-programmed 4 KB block.
 *          Nothing reaches flash before WRITE_END has validated the whole
 *          file.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-09
 * @target  STM32F767ZIT6 (also builds on the host for tests)
 * @srs     docs/FACTORY_SPEC.md section 5
 */

#include <string.h>
#include "factory_proto.h"

/** @brief One block write in progress. */
typedef struct {
    bool active;                    /**< true between BEGIN and END. */
    uint32_t address;               /**< Block address of the write. */
    uint16_t total;                 /**< Declared file length, bytes. */
    uint16_t received;              /**< Bytes staged so far. */
    uint32_t crc;                   /**< CRC-32 the host declared for the file. */
    uint8_t data[FACTORY_BLOCK_SIZE]; /**< Staging buffer for the block. */
} write_session_t;

static write_session_t session; /**< The single write session. */

/** @brief Little-endian u32 from the wire. */
static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

/** @brief Little-endian u16 from the wire. */
static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8U));
}

/** @brief Little-endian u32 to the wire. */
static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8U);
    p[2] = (uint8_t)(v >> 16U);
    p[3] = (uint8_t)(v >> 24U);
}

/** @brief Little-endian u16 to the wire. */
static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8U);
}

/**
 * @brief Check that a range lies inside the factory data window.
 * @param[in] address Logical address.
 * @param[in] length  Range length, bytes.
 * @return true when the whole range is writable.
 */
static bool in_window(uint32_t address, uint32_t length)
{
    return address >= FACTORY_WINDOW_BASE && length <= FACTORY_WINDOW_END &&
           address <= FACTORY_WINDOW_END - length;
}

/**
 * @brief Check for a block-aligned 4 KB block inside the window.
 * @param[in] address Logical address.
 * @return true when a block write may target @p address.
 */
static bool block_address_ok(uint32_t address)
{
    return in_window(address, FACTORY_BLOCK_SIZE) &&
           (address % FACTORY_BLOCK_SIZE) == 0U;
}

/**
 * @brief Write the two-byte command/status reply prefix.
 * @param[in]  cmd    Command being answered.
 * @param[in]  status FACTORY_OK or a FACTORY_ERR_* value.
 * @param[out] reply  Reply buffer.
 * @return Prefix length, 2.
 */
static size_t reply_status(uint8_t cmd, uint8_t status, uint8_t *reply)
{
    reply[0] = (uint8_t)(cmd | FACTORY_REPLY_FLAG);
    reply[1] = status;
    return 2U;
}

/**
 * @brief Append a length-prefixed string to the reply.
 * @param[out] out      Reply buffer.
 * @param[in]  at       Current write offset.
 * @param[in]  capacity Capacity of @p out, bytes.
 * @param[in]  text     NUL-terminated text.
 * @return New write offset, unchanged when the text does not fit.
 */
static size_t put_string(uint8_t *out, size_t at, size_t capacity, const char *text)
{
    size_t len = strlen(text);
    if (len > 255U || at + 1U + len > capacity) {
        return at;
    }
    out[at++] = (uint8_t)len;
    /* The wire format is length-prefixed, not NUL-terminated. */
    memcpy(&out[at], text, len); // NOLINT(bugprone-not-null-terminated-result)
    return at + len;
}

/** @brief Build the INFO reply: name, version, build and chip UID. */
static size_t handle_info(const factory_ops_t *ops, uint8_t *reply, size_t capacity)
{
    size_t n = reply_status(FACTORY_CMD_INFO, FACTORY_OK, reply);

    n = put_string(reply, n, capacity, ops->name);
    n = put_string(reply, n, capacity, ops->version);
    n = put_string(reply, n, capacity, ops->build);
    if (n + 12U <= capacity) {
        ops->chip_uid(&reply[n]);
        n += 12U;
    }
    return n;
}

/** @brief Handle READ: validate the range, copy from flash into the reply. */
static size_t handle_read(const factory_ops_t *ops, const uint8_t *req,
                          size_t req_len, uint8_t *reply, size_t capacity)
{
    if (req_len < 7U) {
        return reply_status(FACTORY_CMD_READ, FACTORY_ERR_BAD_COMMAND, reply);
    }
    uint32_t address = get_u32(&req[1]);
    uint16_t length = get_u16(&req[5]);

    if (length == 0U || length > FACTORY_MAX_CHUNK || 2U + length > capacity ||
        address < FACTORY_READ_BASE || address > FACTORY_WINDOW_END - length) {
        return reply_status(FACTORY_CMD_READ, FACTORY_ERR_BAD_RANGE, reply);
    }
    if (!ops->flash_read(address, &reply[2], length)) {
        return reply_status(FACTORY_CMD_READ, FACTORY_ERR_FLASH, reply);
    }
    reply[0] = FACTORY_CMD_READ | FACTORY_REPLY_FLAG;
    reply[1] = FACTORY_OK;
    return 2U + length;
}

/** @brief Handle WRITE_BEGIN: validate the block and start a new session. */
static size_t handle_write_begin(const uint8_t *req, size_t req_len, uint8_t *reply)
{
    if (req_len < 11U) {
        return reply_status(FACTORY_CMD_WRITE_BEGIN, FACTORY_ERR_BAD_COMMAND, reply);
    }
    uint32_t address = get_u32(&req[1]);
    uint16_t total = get_u16(&req[5]);
    uint32_t crc = get_u32(&req[7]);

    session.active = false;
    if (!block_address_ok(address) || total < FACTORY_FILE_HEADER_SIZE ||
        total > FACTORY_BLOCK_SIZE) {
        return reply_status(FACTORY_CMD_WRITE_BEGIN, FACTORY_ERR_BAD_RANGE, reply);
    }
    memset(session.data, 0xFF, sizeof(session.data));
    session.address = address;
    session.total = total;
    session.received = 0U;
    session.crc = crc;
    session.active = true;
    return reply_status(FACTORY_CMD_WRITE_BEGIN, FACTORY_OK, reply);
}

/** @brief Handle WRITE_DATA: stage one chunk, in order and in range. */
static size_t handle_write_data(const uint8_t *req, size_t req_len, uint8_t *reply)
{
    if (req_len < 4U) {
        return reply_status(FACTORY_CMD_WRITE_DATA, FACTORY_ERR_BAD_COMMAND, reply);
    }
    uint16_t offset = get_u16(&req[1]);
    size_t len = req_len - 3U;

    if (!session.active || offset > session.received) {
        return reply_status(FACTORY_CMD_WRITE_DATA, FACTORY_ERR_SEQUENCE, reply);
    }
    if (len > FACTORY_MAX_CHUNK || (size_t)offset + len > session.total) {
        return reply_status(FACTORY_CMD_WRITE_DATA, FACTORY_ERR_BAD_RANGE, reply);
    }
    memcpy(&session.data[offset], &req[3], len);
    if ((uint16_t)(offset + len) > session.received) {
        session.received = (uint16_t)(offset + len);
    }
    return reply_status(FACTORY_CMD_WRITE_DATA, FACTORY_OK, reply);
}

/**
 * @brief Validate the staged file header, both CRCs and the UID binding.
 * @param[in] ops Hardware hooks.
 * @return FACTORY_OK or the FACTORY_ERR_* value to report.
 */
static uint8_t check_file(const factory_ops_t *ops)
{
    const uint8_t *h = session.data;
    uint32_t magic = get_u32(&h[0]);

    if (magic != FACTORY_MAGIC_SYS && magic != FACTORY_MAGIC_CAL &&
        magic != FACTORY_MAGIC_DIAG) {
        return FACTORY_ERR_BAD_HEADER;
    }
    if (get_u16(&h[4]) != 1U ||
        get_u32(&h[8]) != (uint32_t)(session.total - FACTORY_FILE_HEADER_SIZE)) {
        return FACTORY_ERR_BAD_HEADER;
    }
    if (ops->crc32(h, 28U) != get_u32(&h[28])) {
        return FACTORY_ERR_BAD_HEADER;
    }
    if (ops->crc32(&h[FACTORY_FILE_HEADER_SIZE],
                   (size_t)(session.total - FACTORY_FILE_HEADER_SIZE)) !=
        get_u32(&h[12])) {
        return FACTORY_ERR_BAD_CRC;
    }

    static const uint8_t zero_uid[12] = { 0 };
    if (memcmp(&h[16], zero_uid, 12U) != 0) {
        uint8_t uid[12];
        ops->chip_uid(uid);
        if (memcmp(&h[16], uid, 12U) != 0) {
            return FACTORY_ERR_BAD_UID;
        }
    }
    return FACTORY_OK;
}

/**
 * @brief Write the staged block, read it back and report its CRC.
 * @param[in]  ops   Hardware hooks.
 * @param[in]  cmd   Command being answered.
 * @param[out] reply Reply buffer.
 * @return Reply length.
 */
static size_t program_block(const factory_ops_t *ops, uint8_t cmd, uint8_t *reply)
{
    static uint8_t block[FACTORY_BLOCK_SIZE];   /* too big for the task stack */
    uint8_t status = FACTORY_OK;

    bool ok = ops->flash_write_block(session.address, session.data);
    if (ok) {
        ok = ops->flash_read(session.address, block, FACTORY_BLOCK_SIZE) &&
             memcmp(block, session.data, FACTORY_BLOCK_SIZE) == 0;
    }
    if (!ok) {
        status = FACTORY_ERR_FLASH;
    }
    if (status != FACTORY_OK) {
        return reply_status(cmd, status, reply);
    }
    size_t n = reply_status(cmd, FACTORY_OK, reply);
    put_u32(&reply[n], ops->crc32(session.data, session.total));
    return n + 4U;
}

/** @brief Handle WRITE_END: check the sequence, the CRCs, then program. */
static size_t handle_write_end(const factory_ops_t *ops, uint8_t *reply)
{
    if (!session.active || session.received != session.total) {
        return reply_status(FACTORY_CMD_WRITE_END, FACTORY_ERR_SEQUENCE, reply);
    }
    session.active = false;
    if (ops->crc32(session.data, session.total) != session.crc) {
        return reply_status(FACTORY_CMD_WRITE_END, FACTORY_ERR_BAD_CRC, reply);
    }
    uint8_t status = check_file(ops);
    if (status != FACTORY_OK) {
        return reply_status(FACTORY_CMD_WRITE_END, status, reply);
    }
    return program_block(ops, FACTORY_CMD_WRITE_END, reply);
}

/**
 * @brief Format a clean .diag image into the staging buffer.
 * @param[in] ops Hardware hooks, for crc32().
 * @note A clean .diag is an empty fault log with the default log settings.
 */
static void build_clean_diag(const factory_ops_t *ops)
{
    static const uint8_t log_cfg[] = {
        0x03U, 0x04U, 0x00U,          /* type 3, length 4 */
        0x40U, 0x00U, 0x00U, 0x00U    /* depth 64, flags 0 */
    };
    uint8_t *h = session.data;

    memset(session.data, 0xFF, sizeof(session.data));
    memcpy(&h[FACTORY_FILE_HEADER_SIZE], log_cfg, sizeof(log_cfg));
    put_u32(&h[0], FACTORY_MAGIC_DIAG);
    put_u16(&h[4], 1U);
    put_u16(&h[6], 0U);
    put_u32(&h[8], (uint32_t)sizeof(log_cfg));
    put_u32(&h[12], ops->crc32(log_cfg, sizeof(log_cfg)));
    memset(&h[16], 0, 12U);
    put_u32(&h[28], ops->crc32(h, 28U));
    session.total = (uint16_t)(FACTORY_FILE_HEADER_SIZE + sizeof(log_cfg));
}

/** @brief Handle DIAG_INIT: build a clean .diag image and program it. */
static size_t handle_diag_init(const factory_ops_t *ops, const uint8_t *req,
                               size_t req_len, uint8_t *reply)
{
    if (req_len < 5U) {
        return reply_status(FACTORY_CMD_DIAG_INIT, FACTORY_ERR_BAD_COMMAND, reply);
    }
    uint32_t address = get_u32(&req[1]);

    session.active = false;
    if (!block_address_ok(address)) {
        return reply_status(FACTORY_CMD_DIAG_INIT, FACTORY_ERR_BAD_RANGE, reply);
    }
    session.address = address;
    build_clean_diag(ops);
    return program_block(ops, FACTORY_CMD_DIAG_INIT, reply);
}

/*
 * SELFTEST: run the basic peripheral checks and report each one as
 * id u8, result u8, value u32.  The RAM test borrows the write staging
 * buffer, so it is refused while a write is in progress.
 */
/**
 * @brief RAM test: fixed patterns, walking ones and an address pattern.
 * @param[in,out] buf Buffer to clobber.
 * @param[in]     len Length, bytes.
 * @return true when every pattern read back unchanged.
 */
static bool ram_walk(volatile uint8_t *buf, size_t len)
{
    static const uint8_t patterns[] = { 0x00U, 0xFFU, 0x55U, 0xAAU };

    for (size_t p = 0U; p < sizeof(patterns); p++) {
        for (size_t i = 0U; i < len; i++) {
            buf[i] = patterns[p];
        }
        for (size_t i = 0U; i < len; i++) {
            if (buf[i] != patterns[p]) {
                return false;
            }
        }
    }
    for (size_t bit = 0U; bit < 8U; bit++) {
        uint8_t walk = (uint8_t)(1U << bit);
        for (size_t i = 0U; i < len; i++) {
            buf[i] = walk;
        }
        for (size_t i = 0U; i < len; i++) {
            if (buf[i] != walk) {
                return false;
            }
        }
    }
    for (size_t i = 0U; i < len; i++) {
        buf[i] = (uint8_t)(i ^ (i >> 8U));
    }
    for (size_t i = 0U; i < len; i++) {
        if (buf[i] != (uint8_t)(i ^ (i >> 8U))) {
            return false;
        }
    }
    return true;
}

/** @brief Append one SELFTEST result: id, result, value. */
static size_t put_check(uint8_t *out, size_t at, uint8_t id, uint8_t result, uint32_t value)
{
    out[at] = id;
    out[at + 1U] = result;
    put_u32(&out[at + 2U], value);
    return at + 6U;
}

/** @brief Handle SELFTEST: run the checks and report each id/result/value. */
static size_t handle_selftest(const factory_ops_t *ops, uint8_t *reply, size_t capacity)
{
    static const uint8_t known[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    const size_t need = 3U + (size_t)FACTORY_CHECK_COUNT * 6U;
    uint8_t probe[32];
    uint32_t value = 0U;
    size_t n = 0U;

    if (session.active) {
        return reply_status(FACTORY_CMD_SELFTEST, FACTORY_ERR_SEQUENCE, reply);
    }
    if (capacity < need) {
        return reply_status(FACTORY_CMD_SELFTEST, FACTORY_ERR_BAD_RANGE, reply);
    }
    n = reply_status(FACTORY_CMD_SELFTEST, FACTORY_OK, reply);
    reply[n++] = (uint8_t)FACTORY_CHECK_COUNT;

    if (ops->device_id == NULL) {
        n = put_check(reply, n, FACTORY_CHECK_DEVICE_ID, FACTORY_CHECK_SKIP, 0U);
    } else {
        value = ops->device_id();
        n = put_check(reply, n, FACTORY_CHECK_DEVICE_ID,
                      (value & 0xFFFU) == FACTORY_DEVICE_ID ? FACTORY_CHECK_PASS : FACTORY_CHECK_FAIL,
                      value);
    }
    if (ops->flash_size_kb == NULL) {
        n = put_check(reply, n, FACTORY_CHECK_FLASH_SIZE, FACTORY_CHECK_SKIP, 0U);
    } else {
        value = ops->flash_size_kb();
        n = put_check(reply, n, FACTORY_CHECK_FLASH_SIZE,
                      value == FACTORY_FLASH_SIZE_KB ? FACTORY_CHECK_PASS : FACTORY_CHECK_FAIL,
                      value);
    }
    if (ops->core_clock_hz == NULL) {
        n = put_check(reply, n, FACTORY_CHECK_CLOCK, FACTORY_CHECK_SKIP, 0U);
    } else {
        value = ops->core_clock_hz();
        n = put_check(reply, n, FACTORY_CHECK_CLOCK,
                      value == FACTORY_CORE_CLOCK_HZ ? FACTORY_CHECK_PASS : FACTORY_CHECK_FAIL,
                      value);
    }
    n = put_check(reply, n, FACTORY_CHECK_RAM,
                  ram_walk(session.data, sizeof(session.data)) ? FACTORY_CHECK_PASS
                                                               : FACTORY_CHECK_FAIL,
                  (uint32_t)sizeof(session.data));
    value = ops->crc32(known, sizeof(known));
    n = put_check(reply, n, FACTORY_CHECK_CRC,
                  value == 0xCBF43926UL ? FACTORY_CHECK_PASS : FACTORY_CHECK_FAIL, value);
    n = put_check(reply, n, FACTORY_CHECK_FLASH_READ,
                  ops->flash_read(FACTORY_READ_BASE, probe, (uint16_t)sizeof(probe))
                      ? FACTORY_CHECK_PASS
                      : FACTORY_CHECK_FAIL,
                  0U);
    if (ops->option_bytes == NULL) {
        n = put_check(reply, n, FACTORY_CHECK_OPTION, FACTORY_CHECK_SKIP, 0U);
    } else {
        value = ops->option_bytes();
        /* nDBANK (bit 29) clear is dual-bank; RDP byte 0xCC is level 2. */
        bool dual_bank = (value & (1UL << 29U)) == 0U;
        bool rdp_ok = ((value >> 8U) & 0xFFU) != 0xCCU;
        n = put_check(reply, n, FACTORY_CHECK_OPTION,
                      (dual_bank && rdp_ok) ? FACTORY_CHECK_PASS : FACTORY_CHECK_FAIL, value);
    }
    return n;
}

size_t factory_handle(const factory_ops_t *ops, const uint8_t *request,
                      size_t request_len, uint8_t *reply, size_t reply_capacity)
{
    if (ops == NULL || request == NULL || reply == NULL || request_len == 0U ||
        reply_capacity < 2U) {
        return 0U;
    }

    switch (request[0]) {
    case FACTORY_CMD_INFO:
        return handle_info(ops, reply, reply_capacity);
    case FACTORY_CMD_READ:
        return handle_read(ops, request, request_len, reply, reply_capacity);
    case FACTORY_CMD_WRITE_BEGIN:
        return handle_write_begin(request, request_len, reply);
    case FACTORY_CMD_WRITE_DATA:
        return handle_write_data(request, request_len, reply);
    case FACTORY_CMD_WRITE_END:
        return handle_write_end(ops, reply);
    case FACTORY_CMD_DIAG_INIT:
        return handle_diag_init(ops, request, request_len, reply);
    case FACTORY_CMD_SELFTEST:
        return handle_selftest(ops, reply, reply_capacity);
    default:
        return reply_status(request[0], FACTORY_ERR_BAD_COMMAND, reply);
    }
}

void factory_reset_session(void)
{
    session.active = false;
    session.received = 0U;
}
