#ifndef KESTREL_PORT_H
#define KESTREL_PORT_H

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_gcc.h"
#define PORT_EXC_RETURN_THREAD_PSP 0xFFFFFFFD
#define BASEPRI_TH                 0x50
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
