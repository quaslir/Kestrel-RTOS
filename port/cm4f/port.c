#include "port.h"
#include "stm32f3xx.h"
#include "kestrel/time.h"
#include "kestrel_internal.h"
__attribute__((naked)) void SVC_Handler(void)
{
    // clang-format off
    __asm volatile("ldr r0, =kestrel_current\n"
                   "ldr r0, [r0]\n"
                   "ldr r0, [r0]\n"
                   "ldmia r0!, {lr}\n"
                   "ldmia r0!, {r4-r11}\n"
                   "msr psp, r0\n"
                   "ldr r1, =kestrel_started\n"
                   "mov r2, #1\n"
                   "strb r2, [r1]\n"
                   "bx lr");
    // clang-format on
}

__attribute__((naked)) void PendSV_Handler(void)
{
    // clang-format off
    __asm volatile("mrs r0, psp\n"
                   "tst lr, #0x10\n"
                   "it eq\n"
                   "vstmdbeq r0!, {s16-s31}\n"

                   "stmdb r0!, {r4-r11}\n"
                   "stmdb r0!, {lr}\n"

                   "ldr r1, =kestrel_current\n"
                   "ldr r2, [r1]\n"
                   "str r0, [r2]\n"

                   "push {r3,lr}\n"
                   "bl kestrel_schedule\n"
                   "pop {r3,lr}\n"

                   "ldr r0, =kestrel_current\n"
                   "ldr r0, [r0]\n"
                   "ldr r0, [r0]\n"
                   "ldmia r0!, {lr}\n"
                   "ldmia r0!, {r4-r11}\n"
                   "tst lr, #0x10\n"
                   "it eq\n"
                   "vldmiaeq r0!, {s16-s31}\n"
                   "msr psp, r0\n"
                   "bx lr");
    // clang-format on
}

void port_init(void)
{
    SCB->SHP[10] = 0xF0;
    SCB->SHP[11] = 0xF0;
}

void port_yield(void)
{
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    __DSB();
    __ISB();
}

void SysTick_Handler(void)
{
    if (kestrel_tick()) {
        port_yield();
    }
}

uint32_t port_enter_critical(void)
{
    uint32_t old_value = __get_BASEPRI();
    __set_BASEPRI(PORT_MAX_SYSCALL_PRIO);
    __ISB();
    return old_value;
}

void port_exit_critical(uint32_t old_basepri)
{
    __set_BASEPRI(old_basepri);
}

bool port_in_isr(void)
{
    return __get_IPSR() != 0;
}
