/*
 * factory_fb.h - FACTORY messages over HWLINK FlatBuffers
 *
 * Author:  Sandesh Ghimire <sandesh@soccentric.com>
 * Date:    2026-11-19
 * Target:  STM32F767ZIT6 (also builds on the host for tests)
 *
 * Protocol minor 4.  The factory firmware answers the FACTORY union members
 * (body types 57..68) instead of the legacy proto=0x46 raw command set.  The
 * file bodies are carried unmodified inside MemoryData / FactoryWriteData,
 * so factory_proto.c keeps validating the 32-byte header, both CRC-32 values
 * and the chip-UID binding.
 */

#ifndef FACTORY_FB_H
#define FACTORY_FB_H

#include <stddef.h>
#include <stdint.h>
#include "factory_proto.h"

/* Union member numbers from protocol/hwlink.fbs.  They are appended after
 * OperatorPromptDone (56); existing members are never renumbered. */
#define FACTORY_FB_GET_FACTORY_INFO   57U
#define FACTORY_FB_FACTORY_INFO       58U
#define FACTORY_FB_READ_MEMORY        59U
#define FACTORY_FB_MEMORY_DATA        60U
#define FACTORY_FB_WRITE_BEGIN        61U
#define FACTORY_FB_WRITE_DATA         62U
#define FACTORY_FB_WRITE_END          63U
#define FACTORY_FB_WRITE_RESULT       64U
#define FACTORY_FB_DIAG_INIT          65U
#define FACTORY_FB_SELF_TEST          66U
#define FACTORY_FB_SELF_TEST_ITEM     67U
#define FACTORY_FB_SELF_TEST_RESULT   68U

/* Body numbers that already exist in the generated schema. */
#define FACTORY_FB_HELLO              1U

/*
 * Handle one verified HWLINK Message payload and build the reply payload.
 * Returns the reply length, or 0 when the request cannot be answered.
 * The reply is a complete FlatBuffer with the HWLK identifier.
 */
size_t factory_fb_handle(const factory_ops_t *ops, const uint8_t *request,
                         size_t request_len, uint8_t *reply,
                         size_t reply_capacity, uint16_t seq);

#endif /* FACTORY_FB_H */
