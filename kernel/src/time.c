#include "kestrel/time.h"
#include <stdbool.h>
static volatile uint32_t ticks = 0;
volatile bool started = false;
uint32_t kestrel_get_time(void)
{
    return ticks;
}
void kestrel_delay_ms(uint32_t ms)
{
    uint32_t prev = ticks;

    while (ticks - prev < ms)
        ;
}
bool kestrel_tick(void)
{
    ticks++;
    return started;
}
