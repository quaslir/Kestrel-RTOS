/*
 * startup_stm32f303.c — reset handler and vector table for STM32F303xE
 *
 * Runs from reset to main():
 *   1. enable the FPU (CP10/CP11 full access)
 *   2. copy .data initial values from FLASH to RAM
 *   3. zero .bss
 *   4. call main()
 *
 * Every handler that is not implemented elsewhere falls back to
 * Default_Handler through a weak alias. Defining a function with the same
 * name anywhere in the project (e.g. SysTick_Handler) overrides it.
 */

#include <stdint.h>

#include "stm32f3xx.h"

/* stm32f303xe.h aliases USBWakeUp_IRQHandler to USBWakeUp_RMP_IRQHandler for
 * backward compatibility. Here both vectors (IRQ 42 and IRQ 76) need their own
 * names, so the alias is removed for this file only. */
#undef USBWakeUp_IRQHandler

/* ------------------------------------------------------------------------ */
/* Symbols from the linker script. They are addresses, not variables:       */
/* always use &symbol, never its value.                                     */
/* ------------------------------------------------------------------------ */
extern uint32_t _estack; /* top of the main stack (initial MSP)  */
extern uint32_t _sidata; /* .data initial values in FLASH (LMA)  */
extern uint32_t _sdata;  /* .data start in RAM (VMA)             */
extern uint32_t _edata;  /* .data end in RAM                     */
extern uint32_t _sbss;   /* .bss start                           */
extern uint32_t _ebss;   /* .bss end                             */

int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/* ------------------------------------------------------------------------ */
/* Reset handler                                                            */
/* ------------------------------------------------------------------------ */
void Reset_Handler(void)
{
    /* 1. FPU first: it touches no globals, and after this point the
     *    compiler is free to use floating-point registers anywhere. */
    SCB->CPACR |= (3UL << (10U * 2U)) | (3UL << (11U * 2U));
    __DSB(); /* make sure the write has completed      */
    __ISB(); /* flush the pipeline: new setting applies */

    /* 2. Copy initialised data from FLASH to RAM, one word at a time. */
    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata;) {
        *dst++ = *src++;
    }

    /* 3. Zero the .bss section. */
    for (uint32_t *dst = &_sbss; dst < &_ebss;) {
        *dst++ = 0U;
    }

    /* 4. Enter the application. */
    (void)main();

    /* main() must never return on a microcontroller. */
    for (;;) {
    }
}

/* ------------------------------------------------------------------------ */
/* Default handler: any unexpected exception or interrupt ends up here.     */
/* In the debugger, check SCB->ICSR (VECTACTIVE) to see which one fired.    */
/* ------------------------------------------------------------------------ */
void Default_Handler(void)
{
    for (;;) {
    }
}

#define WEAK_ALIAS __attribute__((weak, alias("Default_Handler")))

/* Cortex-M4 system exceptions */
void NMI_Handler(void) WEAK_ALIAS;
void HardFault_Handler(void) WEAK_ALIAS;
void MemManage_Handler(void) WEAK_ALIAS;
void BusFault_Handler(void) WEAK_ALIAS;
void UsageFault_Handler(void) WEAK_ALIAS;
void SVC_Handler(void) WEAK_ALIAS;
void DebugMon_Handler(void) WEAK_ALIAS;
void PendSV_Handler(void) WEAK_ALIAS;
void SysTick_Handler(void) WEAK_ALIAS;

/* STM32F303xE peripheral interrupts (names match IRQn_Type in stm32f303xe.h) */
void WWDG_IRQHandler(void) WEAK_ALIAS;
void PVD_IRQHandler(void) WEAK_ALIAS;
void TAMP_STAMP_IRQHandler(void) WEAK_ALIAS;
void RTC_WKUP_IRQHandler(void) WEAK_ALIAS;
void FLASH_IRQHandler(void) WEAK_ALIAS;
void RCC_IRQHandler(void) WEAK_ALIAS;
void EXTI0_IRQHandler(void) WEAK_ALIAS;
void EXTI1_IRQHandler(void) WEAK_ALIAS;
void EXTI2_TSC_IRQHandler(void) WEAK_ALIAS;
void EXTI3_IRQHandler(void) WEAK_ALIAS;
void EXTI4_IRQHandler(void) WEAK_ALIAS;
void DMA1_Channel1_IRQHandler(void) WEAK_ALIAS;
void DMA1_Channel2_IRQHandler(void) WEAK_ALIAS;
void DMA1_Channel3_IRQHandler(void) WEAK_ALIAS;
void DMA1_Channel4_IRQHandler(void) WEAK_ALIAS;
void DMA1_Channel5_IRQHandler(void) WEAK_ALIAS;
void DMA1_Channel6_IRQHandler(void) WEAK_ALIAS;
void DMA1_Channel7_IRQHandler(void) WEAK_ALIAS;
void ADC1_2_IRQHandler(void) WEAK_ALIAS;
void USB_HP_CAN_TX_IRQHandler(void) WEAK_ALIAS;
void USB_LP_CAN_RX0_IRQHandler(void) WEAK_ALIAS;
void CAN_RX1_IRQHandler(void) WEAK_ALIAS;
void CAN_SCE_IRQHandler(void) WEAK_ALIAS;
void EXTI9_5_IRQHandler(void) WEAK_ALIAS;
void TIM1_BRK_TIM15_IRQHandler(void) WEAK_ALIAS;
void TIM1_UP_TIM16_IRQHandler(void) WEAK_ALIAS;
void TIM1_TRG_COM_TIM17_IRQHandler(void) WEAK_ALIAS;
void TIM1_CC_IRQHandler(void) WEAK_ALIAS;
void TIM2_IRQHandler(void) WEAK_ALIAS;
void TIM3_IRQHandler(void) WEAK_ALIAS;
void TIM4_IRQHandler(void) WEAK_ALIAS;
void I2C1_EV_IRQHandler(void) WEAK_ALIAS;
void I2C1_ER_IRQHandler(void) WEAK_ALIAS;
void I2C2_EV_IRQHandler(void) WEAK_ALIAS;
void I2C2_ER_IRQHandler(void) WEAK_ALIAS;
void SPI1_IRQHandler(void) WEAK_ALIAS;
void SPI2_IRQHandler(void) WEAK_ALIAS;
void USART1_IRQHandler(void) WEAK_ALIAS;
void USART2_IRQHandler(void) WEAK_ALIAS;
void USART3_IRQHandler(void) WEAK_ALIAS;
void EXTI15_10_IRQHandler(void) WEAK_ALIAS;
void RTC_Alarm_IRQHandler(void) WEAK_ALIAS;
void USBWakeUp_IRQHandler(void) WEAK_ALIAS;
void TIM8_BRK_IRQHandler(void) WEAK_ALIAS;
void TIM8_UP_IRQHandler(void) WEAK_ALIAS;
void TIM8_TRG_COM_IRQHandler(void) WEAK_ALIAS;
void TIM8_CC_IRQHandler(void) WEAK_ALIAS;
void ADC3_IRQHandler(void) WEAK_ALIAS;
void FMC_IRQHandler(void) WEAK_ALIAS;
void SPI3_IRQHandler(void) WEAK_ALIAS;
void UART4_IRQHandler(void) WEAK_ALIAS;
void UART5_IRQHandler(void) WEAK_ALIAS;
void TIM6_DAC_IRQHandler(void) WEAK_ALIAS;
void TIM7_IRQHandler(void) WEAK_ALIAS;
void DMA2_Channel1_IRQHandler(void) WEAK_ALIAS;
void DMA2_Channel2_IRQHandler(void) WEAK_ALIAS;
void DMA2_Channel3_IRQHandler(void) WEAK_ALIAS;
void DMA2_Channel4_IRQHandler(void) WEAK_ALIAS;
void DMA2_Channel5_IRQHandler(void) WEAK_ALIAS;
void ADC4_IRQHandler(void) WEAK_ALIAS;
void COMP1_2_3_IRQHandler(void) WEAK_ALIAS;
void COMP4_5_6_IRQHandler(void) WEAK_ALIAS;
void COMP7_IRQHandler(void) WEAK_ALIAS;
void I2C3_EV_IRQHandler(void) WEAK_ALIAS;
void I2C3_ER_IRQHandler(void) WEAK_ALIAS;
void USB_HP_IRQHandler(void) WEAK_ALIAS;
void USB_LP_IRQHandler(void) WEAK_ALIAS;
void USBWakeUp_RMP_IRQHandler(void) WEAK_ALIAS;
void TIM20_BRK_IRQHandler(void) WEAK_ALIAS;
void TIM20_UP_IRQHandler(void) WEAK_ALIAS;
void TIM20_TRG_COM_IRQHandler(void) WEAK_ALIAS;
void TIM20_CC_IRQHandler(void) WEAK_ALIAS;
void FPU_IRQHandler(void) WEAK_ALIAS;
void SPI4_IRQHandler(void) WEAK_ALIAS;

/* ------------------------------------------------------------------------ */
/* Vector table — placed at the very start of FLASH by the linker script.   */
/* Index in comments = exception number (system) or IRQn (peripheral).      */
/* ------------------------------------------------------------------------ */
typedef void (*vector_t)(void);

#define VECTOR_COUNT (16U + 85U) /* 16 system entries + IRQ 0..84 */

__attribute__((section(".isr_vector"), used))
const vector_t vector_table[] = {
    /* ---- Cortex-M4 system exceptions ---- */
    (vector_t)(uintptr_t)&_estack, /*  0  initial MSP value     */
    Reset_Handler,                 /*  1  Reset                 */
    NMI_Handler,                   /*  2  NMI                   */
    HardFault_Handler,             /*  3  HardFault             */
    MemManage_Handler,             /*  4  MemManage             */
    BusFault_Handler,              /*  5  BusFault              */
    UsageFault_Handler,            /*  6  UsageFault            */
    0,                             /*  7  reserved              */
    0,                             /*  8  reserved              */
    0,                             /*  9  reserved              */
    0,                             /* 10  reserved              */
    SVC_Handler,                   /* 11  SVCall                */
    DebugMon_Handler,              /* 12  Debug monitor         */
    0,                             /* 13  reserved              */
    PendSV_Handler,                /* 14  PendSV                */
    SysTick_Handler,               /* 15  SysTick               */

    /* ---- STM32F303xE peripheral interrupts ---- */
    WWDG_IRQHandler,               /* IRQ  0 */
    PVD_IRQHandler,                /* IRQ  1 */
    TAMP_STAMP_IRQHandler,         /* IRQ  2 */
    RTC_WKUP_IRQHandler,           /* IRQ  3 */
    FLASH_IRQHandler,              /* IRQ  4 */
    RCC_IRQHandler,                /* IRQ  5 */
    EXTI0_IRQHandler,              /* IRQ  6 */
    EXTI1_IRQHandler,              /* IRQ  7 */
    EXTI2_TSC_IRQHandler,          /* IRQ  8 */
    EXTI3_IRQHandler,              /* IRQ  9 */
    EXTI4_IRQHandler,              /* IRQ 10 */
    DMA1_Channel1_IRQHandler,      /* IRQ 11 */
    DMA1_Channel2_IRQHandler,      /* IRQ 12 */
    DMA1_Channel3_IRQHandler,      /* IRQ 13 */
    DMA1_Channel4_IRQHandler,      /* IRQ 14 */
    DMA1_Channel5_IRQHandler,      /* IRQ 15 */
    DMA1_Channel6_IRQHandler,      /* IRQ 16 */
    DMA1_Channel7_IRQHandler,      /* IRQ 17 */
    ADC1_2_IRQHandler,             /* IRQ 18 */
    USB_HP_CAN_TX_IRQHandler,      /* IRQ 19 */
    USB_LP_CAN_RX0_IRQHandler,     /* IRQ 20 */
    CAN_RX1_IRQHandler,            /* IRQ 21 */
    CAN_SCE_IRQHandler,            /* IRQ 22 */
    EXTI9_5_IRQHandler,            /* IRQ 23 */
    TIM1_BRK_TIM15_IRQHandler,     /* IRQ 24 */
    TIM1_UP_TIM16_IRQHandler,      /* IRQ 25 */
    TIM1_TRG_COM_TIM17_IRQHandler, /* IRQ 26 */
    TIM1_CC_IRQHandler,            /* IRQ 27 */
    TIM2_IRQHandler,               /* IRQ 28 */
    TIM3_IRQHandler,               /* IRQ 29 */
    TIM4_IRQHandler,               /* IRQ 30 */
    I2C1_EV_IRQHandler,            /* IRQ 31 */
    I2C1_ER_IRQHandler,            /* IRQ 32 */
    I2C2_EV_IRQHandler,            /* IRQ 33 */
    I2C2_ER_IRQHandler,            /* IRQ 34 */
    SPI1_IRQHandler,               /* IRQ 35 */
    SPI2_IRQHandler,               /* IRQ 36 */
    USART1_IRQHandler,             /* IRQ 37 */
    USART2_IRQHandler,             /* IRQ 38 */
    USART3_IRQHandler,             /* IRQ 39 */
    EXTI15_10_IRQHandler,          /* IRQ 40 */
    RTC_Alarm_IRQHandler,          /* IRQ 41 */
    USBWakeUp_IRQHandler,          /* IRQ 42 */
    TIM8_BRK_IRQHandler,           /* IRQ 43 */
    TIM8_UP_IRQHandler,            /* IRQ 44 */
    TIM8_TRG_COM_IRQHandler,       /* IRQ 45 */
    TIM8_CC_IRQHandler,            /* IRQ 46 */
    ADC3_IRQHandler,               /* IRQ 47 */
    FMC_IRQHandler,                /* IRQ 48 */
    0,                             /* IRQ 49  reserved */
    0,                             /* IRQ 50  reserved */
    SPI3_IRQHandler,               /* IRQ 51 */
    UART4_IRQHandler,              /* IRQ 52 */
    UART5_IRQHandler,              /* IRQ 53 */
    TIM6_DAC_IRQHandler,           /* IRQ 54 */
    TIM7_IRQHandler,               /* IRQ 55 */
    DMA2_Channel1_IRQHandler,      /* IRQ 56 */
    DMA2_Channel2_IRQHandler,      /* IRQ 57 */
    DMA2_Channel3_IRQHandler,      /* IRQ 58 */
    DMA2_Channel4_IRQHandler,      /* IRQ 59 */
    DMA2_Channel5_IRQHandler,      /* IRQ 60 */
    ADC4_IRQHandler,               /* IRQ 61 */
    0,                             /* IRQ 62  reserved */
    0,                             /* IRQ 63  reserved */
    COMP1_2_3_IRQHandler,          /* IRQ 64 */
    COMP4_5_6_IRQHandler,          /* IRQ 65 */
    COMP7_IRQHandler,              /* IRQ 66 */
    0,                             /* IRQ 67  reserved */
    0,                             /* IRQ 68  reserved */
    0,                             /* IRQ 69  reserved */
    0,                             /* IRQ 70  reserved */
    0,                             /* IRQ 71  reserved */
    I2C3_EV_IRQHandler,            /* IRQ 72 */
    I2C3_ER_IRQHandler,            /* IRQ 73 */
    USB_HP_IRQHandler,             /* IRQ 74 */
    USB_LP_IRQHandler,             /* IRQ 75 */
    USBWakeUp_RMP_IRQHandler,      /* IRQ 76 */
    TIM20_BRK_IRQHandler,          /* IRQ 77 */
    TIM20_UP_IRQHandler,           /* IRQ 78 */
    TIM20_TRG_COM_IRQHandler,      /* IRQ 79 */
    TIM20_CC_IRQHandler,           /* IRQ 80 */
    FPU_IRQHandler,                /* IRQ 81 */
    0,                             /* IRQ 82  reserved */
    0,                             /* IRQ 83  reserved */
    SPI4_IRQHandler,               /* IRQ 84 */
};

/* Catch a missing or extra entry at compile time. */
_Static_assert(sizeof(vector_table) / sizeof(vector_table[0]) == VECTOR_COUNT,
               "vector table size does not match STM32F303xE (16 + 85 entries)");

/* Cross-check against the CMSIS device header: SPI4 must be the last IRQ. */
_Static_assert(SPI4_IRQn == 84, "unexpected IRQ numbering in stm32f303xe.h");
