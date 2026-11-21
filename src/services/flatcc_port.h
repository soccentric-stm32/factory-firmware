/**
 * @file    flatcc_port.h
 * @brief   flatcc runtime allocation hooks.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DRT-004 (no allocation after start-up), DCON-002 (flatcc runtime)
 *
 * The flatcc runtime is compiled with FLATCC_ALLOC/FLATCC_FREE/... mapped to
 * these functions, which use a fixed static arena.  This header is force-
 * included into every translation unit in CMakeLists.txt so the macro
 * expansion sees the prototypes.
 */

#ifndef FLATCC_PORT_H
#define FLATCC_PORT_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Claim @p size bytes from the static arena.
 * @param[in] size Bytes requested; rounded up to 16.
 * @return Zeroed-by-nobody pointer, or NULL when the arena is full.
 */
void *app_flatcc_alloc(size_t size);

/**
 * @brief Return a block to the arena and coalesce its neighbours.
 * @param[in] ptr Block from app_flatcc_alloc(); NULL is ignored.
 */
void app_flatcc_free(void *ptr);

/**
 * @brief Grow or shrink an allocation, copying the payload when it moves.
 * @param[in] ptr  Existing block, or NULL to allocate.
 * @param[in] size New size, bytes; 0 frees @p ptr.
 * @return Block of at least @p size bytes, or NULL on exhaustion.
 */
void *app_flatcc_realloc(void *ptr, size_t size);

/**
 * @brief Allocate @p count * @p size bytes and zero them.
 * @param[in] count Element count.
 * @param[in] size  Element size, bytes.
 * @return Zeroed block, or NULL when the arena is full or the size wraps.
 */
void *app_flatcc_calloc(size_t count, size_t size);

/**
 * @brief Allocate @p size bytes with an alignment hint.
 * @param[in] alignment Requested alignment; the arena is always 16-byte
 *                      aligned, so the hint is ignored.
 * @param[in] size      Bytes requested.
 * @return Block, or NULL when the arena is full.
 */
void *app_flatcc_aligned_alloc(size_t alignment, size_t size);

/**
 * @brief Release a block from app_flatcc_aligned_alloc().
 * @param[in] ptr Block to free; NULL is ignored.
 */
void app_flatcc_aligned_free(void *ptr);

#endif /* FLATCC_PORT_H */
