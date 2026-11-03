#ifndef KESTREL_TIME_H
#define KESTREL_TIME_H
#include <stdint.h>
#include <stdbool.h>
extern volatile bool started;
uint32_t kestrel_get_time(void);
void kestrel_delay_ms(uint32_t ms);
bool kestrel_tick(void);
#endif
