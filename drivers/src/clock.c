#include "stm32f3xx.h"
#include <stdint.h>
#include <stdbool.h>
#include "clock.h"
uint32_t SystemCoreClock = 8000000;
static bool reg_wait(volatile const uint32_t *reg, uint32_t mask, uint32_t value)
{
    for (uint32_t i = 0; i < CLOCK_READY_TIMEOUT; i++) {
        if ((*reg & mask) == value) {
            return true;
        }
    }

    return false;
}

clock_status_t clock_setup_72MHz(void)
{
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= FLASH_ACR_LATENCY_1;
    FLASH->ACR |= FLASH_ACR_PRFTBE;

    if ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_1) {
        return CLOCK_ERR_FLASH_LATENCY;
    }

    // HSE enable

    RCC->CR |= RCC_CR_HSEBYP;
    RCC->CR |= RCC_CR_HSEON;

    if (!reg_wait(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY)) {
        return CLOCK_ERR_HSE_TIMEOUT;
    }
    // Configure PLL

    RCC->CFGR2 &= ~RCC_CFGR2_PREDIV; // clear

    RCC->CFGR &= ~RCC_CFGR_PLLSRC;
    RCC->CFGR |= RCC_CFGR_PLLSRC_HSE_PREDIV;
    RCC->CFGR &= ~RCC_CFGR_PLLMUL;
    RCC->CFGR |= RCC_CFGR_PLLMUL9;
    RCC->CFGR &= ~RCC_CFGR_HPRE;
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;
    RCC->CFGR &= ~RCC_CFGR_PPRE1;
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;
    RCC->CFGR &= ~RCC_CFGR_PPRE2;
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;

    // enable PLL

    RCC->CR |= RCC_CR_PLLON;

    if (!reg_wait(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY)) {
        return CLOCK_ERR_PLL_TIMEOUT;
    }

    // switch the core

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;

    if (!reg_wait(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL)) {
        return CLOCK_ERR_SWITCH_TIMEOUT;
    }

    SystemCoreClock = 72000000;
    return CLOCK_OK;
}
