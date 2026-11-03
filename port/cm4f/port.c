#include "port.h"
#include "stm32f3xx.h"
#include "kestrel/time.h"
__attribute__((naked)) void SVC_Handler(void)
{
    __asm volatile("ldr r0, =kestrel_current\n"
                   "ldr r0, [r0]\n"
                   "ldr r0, [r0]\n"
                   "ldmia r0!, {r4-r11}\n"
                   "msr psp, r0\n"
                   "ldr lr, =0xFFFFFFFD\n"
                   "ldr r1, =started\n"
                   "mov r2, #1\n"
                   "strb r2, [r1]\n"
                   "bx lr");
}

__attribute__((naked)) void PendSV_Handler(void)
{
    __asm volatile("mrs r0, psp\n"
                   "stmdb r0!, {r4-r11}\n"
                   "ldr r1, =kestrel_current\n"
                   "ldr r2, [r1]\n"
                   "str r0, [r2]\n"
                   "push {r3,lr}\n"
                   "bl kestrel_schedule\n"
                   "pop {r3,lr}\n"
                   "ldr r0, =kestrel_current\n"
                   "ldr r0, [r0]\n"
                   "ldr r0, [r0]\n"
                   "ldmia r0!, {r4-r11}\n"
                   "msr psp, r0\n"
                   "bx lr");
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
