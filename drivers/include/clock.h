#ifndef KESTREL_CLOCK_H
#define KESTREL_CLOCK_H
#include <stdint.h>

#define CLOCK_READY_TIMEOUT 100000

typedef enum {
    CLOCK_OK = 0,
    CLOCK_ERR_FLASH_LATENCY = -1,
    CLOCK_ERR_HSE_TIMEOUT = -2,
    CLOCK_ERR_PLL_TIMEOUT = -3,
    CLOCK_ERR_SWITCH_TIMEOUT = -4,
} clock_status_t;

/* HSE bypass 8 MHz -> PLL x9 -> 72 MHz SYSCLK, APB1 36 MHz. Returns CLOCK_OK or the failed step. */
clock_status_t clock_setup_72MHz(void);

#endif
