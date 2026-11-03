#ifndef KESTREL_PORT_H
#define KESTREL_PORT_H
#include <stdint.h>
void port_init(void);

void port_yield(void);

void kestrel_schedule(void); // Function is defines in the kernel
uint32_t port_enter_critical(void);
void port_exit_critical(uint32_t old_basepri);
#endif
