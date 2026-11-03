#include "systick.h"
#include "stm32f3xx.h"

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
