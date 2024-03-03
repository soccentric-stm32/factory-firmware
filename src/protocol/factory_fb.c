/*
 * factory_fb.c - FACTORY messages over HWLINK FlatBuffers (protocol minor 4)
 *
 * Author:  Sandesh Ghimire <sandesh@soccentric.com>
 * Date:    2026-11-19
 * Target:  STM32F767ZIT6 (also builds on the host for tests)
 *
 * This file contains a small, bounded FlatBuffers encoder and decoder for the
 * FACTORY message set.  It exists so the firmware can migrate to HWLINK
 * FlatBuffers without regenerating the flatcc headers on every bench; the
 * generated HWLINK tables are still used for all existing messages.  The
 * encoder builds a standard FlatBuffer (root uoffset, HWLK identifier,
 * back-to-front vtables) that the Python generated accessors and flatcc read
 * unchanged.
 *
 * Command semantics stay in factory_proto.c: this module packs the request
 * into the legacy binary command, calls factory_handle() and unpacks the
 * reply into the new message.  That keeps one implementation of the header,
 * CRC and UID checks for both protocols.
 */

#include <string.h>
#include "factory_fb.h"
#include "hwlink.h"

#define FB_ARENA_SIZE      2048U
#define FB_MAX_FIELDS      16U
#define FB_RAW_SIZE        (FACTORY_MAX_CHUNK + 32U)

/* One builder arena serves every reply helper: factory_fb_handle() runs to
 * completion inside the comms task, so two builders are never live at the
 * same time.  A per-function arena would multiply this buffer by seven and
 * blow the RAM budget. */
static uint8_t fb_arena[FB_ARENA_SIZE] __attribute__((aligned(8)));

/* hwlink Error values used by this module (protocol/hwlink.fbs). */
#define FB_ERR_NONE          0U
#define FB_ERR_MALFORMED     1U
#define FB_ERR_UNSUPPORTED   2U
#define FB_ERR_BAD_ARGUMENT  3U
#define FB_ERR_BAD_STATE     4U
#define FB_ERR_BAD_HEADER    7U
#define FB_ERR_FLASH_ERROR   11U
#define FB_ERR_CRC_MISMATCH  12U
#define FB_ERR_BAD_RANGE     16U
#define FB_ERR_BAD_UID       17U

/* hwlink Role.FACTORY / CheckResult / FactoryKind */
#define FB_ROLE_FACTORY      4U
#define FB_CHECK_PASS        0U
#define FB_CHECK_FAIL        1U
#define FB_CHECK_SKIP        2U
#define FB_KIND_SYS          1U
#define FB_KIND_CAL          2U
#define FB_KIND_DIAG         3U

/* ---------------------------------------------------------------- builder */

typedef struct {
    uint8_t *buf;
    size_t size;
    size_t head;
    int failed;
} fbb_t;

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static uint16_t rd_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8U));
}

static void wr_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8U);
}

static void wr_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8U);
    p[2] = (uint8_t)(v >> 16U);
    p[3] = (uint8_t)(v >> 24U);
}

static void fbb_init(fbb_t *b, uint8_t *buf, size_t size)
{
    b->buf = buf;
    b->size = size;
    b->head = size;
    b->failed = 0;
    memset(buf, 0, size);
}

static size_t fbb_place(fbb_t *b, size_t n, size_t align)
{
    if (b->failed || n == 0U || align == 0U) {
        b->failed = 1;
        return 0U;
    }
    if (b->head < n) {
        b->failed = 1;
        return 0U;
    }
    size_t pos = (b->head - n) & ~(align - 1U);
    if (pos + n > b->size) {
        b->failed = 1;
        return 0U;
    }
    b->head = pos;
    return pos;
}

static size_t fbb_string(fbb_t *b, const char *text)
{
    size_t len = text != NULL ? strlen(text) : 0U;
    size_t pos = fbb_place(b, 4U + len + 1U, 4U);
    if (b->failed) {
        return 0U;
    }
    wr_u32(&b->buf[pos], (uint32_t)len);
    if (len > 0U) {
        memcpy(&b->buf[pos + 4U], text, len);
    }
    b->buf[pos + 4U + len] = 0U;
    return pos;
}

static size_t fbb_bytes(fbb_t *b, const uint8_t *data, size_t len)
{
    size_t pos = fbb_place(b, 4U + len, 4U);
    if (b->failed) {
        return 0U;
    }
    wr_u32(&b->buf[pos], (uint32_t)len);
    if (len > 0U) {
        memcpy(&b->buf[pos + 4U], data, len);
    }
    return pos;
}

typedef struct {
    uint8_t field;
    uint8_t size;
    size_t pos;
} fb_field_t;

typedef struct {
    fbb_t *b;
    size_t table_pos;
    int count;
    fb_field_t items[FB_MAX_FIELDS];
} fbt_t;

static void fbt_start(fbt_t *t, fbb_t *b, int count)
{
    t->b = b;
    t->table_pos = 0U;
    t->count = count > (int)FB_MAX_FIELDS ? (int)FB_MAX_FIELDS : count;
    memset(t->items, 0, sizeof(t->items));
}

static void fbt_scalar(fbt_t *t, uint8_t field, uint8_t size, uint32_t value)
{
    if (t->count <= 0 || t->b->failed) {
        return;
    }
    size_t pos = fbb_place(t->b, size, size);
    if (t->b->failed) {
        return;
    }
    for (int i = 0; i < t->count; i++) {
        if (t->items[i].size == 0U) {
            t->items[i].field = field;
            t->items[i].size = size;
            t->items[i].pos = pos;
            break;
        }
    }
    if (size == 1U) {
        t->b->buf[pos] = (uint8_t)value;
    } else if (size == 2U) {
        wr_u16(&t->b->buf[pos], (uint16_t)value);
    } else if (size == 4U) {
        wr_u32(&t->b->buf[pos], value);
    } else if (size == 8U) {
        wr_u32(&t->b->buf[pos], value);
        wr_u32(&t->b->buf[pos + 4U], 0U);
    }
}

static void fbt_u8(fbt_t *t, uint8_t field, uint8_t value)
{
    fbt_scalar(t, field, 1U, value);
}

static void fbt_u16(fbt_t *t, uint8_t field, uint16_t value)
{
    fbt_scalar(t, field, 2U, value);
}

static void fbt_u32(fbt_t *t, uint8_t field, uint32_t value)
{
    fbt_scalar(t, field, 4U, value);
}

static void fbt_bool(fbt_t *t, uint8_t field, bool value)
{
    fbt_scalar(t, field, 1U, value ? 1U : 0U);
}

static void fbt_u64(fbt_t *t, uint8_t field, uint64_t value)
{
    if (t->count <= 0 || t->b->failed) {
        return;
    }
    size_t pos = fbb_place(t->b, 8U, 8U);
    if (t->b->failed) {
        return;
    }
    for (int i = 0; i < t->count; i++) {
        if (t->items[i].size == 0U) {
            t->items[i].field = field;
            t->items[i].size = 8U;
            t->items[i].pos = pos;
            break;
        }
    }
    wr_u32(&t->b->buf[pos], (uint32_t)value);
    wr_u32(&t->b->buf[pos + 4U], (uint32_t)(value >> 32U));
}

static void fbt_offset(fbt_t *t, uint8_t field, size_t target)
{
    if (target == 0U || t->count <= 0 || t->b->failed) {
        return;
    }
    size_t pos = fbb_place(t->b, 4U, 4U);
    if (t->b->failed) {
        return;
    }
    for (int i = 0; i < t->count; i++) {
        if (t->items[i].size == 0U) {
            t->items[i].field = field;
            t->items[i].size = 4U;
            t->items[i].pos = pos;
            break;
        }
    }
    wr_u32(&t->b->buf[pos], (uint32_t)(target - pos));
}

static size_t fbt_end(fbt_t *t)
{
    fbb_t *b = t->b;
    if (b->failed || t->count <= 0) {
        return 0U;
    }

    size_t table_pos = fbb_place(b, 4U, 4U);
    if (b->failed) {
        return 0U;
    }

    uint16_t object_size = 4U;
    for (int i = 0; i < t->count; i++) {
        if (t->items[i].size == 0U) {
            continue;
        }
        uint16_t end = (uint16_t)(t->items[i].pos + t->items[i].size - table_pos);
        if (end > object_size) {
            object_size = end;
        }
    }

    uint16_t vt_size = (uint16_t)(4U + 2U * (uint16_t)t->count);
    size_t vt_pos = fbb_place(b, vt_size, 2U);
    if (b->failed) {
        return 0U;
    }
    wr_u16(&b->buf[vt_pos], vt_size);
    wr_u16(&b->buf[vt_pos + 2U], object_size);
    for (int i = 0; i < t->count; i++) {
        if (t->items[i].size == 0U) {
            continue;
        }
        uint16_t off = (uint16_t)(t->items[i].pos - table_pos);
        wr_u16(&b->buf[vt_pos + 4U + 2U * (uint16_t)t->items[i].field], off);
    }
    wr_u32(&b->buf[table_pos], (uint32_t)(table_pos - vt_pos));
    return table_pos;
}

static size_t fbb_vector_tables(fbb_t *b, const size_t *tables, size_t count)
{
    size_t pos = fbb_place(b, 4U + 4U * count, 4U);
    if (b->failed) {
        return 0U;
    }
    wr_u32(&b->buf[pos], (uint32_t)count);
    for (size_t i = 0U; i < count; i++) {
        size_t at = pos + 4U + 4U * i;
        wr_u32(&b->buf[at], (uint32_t)(tables[i] - at));
    }
    return pos;
}

static size_t fbb_finish(fbb_t *b, uint8_t body_type, size_t body,
                         uint8_t *out, size_t out_capacity)
{
    if (b->failed || body == 0U) {
        return 0U;
    }
    fbt_t message;
    fbt_start(&message, b, 2);
    fbt_u8(&message, 0U, body_type);
    fbt_offset(&message, 1U, body);
    size_t root_table = fbt_end(&message);
    if (root_table == 0U) {
        return 0U;
    }

    size_t root = fbb_place(b, 8U, 4U);
    if (b->failed) {
        return 0U;
    }
    wr_u32(&b->buf[root], (uint32_t)(root_table - root));
    memcpy(&b->buf[root + 4U], "HWLK", 4U);

    size_t total = b->size - root;
    if (total > out_capacity) {
        return 0U;
    }
    memcpy(out, &b->buf[root], total);
    return total;
}

/* ---------------------------------------------------------------- reader */

typedef struct {
    const uint8_t *base;
    const uint8_t *end;
} fbr_t;

static bool fbr_range(const fbr_t *r, const uint8_t *p, size_t n)
{
    return p >= r->base && p <= r->end && n <= (size_t)(r->end - p);
}

static const uint8_t *fbr_root(const fbr_t *r)
{
    if (!fbr_range(r, r->base, 8U)) {
        return NULL;
    }
    uint32_t off = rd_u32(r->base);
    if (off < 8U || off > (uint32_t)(r->end - r->base)) {
        return NULL;
    }
    return r->base + off;
}

static const uint8_t *fbr_field(const fbr_t *r, const uint8_t *table,
                                uint8_t field)
{
    if (!fbr_range(r, table, 4U)) {
        return NULL;
    }
    int32_t soffset = (int32_t)rd_u32(table);
    const uint8_t *vt = table - soffset;
    if (!fbr_range(r, vt, 4U)) {
        return NULL;
    }
    uint16_t vt_size = rd_u16(vt);
    if (vt_size < 4U || 4U + 2U * (uint16_t)field + 2U > vt_size) {
        return NULL;
    }
    uint16_t off = rd_u16(vt + 4U + 2U * field);
    if (off == 0U) {
        return NULL;
    }
    const uint8_t *p = table + off;
    if (!fbr_range(r, p, 1U)) {
        return NULL;
    }
    return p;
}

static bool fbr_u8(const fbr_t *r, const uint8_t *table, uint8_t field, uint8_t *out)
{
    const uint8_t *p = fbr_field(r, table, field);
    if (p == NULL) {
        *out = 0U;   /* FlatBuffers omits a field whose value is the default. */
        return true;
    }
    *out = *p;
    return true;
}

static bool fbr_u16(const fbr_t *r, const uint8_t *table, uint8_t field, uint16_t *out)
{
    const uint8_t *p = fbr_field(r, table, field);
    if (p == NULL) {
        *out = 0U;
        return true;
    }
    if (!fbr_range(r, p, 2U)) {
        return false;
    }
    *out = rd_u16(p);
    return true;
}

static bool fbr_u32(const fbr_t *r, const uint8_t *table, uint8_t field, uint32_t *out)
{
    const uint8_t *p = fbr_field(r, table, field);
    if (p == NULL) {
        *out = 0U;
        return true;
    }
    if (!fbr_range(r, p, 4U)) {
        return false;
    }
    *out = rd_u32(p);
    return true;
}

static const uint8_t *fbr_table(const fbr_t *r, const uint8_t *table, uint8_t field)
{
    const uint8_t *p = fbr_field(r, table, field);
    if (p == NULL || !fbr_range(r, p, 4U)) {
        return NULL;
    }
    uint32_t off = rd_u32(p);
    const uint8_t *target = p + off;
    if (off == 0U || !fbr_range(r, target, 4U)) {
        return NULL;
    }
    return target;
}

static bool fbr_vector(const fbr_t *r, const uint8_t *table, uint8_t field,
                       const uint8_t **data, uint32_t *len)
{
    const uint8_t *p = fbr_field(r, table, field);
    if (p == NULL || !fbr_range(r, p, 4U)) {
        return false;
    }
    uint32_t off = rd_u32(p);
    const uint8_t *target = p + off;
    if (off == 0U || !fbr_range(r, target, 4U)) {
        return false;
    }
    uint32_t count = rd_u32(target);
    if (!fbr_range(r, target + 4U, count)) {
        return false;
    }
    *data = target + 4U;
    *len = count;
    return true;
}

/* ------------------------------------------------------------- replies */

static size_t fb_ack(uint8_t *reply, size_t capacity, uint16_t seq, uint16_t body_type)
{
    fbb_t b;
    fbb_init(&b, fb_arena, sizeof(fb_arena));
    fbt_t t;
    fbt_start(&t, &b, 2);
    fbt_u16(&t, 0U, seq);
    fbt_u32(&t, 1U, 0U);
    size_t body = fbt_end(&t);
    return fbb_finish(&b, body_type, body, reply, capacity);
}

static size_t fb_nak(uint8_t *reply, size_t capacity, uint16_t seq,
                     uint16_t error, const char *detail)
{
    fbb_t b;
    fbb_init(&b, fb_arena, sizeof(fb_arena));
    size_t text = fbb_string(&b, detail != NULL ? detail : "");
    fbt_t t;
    fbt_start(&t, &b, 4);
    fbt_u16(&t, 0U, seq);
    fbt_u16(&t, 1U, error);
    fbt_offset(&t, 2U, text);
    fbt_u32(&t, 3U, 0U);
    size_t body = fbt_end(&t);
    return fbb_finish(&b, 7U /* Body.Nak */, body, reply, capacity);
}

static uint16_t map_error(uint8_t status)
{
    switch (status) {
    case FACTORY_ERR_BAD_COMMAND:
        return FB_ERR_UNSUPPORTED;
    case FACTORY_ERR_BAD_RANGE:
        return FB_ERR_BAD_RANGE;
    case FACTORY_ERR_BAD_HEADER:
        return FB_ERR_BAD_HEADER;
    case FACTORY_ERR_BAD_CRC:
        return FB_ERR_CRC_MISMATCH;
    case FACTORY_ERR_BAD_UID:
        return FB_ERR_BAD_UID;
    case FACTORY_ERR_FLASH:
        return FB_ERR_FLASH_ERROR;
    case FACTORY_ERR_SEQUENCE:
        return FB_ERR_BAD_STATE;
    default:
        return FB_ERR_UNSUPPORTED;
    }
}

static size_t fb_hello_reply(const factory_ops_t *ops, uint8_t *reply,
                             size_t capacity, uint16_t seq)
{
    uint8_t uid[12];
    fbb_t b;
    fbb_init(&b, fb_arena, sizeof(fb_arena));
    ops->chip_uid(uid);
    uint64_t uid64 = (uint64_t)rd_u32(uid) | ((uint64_t)rd_u32(&uid[4]) << 32U);
    size_t name = fbb_string(&b, ops->name);
    size_t version = fbb_string(&b, ops->version);
    size_t build = fbb_string(&b, ops->build);
    size_t detail = fbb_string(&b, "factory protocol");
    fbt_t t;
    fbt_start(&t, &b, 13);
    fbt_u8(&t, 0U, FB_ROLE_FACTORY);
    fbt_offset(&t, 1U, name);
    fbt_offset(&t, 2U, version);
    fbt_u8(&t, 8U, 0U);          /* FlashMode.UNKNOWN */
    fbt_u64(&t, 5U, uid64);
    fbt_u16(&t, 9U, 1U);          /* protocol_major */
    fbt_u16(&t, 10U, 4U);         /* protocol_minor */
    fbt_u16(&t, 11U, HWLINK_MAX_FRAME);
    fbt_offset(&t, 4U, build);
    fbt_offset(&t, 12U, detail);
    size_t body = fbt_end(&t);
    (void)seq;
    return fbb_finish(&b, 2U /* Body.HelloReply */, body, reply, capacity);
}

/* -------------------------------------------------------------- handlers */

static size_t handle_get_info(const factory_ops_t *ops, uint8_t *reply,
                              size_t capacity, uint16_t seq)
{
    static uint8_t raw_reply[FB_RAW_SIZE];
    uint8_t request[1] = { FACTORY_CMD_INFO };
    size_t n = factory_handle(ops, request, sizeof(request), raw_reply, sizeof(raw_reply));
    if (n < 2U || raw_reply[1] != FACTORY_OK) {
        return fb_nak(reply, capacity, seq, FB_ERR_UNSUPPORTED, "INFO failed");
    }

    char name[40] = { 0 };
    char version[24] = { 0 };
    char build[40] = { 0 };
    const char *slots[3] = { name, version, build };
    size_t sizes[3] = { sizeof(name) - 1U, sizeof(version) - 1U, sizeof(build) - 1U };
    size_t at = 2U;
    for (size_t i = 0U; i < 3U; i++) {
        if (at >= n) {
            return fb_nak(reply, capacity, seq, FB_ERR_MALFORMED, "INFO truncated");
        }
        uint8_t len = raw_reply[at++];
        if (at + len > n || len > sizes[i]) {
            return fb_nak(reply, capacity, seq, FB_ERR_MALFORMED, "INFO truncated");
        }
        memcpy((void *)slots[i], &raw_reply[at], len);
        at += len;
    }

    uint8_t uid[12];
    ops->chip_uid(uid);
    fbb_t b;
    fbb_init(&b, fb_arena, sizeof(fb_arena));
    size_t s_name = fbb_string(&b, name);
    size_t s_ver = fbb_string(&b, version);
    size_t s_build = fbb_string(&b, build);
    size_t s_uid = fbb_bytes(&b, uid, sizeof(uid));
    fbt_t t;
    fbt_start(&t, &b, 8);
    fbt_offset(&t, 0U, s_name);
    fbt_offset(&t, 1U, s_ver);
    fbt_offset(&t, 2U, s_build);
    fbt_offset(&t, 3U, s_uid);
    fbt_u8(&t, 4U, 0U);                    /* FlashMode.UNKNOWN */
    fbt_u32(&t, 5U, FACTORY_WINDOW_BASE);
    fbt_u32(&t, 6U, FACTORY_WINDOW_END);
    fbt_u16(&t, 7U, FACTORY_MAX_CHUNK);
    size_t body = fbt_end(&t);
    return fbb_finish(&b, FACTORY_FB_FACTORY_INFO, body, reply, capacity);
}

static size_t handle_read(const factory_ops_t *ops, const fbr_t *r,
                          const uint8_t *request, uint8_t *reply,
                          size_t capacity, uint16_t seq)
{
    uint32_t address = 0U;
    uint16_t length = 0U;
    if (!fbr_u32(r, request, 0U, &address) || !fbr_u16(r, request, 1U, &length) ||
        length == 0U || length > FACTORY_MAX_CHUNK) {
        return fb_nak(reply, capacity, seq, FB_ERR_BAD_ARGUMENT, "read range");
    }

    uint8_t raw_req[7];
    raw_req[0] = FACTORY_CMD_READ;
    wr_u32(&raw_req[1], address);
    wr_u16(&raw_req[5], length);
    static uint8_t raw_reply[FB_RAW_SIZE];
    size_t n = factory_handle(ops, raw_req, sizeof(raw_req), raw_reply, sizeof(raw_reply));
    if (n < 2U || raw_reply[1] != FACTORY_OK || n < 2U + length) {
        uint16_t status = n >= 2U ? map_error(raw_reply[1]) : FB_ERR_MALFORMED;
        return fb_nak(reply, capacity, seq, status, "read failed");
    }

    fbb_t b;
    fbb_init(&b, fb_arena, sizeof(fb_arena));
    size_t data = fbb_bytes(&b, &raw_reply[2], length);
    fbt_t t;
    fbt_start(&t, &b, 2);
    fbt_u32(&t, 0U, address);
    fbt_offset(&t, 1U, data);
    size_t body = fbt_end(&t);
    return fbb_finish(&b, FACTORY_FB_MEMORY_DATA, body, reply, capacity);
}

static size_t handle_write_begin(const factory_ops_t *ops, const fbr_t *r,
                                 const uint8_t *request, uint8_t *reply,
                                 size_t capacity, uint16_t seq)
{
    uint8_t kind = 0U;
    uint32_t address = 0U;
    uint16_t total = 0U;
    uint32_t crc = 0U;
    if (!fbr_u8(r, request, 0U, &kind) || !fbr_u32(r, request, 1U, &address) ||
        !fbr_u16(r, request, 2U, &total) || !fbr_u32(r, request, 3U, &crc) ||
        kind < FB_KIND_SYS || kind > FB_KIND_DIAG) {
        return fb_nak(reply, capacity, seq, FB_ERR_BAD_ARGUMENT, "write begin");
    }

    uint8_t raw_req[11];
    raw_req[0] = FACTORY_CMD_WRITE_BEGIN;
    wr_u32(&raw_req[1], address);
    wr_u16(&raw_req[5], total);
    wr_u32(&raw_req[7], crc);
    static uint8_t raw_reply[FB_RAW_SIZE];
    size_t n = factory_handle(ops, raw_req, sizeof(raw_req), raw_reply, sizeof(raw_reply));
    if (n < 2U || raw_reply[1] != FACTORY_OK) {
        return fb_nak(reply, capacity, seq, n >= 2U ? map_error(raw_reply[1]) : FB_ERR_MALFORMED,
                      "write begin refused");
    }
    return fb_ack(reply, capacity, seq, 6U /* Body.Ack */);
}

static size_t handle_write_data(const factory_ops_t *ops, const fbr_t *r,
                                const uint8_t *request, uint8_t *reply,
                                size_t capacity, uint16_t seq)
{
    uint16_t offset = 0U;
    const uint8_t *data = NULL;
    uint32_t len = 0U;
    if (!fbr_u16(r, request, 0U, &offset) ||
        !fbr_vector(r, request, 1U, &data, &len) ||
        len == 0U || len > FACTORY_MAX_CHUNK) {
        return fb_nak(reply, capacity, seq, FB_ERR_BAD_ARGUMENT, "write data");
    }

    uint8_t raw_req[3U + FACTORY_MAX_CHUNK];
    raw_req[0] = FACTORY_CMD_WRITE_DATA;
    wr_u16(&raw_req[1], offset);
    memcpy(&raw_req[3], data, len);
    static uint8_t raw_reply[FB_RAW_SIZE];
    size_t n = factory_handle(ops, raw_req, 3U + len, raw_reply, sizeof(raw_reply));
    if (n < 2U || raw_reply[1] != FACTORY_OK) {
        return fb_nak(reply, capacity, seq, n >= 2U ? map_error(raw_reply[1]) : FB_ERR_MALFORMED,
                      "write data refused");
    }
    return fb_ack(reply, capacity, seq, 6U);
}

static size_t fb_write_result(uint8_t *reply, size_t capacity, bool success,
                              uint32_t crc, uint16_t error, const char *detail)
{
    fbb_t b;
    fbb_init(&b, fb_arena, sizeof(fb_arena));
    size_t text = fbb_string(&b, detail != NULL ? detail : "");
    fbt_t t;
    fbt_start(&t, &b, 4);
    fbt_bool(&t, 0U, success);
    fbt_u32(&t, 1U, crc);
    fbt_u16(&t, 2U, error);
    fbt_offset(&t, 3U, text);
    size_t body = fbt_end(&t);
    return fbb_finish(&b, FACTORY_FB_WRITE_RESULT, body, reply, capacity);
}

static size_t handle_write_end(const factory_ops_t *ops, uint8_t *reply,
                               size_t capacity, uint16_t seq)
{
    uint8_t raw_req[1] = { FACTORY_CMD_WRITE_END };
    static uint8_t raw_reply[FB_RAW_SIZE];
    size_t n = factory_handle(ops, raw_req, sizeof(raw_req), raw_reply, sizeof(raw_reply));
    if (n >= 2U && raw_reply[1] == FACTORY_OK && n >= 6U) {
        return fb_write_result(reply, capacity, true, rd_u32(&raw_reply[2]),
                               FB_ERR_NONE, "programmed and verified");
    }
    uint16_t status = n >= 2U ? map_error(raw_reply[1]) : FB_ERR_MALFORMED;
    (void)seq;
    return fb_write_result(reply, capacity, false, 0U, status, "write end failed");
}

static size_t handle_diag_init(const factory_ops_t *ops, const fbr_t *r,
                               const uint8_t *request, uint8_t *reply,
                               size_t capacity, uint16_t seq)
{
    uint32_t address = 0U;
    if (!fbr_u32(r, request, 0U, &address)) {
        return fb_nak(reply, capacity, seq, FB_ERR_BAD_ARGUMENT, "diag address");
    }
    uint8_t raw_req[5];
    raw_req[0] = FACTORY_CMD_DIAG_INIT;
    wr_u32(&raw_req[1], address);
    static uint8_t raw_reply[FB_RAW_SIZE];
    size_t n = factory_handle(ops, raw_req, sizeof(raw_req), raw_reply, sizeof(raw_reply));
    if (n >= 2U && raw_reply[1] == FACTORY_OK && n >= 6U) {
        return fb_write_result(reply, capacity, true, rd_u32(&raw_reply[2]),
                               FB_ERR_NONE, "clean diagnostic map programmed");
    }
    uint16_t status = n >= 2U ? map_error(raw_reply[1]) : FB_ERR_MALFORMED;
    return fb_write_result(reply, capacity, false, 0U, status, "diag init failed");
}

static size_t handle_selftest(const factory_ops_t *ops, uint8_t *reply,
                              size_t capacity, uint16_t seq)
{
    uint8_t raw_req[1] = { FACTORY_CMD_SELFTEST };
    static uint8_t raw_reply[FB_RAW_SIZE];
    size_t n = factory_handle(ops, raw_req, sizeof(raw_req), raw_reply, sizeof(raw_reply));
    if (n < 3U || raw_reply[1] != FACTORY_OK) {
        return fb_nak(reply, capacity, seq, n >= 2U ? map_error(raw_reply[1]) : FB_ERR_MALFORMED,
                      "self-test refused");
    }
    uint8_t count = raw_reply[2];
    if (3U + (size_t)count * 6U > n) {
        return fb_nak(reply, capacity, seq, FB_ERR_MALFORMED, "self-test truncated");
    }

    fbb_t b;
    fbb_init(&b, fb_arena, sizeof(fb_arena));
    size_t items[16];
    if (count > 16U) {
        count = 16U;
    }
    for (uint8_t i = 0U; i < count; i++) {
        const uint8_t *item = &raw_reply[3U + (size_t)i * 6U];
        fbt_t t;
        fbt_start(&t, &b, 3);
        fbt_u8(&t, 0U, item[0]);
        fbt_u8(&t, 1U, item[1]);
        fbt_u32(&t, 2U, rd_u32(&item[2]));
        items[i] = fbt_end(&t);
    }
    size_t vector = fbb_vector_tables(&b, items, count);
    fbt_t body_t;
    fbt_start(&body_t, &b, 1);
    fbt_offset(&body_t, 0U, vector);
    size_t body = fbt_end(&body_t);
    return fbb_finish(&b, FACTORY_FB_SELF_TEST_RESULT, body, reply, capacity);
}

/* ---------------------------------------------------------------- entry */

size_t factory_fb_handle(const factory_ops_t *ops, const uint8_t *request,
                         size_t request_len, uint8_t *reply,
                         size_t reply_capacity, uint16_t seq)
{
    if (ops == NULL || request == NULL || reply == NULL || request_len < 8U) {
        return 0U;
    }
    fbr_t r = { request, request + request_len };
    const uint8_t *message = fbr_root(&r);
    if (message == NULL) {
        return fb_nak(reply, reply_capacity, seq, FB_ERR_MALFORMED, "malformed message");
    }
    uint8_t body_type = 0U;
    const uint8_t *body = fbr_table(&r, message, 1U);
    if (!fbr_u8(&r, message, 0U, &body_type) || body == NULL) {
        return fb_nak(reply, reply_capacity, seq, FB_ERR_MALFORMED, "message has no body");
    }

    switch (body_type) {
    case FACTORY_FB_HELLO:
        return fb_hello_reply(ops, reply, reply_capacity, seq);
    case FACTORY_FB_GET_FACTORY_INFO:
        return handle_get_info(ops, reply, reply_capacity, seq);
    case FACTORY_FB_READ_MEMORY:
        return handle_read(ops, &r, body, reply, reply_capacity, seq);
    case FACTORY_FB_WRITE_BEGIN:
        return handle_write_begin(ops, &r, body, reply, reply_capacity, seq);
    case FACTORY_FB_WRITE_DATA:
        return handle_write_data(ops, &r, body, reply, reply_capacity, seq);
    case FACTORY_FB_WRITE_END:
        return handle_write_end(ops, reply, reply_capacity, seq);
    case FACTORY_FB_DIAG_INIT:
        return handle_diag_init(ops, &r, body, reply, reply_capacity, seq);
    case FACTORY_FB_SELF_TEST:
        return handle_selftest(ops, reply, reply_capacity, seq);
    default:
        return fb_nak(reply, reply_capacity, seq, FB_ERR_UNSUPPORTED, "message not supported");
    }
}
