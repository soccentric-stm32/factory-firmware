/**
 * @file    hwlink.h
 * @brief   HWLINK framing: 4-byte header, CRC-32 trailer and COBS.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     HWLINK LNK-001..LNK-005 (SRS-GUI-F767 Appendix A)
 */

#ifndef HWLINK_H
#define HWLINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup hwlink HWLINK framing
 * @brief Frame: proto, flags, seq (LE16), payload, CRC-32 (LE32), COBS, 0x00.
 * @{
 */

#define HWLINK_PROTOCOL_MAJOR     1U      /**< Protocol byte of FlatBuffers frames. */
#define HWLINK_PROTOCOL_MINOR     4U      /**< Protocol minor version. */
#define HWLINK_PROTOCOL_FACTORY   0x46U   /**< "F": raw factory frames. */
#define HWLINK_MAX_FRAME          2048U   /**< Max unencoded frame, bytes. */
/** Worst-case COBS-encoded size including the delimiter, bytes. */
#define HWLINK_MAX_ENCODED        (HWLINK_MAX_FRAME + (HWLINK_MAX_FRAME / 254U) + 2U)

#define HWLINK_FLAG_RESPONSE      (1U << 0U) /**< Frame answers a request. */
#define HWLINK_FLAG_EVENT         (1U << 1U) /**< Unsolicited event frame. */

/**
 * @brief Build an unencoded frame with protocol HWLINK_PROTOCOL_MAJOR.
 * @param[in]  payload      Payload bytes (non-NULL).
 * @param[in]  payload_len  Payload length, bytes.
 * @param[in]  flags        HWLINK_FLAG_* mask.
 * @param[in]  seq          Sequence number.
 * @param[out] raw          Frame buffer.
 * @param[in]  raw_capacity Capacity of @p raw, bytes.
 * @param[out] raw_len      Frame length, bytes.
 * @return true on success; false on NULL argument or overflow.
 */
bool hwlink_build_raw(const uint8_t *payload, size_t payload_len,
                      uint8_t flags, uint16_t seq,
                      uint8_t *raw, size_t raw_capacity, size_t *raw_len);

/**
 * @brief Build an unencoded frame with an explicit protocol byte.
 * @param[in]  proto        Protocol byte, e.g. HWLINK_PROTOCOL_FACTORY.
 * @param[in]  payload      Payload bytes (non-NULL).
 * @param[in]  payload_len  Payload length, bytes.
 * @param[in]  flags        HWLINK_FLAG_* mask.
 * @param[in]  seq          Sequence number.
 * @param[out] raw          Frame buffer.
 * @param[in]  raw_capacity Capacity of @p raw, bytes.
 * @param[out] raw_len      Frame length, bytes.
 * @return true on success; false on NULL argument or overflow.
 */
bool hwlink_build_raw_proto(uint8_t proto, const uint8_t *payload,
                            size_t payload_len, uint8_t flags, uint16_t seq,
                            uint8_t *raw, size_t raw_capacity,
                            size_t *raw_len);

/**
 * @brief Validate a frame of either protocol and extract its payload.
 * @param[in]  raw              Unencoded frame.
 * @param[in]  raw_len          Frame length, bytes.
 * @param[out] payload          Payload buffer.
 * @param[in]  payload_capacity Capacity of @p payload, bytes.
 * @param[out] payload_len      Payload length, bytes.
 * @param[out] flags            Frame flags.
 * @param[out] seq              Sequence number.
 * @param[out] proto            Protocol byte.
 * @return true for a valid frame; false on bad length, header, CRC or size.
 */
bool hwlink_decode_raw_proto(const uint8_t *raw, size_t raw_len,
                             uint8_t *payload, size_t payload_capacity,
                             size_t *payload_len, uint8_t *flags,
                             uint16_t *seq, uint8_t *proto);

/**
 * @brief COBS-encode a frame and append the 0x00 delimiter.
 * @param[in]  raw          Unencoded frame, non-empty.
 * @param[in]  raw_len      Frame length, bytes.
 * @param[out] out          Encoded buffer.
 * @param[in]  out_capacity Capacity of @p out, bytes.
 * @param[out] out_len      Encoded length incl. delimiter, bytes.
 * @return true on success; false on bad argument or overflow.
 */
bool hwlink_cobs_encode(const uint8_t *raw, size_t raw_len,
                        uint8_t *out, size_t out_capacity, size_t *out_len);

/**
 * @brief COBS-decode one frame without its delimiter.
 * @param[in]  in           Encoded bytes.
 * @param[in]  in_len       Encoded length, bytes.
 * @param[out] out          Decoded buffer.
 * @param[in]  out_capacity Capacity of @p out, bytes.
 * @param[out] out_len      Decoded length, bytes.
 * @retval 0  Success.
 * @retval -1 Bad argument, malformed input or overflow.
 */
int hwlink_cobs_decode(const uint8_t *in, size_t in_len,
                       uint8_t *out, size_t out_capacity, size_t *out_len);

/**
 * @brief Validate a HWLINK_PROTOCOL_MAJOR frame and extract its payload.
 * @param[in]  raw              Unencoded frame.
 * @param[in]  raw_len          Frame length, bytes.
 * @param[out] payload          Payload buffer.
 * @param[in]  payload_capacity Capacity of @p payload, bytes.
 * @param[out] payload_len      Payload length, bytes.
 * @param[out] flags            Frame flags.
 * @param[out] seq              Sequence number.
 * @return true for a valid frame of protocol HWLINK_PROTOCOL_MAJOR.
 * @note Outputs are written for a valid factory frame even though it
 *       returns false.
 */
bool hwlink_decode_raw(const uint8_t *raw, size_t raw_len,
                       uint8_t *payload, size_t payload_capacity,
                       size_t *payload_len, uint8_t *flags, uint16_t *seq);

/** @} */

#endif /* HWLINK_H */
