#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32f3xx.h"

#include "clock.h"
#include "gpio.h"
#include "systick.h"
#include "uart.h"

#include "kestrel/mutex.h"
#include "kestrel/semaphore.h"
#include "kestrel/task.h"
#include "kestrel/time.h"

#define LED_PORT GPIOA
#define LED_PIN  5U

#define PRIO_BACKGROUND 1U
#define PRIO_REPORTER   2U
#define PRIO_HANDLER    3U

#define TIM2_IRQ_PRIO       6U
#define TIM2_PSC            (72U - 1U)
#define TIM2_ARR            (10000U - 1U)
#define REPORT_PERIOD_TICKS 1000U
#define CPU_MHZ             72U

static kestrel_sem_t irq_sem;
static kestrel_sem_t never_sem;
static kestrel_mutex_t isr_mutex;

static volatile uint32_t isr_cycles;
static volatile uint32_t irq_count;
static volatile uint32_t wake_count;
static volatile uint32_t lat_last;
static volatile uint32_t lat_min = 0xFFFFFFFFU;
static volatile uint32_t lat_max;
static volatile kestrel_status_t isr_take_status = KESTREL_OK;
static volatile kestrel_status_t isr_lock_status = KESTREL_OK;
static volatile uint32_t bg_counter[2];

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

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void tim2_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;

    TIM2->PSC = TIM2_PSC;
    TIM2->ARR = TIM2_ARR;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0U;
    TIM2->DIER = TIM_DIER_UIE;

    NVIC_SetPriority(TIM2_IRQn, TIM2_IRQ_PRIO);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 = TIM_CR1_CEN;
}

void TIM2_IRQHandler(void)
{
    TIM2->SR = ~TIM_SR_UIF;
    irq_count++;

    if (irq_count == 1U) {
        isr_take_status = kestrel_sem_take(&never_sem, 10U);
        isr_lock_status = kestrel_mutex_lock(&isr_mutex, KESTREL_NO_WAIT);
    }

    isr_cycles = DWT->CYCCNT;
    (void)kestrel_sem_give(&irq_sem);
}

static void handler_task(void *arg)
{
    (void)arg;
    tim2_init();

    while (1) {
        if (kestrel_sem_take(&irq_sem, KESTREL_WAIT_FOREVER) != KESTREL_OK) {
            fail();
        }
        uint32_t lat = DWT->CYCCNT - isr_cycles;

        lat_last = lat;
        if (lat < lat_min) {
            lat_min = lat;
        }
        if (lat > lat_max) {
            lat_max = lat;
        }
        wake_count++;
    }
}

static void background_task(void *arg)
{
    volatile uint32_t *counter = (volatile uint32_t *)arg;

    while (1) {
        (*counter)++;
    }
}

static void put_status(const char *name, kestrel_status_t status)
{
    uart_puts(name);
    uart_puts(status == KESTREL_ERR_IN_ISR ? "=IN_ISR" : "=WRONG");
}

static void reporter_task(void *arg)
{
    (void)arg;
    uint32_t last = kestrel_get_ticks();

    while (1) {
        kestrel_delay_until(&last, REPORT_PERIOD_TICKS);

        uart_puts("t=");
        uart_put_u32(last / 1000U);
        uart_puts("s irq=");
        uart_put_u32(irq_count);
        uart_puts(" wake=");
        uart_put_u32(wake_count);
        uart_puts(" lat=");
        uart_put_u32(lat_last);
        uart_puts(" min=");
        uart_put_u32(lat_min);
        uart_puts(" max=");
        uart_put_u32(lat_max);
        uart_puts(" cyc max_us=");
        uart_put_u32(lat_max / CPU_MHZ);
        uart_puts(" ");
        put_status("take", isr_take_status);
        uart_puts(" ");
        put_status("lock", isr_lock_status);
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
    dwt_init();

    output_pin_init(LED_PORT, LED_PIN);

    if (!uart_init(115200U)) {
        fail();
    }
    uart_puts("\r\n=== Kestrel M5.2: ISR -> task latency ===\r\n");

    if (kestrel_sem_init(&irq_sem, 0U, 1U) != KESTREL_OK) {
        fail();
    }
    if (kestrel_sem_init(&never_sem, 0U, 1U) != KESTREL_OK) {
        fail();
    }
    if (kestrel_mutex_init(&isr_mutex) != KESTREL_OK) {
        fail();
    }

    create_task(background_task, (void *)&bg_counter[0], PRIO_BACKGROUND);
    create_task(background_task, (void *)&bg_counter[1], PRIO_BACKGROUND);
    create_task(reporter_task, NULL, PRIO_REPORTER);
    create_task(handler_task, NULL, PRIO_HANDLER);
    kestrel_start();

    while (1) {
    }
}
