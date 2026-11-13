#ifndef KESTREL_PORT_H
#define KESTREL_PORT_H

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_gcc.h"
#define PORT_EXC_RETURN_THREAD_PSP 0xFFFFFFFD
/* BASEPRI value used by critical sections. Interrupts that call the kernel must
 * have a priority numerically >= this value: with the 4 priority bits of the
 * STM32F3 that is NVIC_SetPriority(irq, 5) or larger. More urgent interrupts
 * (0..4) are never masked by the kernel and must not call it. */
#define PORT_MAX_SYSCALL_PRIO      0x50
#define CLEAR_BIT_ZERO             (~(1U << 0))
#define INITIAL_XPSR               (1UL << 24)
void port_init(void);

void port_yield(void);

uint32_t port_enter_critical(void);
void port_exit_critical(uint32_t old_basepri);
bool port_in_isr(void);
static inline uint8_t port_highest_priority(uint32_t mask) // correct only if mask is not zero!
{
    return 31 - __CLZ(mask);
}
#endif
