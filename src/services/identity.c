/**
 * @file    identity.c
 * @brief   Factory provisioning record (PRV1) reader.
 * @details The factory writes a PRV1 record into bank 2 sector 13 (0x08104000,
 *          or 0x08004000 while bank 2 is swapped in).  This reader checks the
 *          header, both CRC-32 values and the chip UID binding, then exposes
 *          the serial, product, revision, profile and MAC.  A blank or torn
 *          record is reported as absent; it is never treated as valid
 *          identity.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-04
 * @target  STM32F767ZIT6
 * @srs     identity contract PRV1, factory A-02/A-03/A-05/A-06
 */

#include <string.h>
#include "stm32f7xx_hal.h"
#include "crc32.h"
#include "identity.h"

#define ID_MAGIC              0x31565250UL  /* "PRV1" little-endian */
#define ID_FORMAT             1U
#define ID_HEADER_SIZE        32U
#define ID_MAX_TLV            4096U

#define ID_T_SERIAL           0x01U
#define ID_T_PRODUCT          0x02U
#define ID_T_HW_REV           0x03U
#define ID_T_MFG_TIME         0x04U
#define ID_T_STATION_FIXTURE  0x05U
#define ID_T_PROFILE          0x06U
#define ID_T_FIRMWARE_VERSION 0x07U
#define ID_T_MAC              0x10U

#define ID_IDENTITY_A         0x08104000UL
#define ID_IDENTITY_SWAPPED   0x08004000UL

/** PRV1 record header, 32 bytes.  The first 28 bytes carry the CRC. */
typedef struct {
    uint32_t magic;       /**< ID_MAGIC, "PRV1". */
    uint16_t format;      /**< ID_FORMAT. */
    uint16_t revision;    /**< Record revision counter. */
    uint32_t length;      /**< TLV length, bytes. */
    uint32_t tlv_crc32;   /**< CRC-32 of the TLV block. */
    uint8_t uid[12];      /**< 96-bit chip UID the record is bound to. */
    uint32_t header_crc32;/**< CRC-32 of the previous 28 bytes. */
} id_header_t;

/** Decoded identity fields; empty when no valid record was found. */
typedef struct {
    bool valid;             /**< true once parse_from() succeeded. */
    uint16_t revision;      /**< Record revision counter. */
    char serial[33];        /**< Serial number, NUL-terminated. */
    char product[33];       /**< Product name, NUL-terminated. */
    char hw_revision[9];    /**< Hardware revision, NUL-terminated. */
    char profile[65];       /**< Calibration profile, NUL-terminated. */
    uint8_t mac[6];         /**< Provisioned EUI-48. */
    bool mac_valid;         /**< true when @ref mac was provisioned. */
} app_identity_t;

static app_identity_t identity; /**< Current identity, valid or empty. */
static bool refreshed;          /**< true once a read was attempted. */

/** @brief CRC-32 of the 28 bytes of header that precede the checksum. */
static uint32_t header_crc(const uint8_t *raw)
{
    return app_crc32(raw, 28U);
}

/**
 * @brief Copy a TLV value into a NUL-terminated text field.
 * @param[out] out    Destination field.
 * @param[in]  cap    Capacity of @p out, bytes.
 * @param[in]  data   TLV value, not NUL-terminated.
 * @param[in]  length TLV value length, bytes.
 */
static void copy_text(char *out, size_t cap, const uint8_t *data, uint8_t length)
{
    size_t n = length;
    if (n >= cap) {
        n = cap - 1U;
    }
    memcpy(out, data, n);
    out[n] = '\0';
}

/**
 * @brief Parse and validate the record at one sector address.
 * @param[in] address Unswapped or swapped sector base.
 * @return true when the header, both CRCs and the UID match and the record
 *         carries at least a serial and a product.
 */
static bool parse_from(uint32_t address)
{
    uint8_t raw[ID_HEADER_SIZE];
    const uint32_t *words = (const uint32_t *)address;

    SCB_InvalidateDCache_by_Addr((uint32_t *)address, ID_MAX_TLV);
    memcpy(raw, (const void *)address, sizeof(raw));
    if (words[0] != ID_MAGIC) {
        return false;
    }

    id_header_t header;
    memcpy(&header, raw, sizeof(header));
    if (header.format != ID_FORMAT || header.length == 0U ||
        header.length > ID_MAX_TLV || header.length < 2U) {
        return false;
    }
    if (header_crc(raw) != header.header_crc32) {
        return false;
    }

    /* UID binding: the record has the three UID words in flash byte order. */
    uint8_t uid[12];
    uint32_t uid_words[3] = { HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2() };
    memcpy(uid, uid_words, sizeof(uid));
    if (memcmp(uid, header.uid, sizeof(uid)) != 0) {
        return false;
    }

    const uint8_t *tlv = (const uint8_t *)(address + ID_HEADER_SIZE);
    if (app_crc32(tlv, header.length) != header.tlv_crc32) {
        return false;
    }

    memset(&identity, 0, sizeof(identity));
    identity.revision = header.revision;
    size_t offset = 0U;
    while (offset + 2U <= header.length) {
        uint8_t type = tlv[offset];
        uint8_t length = tlv[offset + 1U];
        offset += 2U;
        if (type == 0U) {
            break;
        }
        if (offset + length > header.length) {
            return false;
        }
        const uint8_t *value = &tlv[offset];
        switch (type) {
        case ID_T_SERIAL:
            copy_text(identity.serial, sizeof(identity.serial), value, length);
            break;
        case ID_T_PRODUCT:
            copy_text(identity.product, sizeof(identity.product), value, length);
            break;
        case ID_T_HW_REV:
            copy_text(identity.hw_revision, sizeof(identity.hw_revision), value, length);
            break;
        case ID_T_PROFILE:
            copy_text(identity.profile, sizeof(identity.profile), value, length);
            break;
        case ID_T_MAC:
            if (length == 6U) {
                memcpy(identity.mac, value, 6U);
                identity.mac_valid = true;
            }
            break;
        default:
            break;
        }
        offset += length;
    }

    if (identity.serial[0] == '\0' || identity.product[0] == '\0') {
        return false;
    }
    identity.valid = true;
    return true;
}

bool app_identity_refresh(void)
{
    refreshed = true;
    memset(&identity, 0, sizeof(identity));
    if (parse_from(ID_IDENTITY_A)) {
        return true;
    }
    return parse_from(ID_IDENTITY_SWAPPED);
}

/**
 * @brief Read the record on first use.
 * @return true when a valid record is loaded.
 */
static bool ensure_loaded(void)
{
    if (!refreshed) {
        (void)app_identity_refresh();
    }
    return identity.valid;
}

bool app_identity_valid(void)
{
    return ensure_loaded();
}

const char *app_identity_serial(void)
{
    return ensure_loaded() ? identity.serial : "";
}

const char *app_identity_product(void)
{
    return ensure_loaded() ? identity.product : "";
}

const char *app_identity_hw_revision(void)
{
    return ensure_loaded() ? identity.hw_revision : "";
}

const char *app_identity_profile(void)
{
    return ensure_loaded() ? identity.profile : "";
}

uint16_t app_identity_revision(void)
{
    return ensure_loaded() ? identity.revision : 0U;
}

bool app_identity_mac(uint8_t mac[6])
{
    if (!ensure_loaded() || !identity.mac_valid) {
        return false;
    }
    memcpy(mac, identity.mac, 6U);
    return true;
}

/**
 * @brief Append text to a bounded key=value string.
 * @param[out] out  Destination buffer.
 * @param[in]  cap  Capacity of @p out, bytes.
 * @param[in]  pos  Current write offset.
 * @param[in]  text Text to append; NULL is ignored.
 * @return New write offset; @p out stays NUL-terminated.
 */
static size_t append_text(char *out, size_t cap, size_t pos, const char *text)
{
    while (text != NULL && *text != '\0' && pos + 1U < cap) {
        out[pos++] = *text++;
    }
    out[pos] = '\0';
    return pos;
}

/**
 * @brief Append an unsigned decimal number to a bounded string.
 * @param[out] out   Destination buffer.
 * @param[in]  cap   Capacity of @p out, bytes.
 * @param[in]  pos   Current write offset.
 * @param[in]  value Number to format.
 * @return New write offset; @p out stays NUL-terminated.
 */
static size_t append_uint(char *out, size_t cap, size_t pos, unsigned value)
{
    char digits[10];
    size_t count = 0U;
    do {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U && count < sizeof(digits));
    while (count > 0U && pos + 1U < cap) {
        out[pos++] = digits[--count];
    }
    out[pos] = '\0';
    return pos;
}

/**
 * @brief Append the provisioned MAC as AA:BB:CC:DD:EE:FF.
 * @param[out] out Destination buffer.
 * @param[in]  cap Capacity of @p out, bytes.
 * @param[in]  pos Current write offset.
 * @return New write offset; @p out stays NUL-terminated.
 */
static size_t append_mac(char *out, size_t cap, size_t pos)
{
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0U; i < 6U && pos + 3U < cap; i++) {
        if (i != 0U) {
            out[pos++] = ':';
        }
        out[pos++] = hex[(identity.mac[i] >> 4U) & 0xFU];
        out[pos++] = hex[identity.mac[i] & 0xFU];
    }
    out[pos] = '\0';
    return pos;
}

bool app_identity_detail(char *out, size_t capacity)
{
    if (out == NULL || capacity == 0U) {
        return false;
    }
    out[0] = '\0';
    if (!ensure_loaded()) {
        (void)append_text(out, capacity, 0U, "no identity");
        return false;
    }
    size_t pos = 0U;
    pos = append_text(out, capacity, pos, "PRV1 serial=");
    pos = append_text(out, capacity, pos, identity.serial);
    pos = append_text(out, capacity, pos, ";mac=");
    if (identity.mac_valid) {
        pos = append_mac(out, capacity, pos);
    }
    pos = append_text(out, capacity, pos, ";hw=");
    pos = append_text(out, capacity, pos, identity.hw_revision);
    pos = append_text(out, capacity, pos, ";product=");
    pos = append_text(out, capacity, pos, identity.product);
    pos = append_text(out, capacity, pos, ";profile=");
    pos = append_text(out, capacity, pos, identity.profile);
    pos = append_text(out, capacity, pos, ";revision=");
    (void)append_uint(out, capacity, pos, identity.revision);
    return true;
}
