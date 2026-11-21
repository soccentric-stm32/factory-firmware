/**
 * @file    identity.h
 * @brief   Factory provisioning record (PRV1) reader.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-04
 * @target  STM32F767ZIT6
 * @srs     identity contract PRV1, factory A-02/A-03/A-05/A-06
 */

#ifndef IDENTITY_H
#define IDENTITY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Reload and validate the record from the identity sector.
 * @return true when a valid, UID-bound record is present.
 * @note Reads both the unswapped and the swapped sector address.
 */
bool app_identity_refresh(void);

/** @brief Check whether a valid, UID-bound record is present. */
bool app_identity_valid(void);

/**
 * @brief Get the provisioned serial number.
 * @return Serial text; "" when no record is present.
 */
const char *app_identity_serial(void);

/**
 * @brief Get the product name.
 * @return Product text; "" when no record is present.
 */
const char *app_identity_product(void);

/**
 * @brief Get the hardware revision.
 * @return Revision text; "" when no record is present.
 */
const char *app_identity_hw_revision(void);

/**
 * @brief Get the calibration/profile name.
 * @return Profile text; "" when no record is present.
 */
const char *app_identity_profile(void);

/**
 * @brief Get the record revision counter.
 * @return Record revision, 0 when no record is present.
 */
uint16_t app_identity_revision(void);

/**
 * @brief Copy the provisioned EUI-48.
 * @param[out] mac Caller buffer of 6 bytes.
 * @return true when a MAC is provisioned.
 */
bool app_identity_mac(uint8_t mac[6]);

/**
 * @brief Build the compact key=value detail carried in HelloReply (A-05).
 * @param[out] out      Destination buffer.
 * @param[in]  capacity Capacity of @p out, bytes.
 * @return true when a record was formatted; false (with "no identity" or an
 *         empty string) when none is present or the buffer is unusable.
 */
bool app_identity_detail(char *out, size_t capacity);

#endif /* IDENTITY_H */
