/**
 * @file    factory_store.h
 * @brief   Flash access for the factory data window.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-09
 * @target  STM32F767ZIT6
 */

#ifndef FACTORY_STORE_H
#define FACTORY_STORE_H

#include "factory_proto.h"

/**
 * @brief Get the operations table that backs factory_handle().
 * @return Static table bound to the identity sector and the STM32F7 flash.
 * @note The table covers flash read/write, chip UID, CRC-32 and the
 *       SELFTEST hardware readers; factory_proto.c stays hardware-free.
 */
const factory_ops_t *factory_store_ops(void);

#endif /* FACTORY_STORE_H */
