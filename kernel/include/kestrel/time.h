#ifndef KESTREL_TIME_H
#define KESTREL_TIME_H
#include <stdint.h>
#include <stdbool.h>
extern volatile bool kestrel_started;
uint32_t kestrel_get_ticks(void);
void kestrel_delay_ms(uint32_t ms);
#endif
