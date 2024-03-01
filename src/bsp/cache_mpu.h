/**
 * @file    cache_mpu.h
 * @brief   CPU caches and the non-cacheable DMA window.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DGEN-003 (caches on), COM-002 (DMA buffers)
 */

#ifndef CACHE_MPU_H
#define CACHE_MPU_H

/**
 * @brief Make SRAM2 (.dma_buffer) non-cacheable, then enable I- and D-cache.
 * @note Call once, before any DMA starts.
 */
void app_cache_mpu_init(void);

#endif /* CACHE_MPU_H */
