#include "systick.h"
#include "stm32f3xx.h"

static volatile uint32_t ticks = 0;

void systick_init(uint32_t hz)
{
    uint32_t load = (hz / KESTREL_TICK_HZ) - 1;
    if (load > SysTick_LOAD_RELOAD_Msk) {
        // handle error
        return;
    }
    SysTick->LOAD = load;
    SysTick->VAL = 0;
    SysTick->CTRL =
        (SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk);
}
uint32_t systick_get_time(void)
{
    return ticks;
}
void systick_delay_ms(uint32_t ms)
{
    uint32_t prev = ticks;
    while (ticks - prev < ms) {
    }
}

void SysTick_Handler(void) // interrupt handler
{
    ticks++;
}
