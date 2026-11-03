#ifndef KESTREL_PORT_H
#define KESTREL_PORT_H

void port_init(void);

void port_yield(void);

void kestrel_schedule(void); // Function is defines in the kernel
#endif
