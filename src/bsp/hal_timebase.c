/**
 * @file    hal_timebase.c
 * @brief   1 ms HAL tick on TIM6.
 * @details SysTick belongs to FreeRTOS (DRT-001), so the HAL time base uses
 *          TIM6, as in the validation firmware (HWVAL GEN-012).
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DGEN-006
 */

#include "stm32f7xx_hal.h"

/** @brief TIM6 update interrupt: advance the HAL tick. */
void TIM6_DAC_IRQHandler(void);

/**
 * @brief Start TIM6 as a 1 ms HAL tick (overrides the HAL weak version).
 * @param[in] TickPriority NVIC priority of the tick interrupt.
 * @return HAL_OK always.
 */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    /* APB timers run at PCLK x2 unless the APB prescaler is 1 (RM0410). */
    uint32_t timclk = ((RCC->CFGR & RCC_CFGR_PPRE1) == 0U) ? pclk1 : pclk1 * 2U;
    uint32_t prescaler = (timclk / 1000000U) - 1U;

    __HAL_RCC_TIM6_CLK_ENABLE();

    TIM6->CR1 = 0U;
    TIM6->PSC = prescaler;       /* 1 MHz count */
    TIM6->ARR = 1000U - 1U;      /* 1 ms period (uwTickFreq 1 kHz) */
    TIM6->EGR = TIM_EGR_UG;      /* latch PSC/ARR, reset counter */
    TIM6->SR  = 0U;
    TIM6->DIER = TIM_DIER_UIE;
    TIM6->CR1 = TIM_CR1_CEN | TIM_CR1_URS;

    uwTickPrio = TickPriority;
    NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority);
    NVIC_EnableIRQ(TIM6_DAC_IRQn);

    return HAL_OK;
}

/** @brief Stop the HAL tick interrupt. */
void HAL_SuspendTick(void)
{
    TIM6->DIER &= ~TIM_DIER_UIE;
}

/** @brief Clear a pending update and restart the HAL tick interrupt. */
void HAL_ResumeTick(void)
{
    TIM6->SR = 0U;
    TIM6->DIER |= TIM_DIER_UIE;
}

/** @brief Advance uwTick by uwTickFreq on each TIM6 update. */
void TIM6_DAC_IRQHandler(void)
{
    if ((TIM6->SR & TIM_SR_UIF) != 0U) {
        TIM6->SR = ~TIM_SR_UIF;
        uwTick += uwTickFreq;
    }
}
