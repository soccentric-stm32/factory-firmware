/**
 * @file    factory_fb.h
 * @brief   FACTORY messages over HWLINK FlatBuffers.
 * @details Protocol minor 4.  The factory firmware answers the FACTORY union
 *          members (body types 57..68) instead of the legacy proto=0x46 raw
 *          command set.  The file bodies are carried unmodified inside
 *          MemoryData / FactoryWriteData, so factory_proto.c keeps validating
 *          the 32-byte header, both CRC-32 values and the chip-UID binding.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-19
 * @target  STM32F767ZIT6 (also builds on the host for tests)
 * @srs     docs/FACTORY_SPEC.md, protocol/hwlink.fbs
 */

#ifndef FACTORY_FB_H
#define FACTORY_FB_H

#include <stddef.h>
#include <stdint.h>
#include "factory_proto.h"

/* Union member numbers from protocol/hwlink.fbs.  They are appended after
 * OperatorPromptDone (56); existing members are never renumbered. */
#define FACTORY_FB_GET_FACTORY_INFO   57U /**< Request FactoryInfo. */
#define FACTORY_FB_FACTORY_INFO       58U /**< Reply with FactoryInfo. */
#define FACTORY_FB_READ_MEMORY        59U /**< Request a memory read. */
#define FACTORY_FB_MEMORY_DATA        60U /**< Reply with memory data. */
#define FACTORY_FB_WRITE_BEGIN        61U /**< Start a block write. */
#define FACTORY_FB_WRITE_DATA         62U /**< Send a chunk. */
#define FACTORY_FB_WRITE_END          63U /**< Commit the block. */
#define FACTORY_FB_WRITE_RESULT       64U /**< Reply with the write result. */
#define FACTORY_FB_DIAG_INIT          65U /**< Write a clean .diag block. */
#define FACTORY_FB_SELF_TEST          66U /**< Request the self-test. */
#define FACTORY_FB_SELF_TEST_ITEM     67U /**< One self-test result item. */
#define FACTORY_FB_SELF_TEST_RESULT   68U /**< Reply with all self-test items. */

/* Body numbers that already exist in the generated schema. */
#define FACTORY_FB_HELLO              1U /**< hwlink Body.Hello. */

/**
 * @brief Handle one verified HWLINK Message payload and build the reply.
 * @param[in]  ops            Hardware hooks passed to factory_handle().
 * @param[in]  request        Verified Message bytes.
 * @param[in]  request_len    Length, bytes.
 * @param[out] reply          Reply buffer.
 * @param[in]  reply_capacity Capacity of @p reply, bytes.
 * @param[in]  seq            Request sequence number.
 * @return Reply length, or 0 when the request cannot be answered.
 * @note The reply is a complete FlatBuffer with the HWLK identifier.
 */
size_t factory_fb_handle(const factory_ops_t *ops, const uint8_t *request,
                         size_t request_len, uint8_t *reply,
                         size_t reply_capacity, uint16_t seq);

#endif /* FACTORY_FB_H */
