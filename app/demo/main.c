#include <stdint.h>

/* Startup check: after reset, a must be 0x12345678 (copied from FLASH,
 * .data) and b must be 0 (.bss zeroed). Inspect both in GDB at main(). */
uint32_t a = 0x12345678U;
uint32_t b;

int main(void)
{
    for (;;) {
        a++;
        b++;
    }
}
