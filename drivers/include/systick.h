#ifndef KESTREL_SYSTICK_H
#define KESTREL_SYSTICK_H
#include <stdint.h>

#define KESTREL_TICK_HZ 1000
void systick_init(uint32_t hz);

#endif
