#include "kestrel/time.h"
#include "kestrel/task.h"
#include <stdbool.h>
#include "kestrel_internal.h"
static volatile uint32_t ticks = 0;
bool kestrel_tick(void)
{
    ticks++;
    kestrel_wake_sleeping(ticks);
    return kestrel_started;
}
uint32_t kestrel_get_ticks(void)
{
    return ticks;
}
void kestrel_delay_ms(uint32_t ms)
{
    uint32_t prev = ticks;

    while (ticks - prev < ms)
        ;
}
