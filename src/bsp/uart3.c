/**
 * @file    uart3.c
 * @brief   USART3 HWLINK link on the ST-LINK VCP.
 * @details Receive is a 4 KB circular DMA ring read by the Comms task, so no
 *          byte is lost while it builds a reply. Transmit is DMA-driven and
 *          only the caller waits. The RX ring lives in the non-cacheable
 *          .dma_buffer window (cache_mpu.c).
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (PD8 TX, PD9 RX)
 * @srs     COM-001, COM-002, DCON-004
 */

#include "stm32f7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "uart3.h"

#define UART_RX_RING_SIZE       4096U /**< RX DMA ring size, bytes. */
#define UART_TX_TIMEOUT_MS      200U  /**< TX timeout, ms (currently unused). */

static UART_HandleTypeDef huart3;         /**< USART3 HAL handle. */
static DMA_HandleTypeDef hdma_usart3_rx;  /**< DMA1 Stream1 ch4, circular RX. */
static DMA_HandleTypeDef hdma_usart3_tx;  /**< DMA1 Stream3 ch4, normal TX. */

/** RX DMA ring in the non-cacheable window, cache-line aligned. */
static uint8_t uart_rx_ring[UART_RX_RING_SIZE]
    __attribute__((section(".dma_buffer"), aligned(32)));
static uint32_t uart_rx_read;           /**< Next ring index to read. */
static uint32_t uart_rx_consumed;       /**< Bytes consumed since arming. */
static volatile uint32_t uart_rx_wraps; /**< Full-ring DMA completions since arming. */
static uint32_t uart_baud = APP_UART_DEFAULT_BAUD; /**< Current baud rate. */
static volatile TaskHandle_t uart_tx_task; /**< Task waiting on TX, or NULL. */

/** @brief Configure PD8/PD9 as USART3 TX/RX (AF7, pull-up). */
static void uart_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;   /* PD8 TX, PD9 RX */
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &gpio);
}

/** @brief Configure the RX and TX DMA streams and their interrupts. */
static void uart_dma_init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_usart3_rx.Instance                 = DMA1_Stream1;
    hdma_usart3_rx.Init.Channel             = DMA_CHANNEL_4;
    hdma_usart3_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart3_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart3_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart3_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart3_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart3_rx.Init.Mode                = DMA_CIRCULAR;
    hdma_usart3_rx.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
    hdma_usart3_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    hdma_usart3_rx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_1QUARTERFULL;
    hdma_usart3_rx.Init.MemBurst            = DMA_MBURST_SINGLE;
    hdma_usart3_rx.Init.PeriphBurst         = DMA_PBURST_SINGLE;
    (void)HAL_DMA_Init(&hdma_usart3_rx);

    hdma_usart3_tx.Instance                 = DMA1_Stream3;
    hdma_usart3_tx.Init.Channel             = DMA_CHANNEL_4;
    hdma_usart3_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_usart3_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart3_tx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart3_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart3_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart3_tx.Init.Mode                = DMA_NORMAL;
    hdma_usart3_tx.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_usart3_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    hdma_usart3_tx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_1QUARTERFULL;
    hdma_usart3_tx.Init.MemBurst            = DMA_MBURST_SINGLE;
    hdma_usart3_tx.Init.PeriphBurst         = DMA_PBURST_SINGLE;
    (void)HAL_DMA_Init(&hdma_usart3_tx);

    __HAL_LINKDMA(&huart3, hdmarx, hdma_usart3_rx);
    __HAL_LINKDMA(&huart3, hdmatx, hdma_usart3_tx);

    HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
}

/**
 * @brief HAL MSP hook: clocks, pins, DMA and NVIC for USART3.
 * @param[in] huart UART handle; other instances are ignored.
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART3) {
        return;
    }

    __HAL_RCC_USART3_CLK_ENABLE();
    uart_gpio_init();
    uart_dma_init();

    HAL_NVIC_SetPriority(USART3_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
}

/**
 * @brief Reset the read state and arm the circular DMA receiver.
 * @return true when the receiver is armed.
 */
static bool uart_start_receiver(void)
{
    uart_rx_read = 0U;
    uart_rx_consumed = 0U;
    uart_rx_wraps = 0U;
    if (HAL_UART_Receive_DMA(&huart3, uart_rx_ring, UART_RX_RING_SIZE) !=
        HAL_OK) {
        return false;
    }
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_ERR);
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
    return true;
}

/**
 * @brief Start USART3 at the default rate and arm the receiver.
 * @return true on success.
 */
bool app_uart3_init(void)
{
    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = APP_UART_DEFAULT_BAUD;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart3) != HAL_OK) {
        return false;
    }
    uart_baud = APP_UART_DEFAULT_BAUD;
    return uart_start_receiver();
}

/**
 * @brief Re-initialise USART3 at a supported rate.
 * @return true on success; on failure the old rate is restored.
 */
bool app_uart3_set_baud(uint32_t baud)
{
    if (baud != APP_UART_DEFAULT_BAUD && baud != APP_UART_FAST_BAUD) {
        return false;
    }
    if (baud == uart_baud) {
        return true;
    }

    uint32_t old_baud = uart_baud;
    (void)HAL_UART_DMAStop(&huart3);
    (void)HAL_UART_DeInit(&huart3);

    huart3.Init.BaudRate = baud;
    bool ok = HAL_UART_Init(&huart3) == HAL_OK && uart_start_receiver();
    if (!ok) {

        (void)HAL_UART_DeInit(&huart3);
        huart3.Init.BaudRate = old_baud;
        if (HAL_UART_Init(&huart3) == HAL_OK) {
            (void)uart_start_receiver();
        }
        return false;
    }

    uart_baud = baud;
    return true;
}

/** @brief Get the current baud rate. @return Baud rate, bit/s. */
uint32_t app_uart3_baud(void)
{
    return uart_baud;
}

/**
 * @brief Copy new bytes out of the RX ring.
 * @return Bytes copied; 0 after an overrun resync.
 */
uint32_t app_uart3_rx_read(uint8_t *dst, uint32_t max_len)
{
    uint32_t head = UART_RX_RING_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart3_rx);
    head %= UART_RX_RING_SIZE;
    uint32_t produced = (uart_rx_wraps * UART_RX_RING_SIZE) + head;

    if (produced == uart_rx_consumed || max_len == 0U) {
        return 0U;
    }
    if ((produced - uart_rx_consumed) > UART_RX_RING_SIZE) {
        /* DMA lapped the reader: resync at the write position rather than
         * return overwritten data. */
        uart_rx_read = head;
        uart_rx_consumed = produced;
        return 0U;
    }

    uint32_t count = produced - uart_rx_consumed;
    if (count > max_len) {
        count = max_len;
        produced = uart_rx_consumed + count;
    }
    for (uint32_t i = 0U; i < count; i++) {
        dst[i] = uart_rx_ring[(uart_rx_read + i) % UART_RX_RING_SIZE];
    }
    uart_rx_read = (uart_rx_read + count) % UART_RX_RING_SIZE;
    uart_rx_consumed = produced;
    return count;
}

/**
 * @brief Check HAL, DMA and RE state of the receiver.
 * @return true while the receiver is armed.
 */
bool app_uart3_rx_running(void)
{
    return huart3.RxState == HAL_UART_STATE_BUSY_RX &&
           hdma_usart3_rx.State == HAL_DMA_STATE_BUSY &&
           (USART3->CR1 & USART_CR1_RE) != 0U;
}

/**
 * @brief Clear errors, abort and re-arm the receiver if it stopped.
 * @return true when the receiver is running.
 */
bool app_uart3_rx_restart(void)
{
    if (app_uart3_rx_running()) {
        return true;
    }
    /* A DMA error or overrun can leave the receiver idle; recover HWLINK
     * without a reset. */
    USART3->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF |
                  USART_ICR_PECF | USART_ICR_IDLECF;
    (void)HAL_UART_AbortReceive(&huart3);
    return uart_start_receiver();
}

/**
 * @brief Send one frame by DMA and wait for the last stop bit.
 * @return true when sent; false on HAL error or timeout.
 */
bool app_uart3_send(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (len == 0U || data == NULL) {
        return true;
    }

    uart_tx_task = xTaskGetCurrentTaskHandle();
    (void)ulTaskNotifyTake(pdTRUE, 0U);   /* drop a stale completion */

    if (HAL_UART_Transmit_DMA(&huart3, (uint8_t *)data, len) != HAL_OK) {
        uart_tx_task = NULL;
        return false;
    }

    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeout_ms)) == 0U) {
        (void)HAL_UART_AbortTransmit(&huart3);
        uart_tx_task = NULL;
        return false;
    }

    /* DBT-005: wait for TC (last stop bit sent), not just the DMA write to
     * TDR; give up after about 10 ms. */
    uint32_t start = HAL_GetTick();
    while ((USART3->ISR & USART_ISR_TC) == 0U) {
        if ((HAL_GetTick() - start) > 10U) {
            break;
        }
    }
    USART3->ICR = USART_ICR_TCCF;

    uart_tx_task = NULL;
    return true;
}

/**
 * @brief RX DMA full-ring callback: count the wrap and wake Comms.
 * @param[in] huart UART handle.
 * @note ISR context.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        uart_rx_wraps++;
        comms_post_rx_from_isr();
    }
}

/**
 * @brief RX DMA half-ring callback: wake Comms.
 * @param[in] huart UART handle.
 * @note ISR context.
 */
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        comms_post_rx_from_isr();
    }
}

/**
 * @brief TX complete callback: notify the waiting task.
 * @param[in] huart UART handle.
 * @note ISR context.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART3 || uart_tx_task == NULL) {
        return;
    }

    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(uart_tx_task, &woken);
    portYIELD_FROM_ISR(woken);
}

/**
 * @brief UART error callback: wake Comms so it can restart the receiver.
 * @param[in] huart UART handle.
 * @note ISR context.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        comms_post_rx_from_isr();
    }
}

/** @brief USART3 interrupt: clear RX errors, finish TX, handle IDLE. */
void USART3_IRQHandler(void)
{
    uint32_t sr = USART3->ISR;
    bool wake = false;

    /* Clear RX errors here, not in the HAL: HAL_UART_IRQHandler() aborts the
     * circular receiver on overrun. Use ICR only; reading RDR would steal a
     * byte from the DMA ring (RM0410 30.6.9). */
    if ((sr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE)) != 0U) {
        USART3->ICR = USART_ICR_ORECF | USART_ICR_FECF |
                      USART_ICR_NCF | USART_ICR_PECF;
        wake = true;
    }

    /* TX complete: the HAL disables TCIE, readies the handle and calls
     * HAL_UART_TxCpltCallback(); this is the only path that ends a DMA
     * transmit. Safe now that the error flags are clear. TC stays set so
     * app_uart3_send() can wait for the last stop bit (DBT-005). */
    if (((sr & USART_ISR_TC) != 0U) &&
        ((USART3->CR1 & USART_CR1_TCIE) != 0U)) {
        HAL_UART_IRQHandler(&huart3);
    }

    if ((USART3->ISR & USART_ISR_IDLE) != 0U) {
        USART3->ICR = USART_ICR_IDLECF;
        wake = true;
    }
    if (wake) {
        comms_post_rx_from_isr();
    }
}

/** @brief USART3 RX DMA stream interrupt. */
void DMA1_Stream1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart3_rx);
}

/** @brief USART3 TX DMA stream interrupt. */
void DMA1_Stream3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart3_tx);
}
