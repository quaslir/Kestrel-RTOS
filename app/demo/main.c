#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "clock.h"
#include "gpio.h"
#include "systick.h"
#include "uart.h"

#include "kestrel/task.h"
#include "kestrel/time.h"

#define LED_PORT GPIOA
#define LED_PIN  5U

#define PRIO_WORKER   1U
#define PRIO_REPORTER 2U

#define FPU_TASKS           3U
#define SPIN_LOOPS          100000U
#define REPORT_PERIOD_TICKS 1000U

static volatile uint32_t fpu_checks[FPU_TASKS];
static volatile uint32_t fpu_errors[FPU_TASKS];
static volatile uint32_t int_checks;
static volatile uint32_t int_errors;

static void uart_put_u32(uint32_t value)
{
    char buf[10];
    uint8_t len = 0;

    do {
        buf[len++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);

    while (len > 0U) {
        uart_putc(buf[--len]);
    }
}

static void output_pin_init(GPIO_TypeDef *port, uint8_t pin)
{
    gpio_enable_port(port);
    gpio_set_mode(port, pin, GPIO_MODE_OUTPUT);
    gpio_pull(port, pin, GPIO_PULL_NONE);
    gpio_put(port, pin, false);
}

static void fail(void)
{
    output_pin_init(LED_PORT, LED_PIN);
    gpio_put(LED_PORT, LED_PIN, true);
    while (1) {
    }
}

static void fpu_task(void *arg)
{
    uint32_t id = (uint32_t)arg;
    uint32_t pattern[32];
    uint32_t result[32];
    uint32_t round = 0;

    while (1) {
        for (uint32_t i = 0; i < 32U; i++) {
            pattern[i] = (id << 28) | (i << 20) | (round & 0xFFFFFU);
        }

        uint32_t n = SPIN_LOOPS;
        __asm volatile("vldmia %[in], {s0-s31}\n"
                       "1:\n"
                       "subs %[n], %[n], #1\n"
                       "bne 1b\n"
                       "vstmia %[out], {s0-s31}\n"
                       : [n] "+r"(n)
                       : [in] "r"(pattern), [out] "r"(result)
                       : "memory", "cc", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9",
                         "d10", "d11", "d12", "d13", "d14", "d15");

        for (uint32_t i = 0; i < 32U; i++) {
            if (result[i] != pattern[i]) {
                fpu_errors[id]++;
                break;
            }
        }
        fpu_checks[id]++;
        round++;
    }
}

static void int_task(void *arg)
{
    (void)arg;
    uint32_t round = 0;

    while (1) {
        uint32_t n = SPIN_LOOPS;
        uint32_t r4_in = 0xA0000000U | round;
        uint32_t r4_out;
        __asm volatile("mov r4, %[v]\n"
                       "1:\n"
                       "subs %[n], %[n], #1\n"
                       "bne 1b\n"
                       "mov %[out], r4\n"
                       : [n] "+r"(n), [out] "=r"(r4_out)
                       : [v] "r"(r4_in)
                       : "cc", "r4");

        if (r4_out != r4_in) {
            int_errors++;
        }
        int_checks++;
        round++;
    }
}

static void reporter_task(void *arg)
{
    (void)arg;
    uint32_t last = kestrel_get_ticks();

    while (1) {
        kestrel_delay_until(&last, REPORT_PERIOD_TICKS);

        uart_puts("t=");
        uart_put_u32(last / 1000U);
        uart_puts("s");
        for (uint32_t i = 0; i < FPU_TASKS; i++) {
            uart_puts(" f");
            uart_put_u32(i);
            uart_puts("=");
            uart_put_u32(fpu_checks[i]);
            uart_puts("/");
            uart_put_u32(fpu_errors[i]);
        }
        uart_puts(" int=");
        uart_put_u32(int_checks);
        uart_puts("/");
        uart_put_u32(int_errors);
        uart_puts("\r\n");
    }
}

static void create_task(kestrel_task_fn_t fn, void *arg, uint8_t priority)
{
    if (kestrel_new_task(fn, arg, priority).status != KESTREL_OK) {
        fail();
    }
}

int main(void)
{
    if (clock_setup_72MHz() != CLOCK_OK) {
        fail();
    }
    systick_init(SystemCoreClock);

    output_pin_init(LED_PORT, LED_PIN);

    if (!uart_init(115200U)) {
        fail();
    }
    uart_puts("\r\n=== Kestrel M5.1: FPU context ===\r\n");

    for (uint32_t i = 0; i < FPU_TASKS; i++) {
        create_task(fpu_task, (void *)i, PRIO_WORKER);
    }
    create_task(int_task, NULL, PRIO_WORKER);
    create_task(reporter_task, NULL, PRIO_REPORTER);
    kestrel_start();

    while (1) {
    }
}
