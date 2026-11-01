#ifndef KESTREL_SYSTICK_H
#define KESTREL_SYSTICK_H
#include <stdint.h>

#define KESTREL_TICK_HZ 1000
void systick_init(uint32_t hz);
uint32_t systick_get_time(void);
void systick_delay_ms(uint32_t ms);
#endif
