/*
 * factory_store.h - flash access for the factory data window
 *
 * Author:  Sandesh Ghimire <sandesh@soccentric.com>
 * Date:    2026-11-09
 * Target:  STM32F767ZIT6
 */

#ifndef FACTORY_STORE_H
#define FACTORY_STORE_H

#include "factory_proto.h"

/* Operations table for factory_handle(), backed by the identity sector. */
const factory_ops_t *factory_store_ops(void);

#endif /* FACTORY_STORE_H */
