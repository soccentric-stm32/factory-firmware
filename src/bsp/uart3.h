/**
 * @file    uart3.h
 * @brief   USART3 HWLINK link on the ST-LINK VCP.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (PD8 TX, PD9 RX)
 * @srs     COM-001, COM-002, DCON-004
 */

#ifndef UART3_H
#define UART3_H

#include <stdbool.h>
#include <stdint.h>

#define APP_UART_DEFAULT_BAUD   115200U /**< Baud rate after reset. */
#define APP_UART_FAST_BAUD      921600U /**< Optional fast rate (COM-001). */

/**
 * @brief Start USART3 at 115200 8N1 with circular DMA receive.
 * @return true on success.
 * @note Call after the clocks and before the scheduler.
 */
bool app_uart3_init(void);

/**
 * @brief Switch to one of the two supported rates.
 * @param[in] baud APP_UART_DEFAULT_BAUD or APP_UART_FAST_BAUD.
 * @return true on success, false for an unsupported rate or HAL error.
 * @pre The Ack has already been sent at the old rate (COM-001).
 */
bool app_uart3_set_baud(uint32_t baud);

/**
 * @brief Get the current baud rate.
 * @return Baud rate, bit/s.
 */
uint32_t app_uart3_baud(void);

/**
 * @brief Copy received bytes out of the DMA ring.
 * @param[out] dst     Destination buffer.
 * @param[in]  max_len Capacity of @p dst, bytes.
 * @return Bytes copied.
 */
uint32_t app_uart3_rx_read(uint8_t *dst, uint32_t max_len);

/**
 * @brief Check whether the DMA receiver is armed (HLT-001).
 * @return true while receiving.
 */
bool app_uart3_rx_running(void);

/**
 * @brief Re-arm the circular DMA receiver after an error or abort.
 * @return true when the receiver is running afterwards.
 * @note Comms task only; no-op while the receiver runs.
 */
bool app_uart3_rx_restart(void);

/**
 * @brief Transmit one frame and wait for the last stop bit.
 * @param[in] data       Frame bytes.
 * @param[in] len        Length, bytes.
 * @param[in] timeout_ms Completion timeout, ms.
 * @return true when the frame was sent in time.
 * @note Single caller task only; blocks only the caller.
 */
bool app_uart3_send(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief Wake the Comms task after received data or a receive event.
 * @note Called from the USART3/DMA ISRs; defined by the Comms task.
 */
void comms_post_rx_from_isr(void);

#endif /* UART3_H */
