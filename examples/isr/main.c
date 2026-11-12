/*
 * Deferred interrupt handling: TIM2 interrupts at 10 Hz and only gives a
 * binary semaphore; a task waiting on it does the work (toggles the LED).
 *
 * TIM2's NVIC priority (6) is numerically above the kernel threshold (5),
 * which is required for any interrupt that calls the kernel.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32f3xx.h"

#include "clock.h"
#include "gpio.h"
#include "systick.h"

#include "kestrel/semaphore.h"
#include "kestrel/task.h"

#define LED_PORT GPIOA
#define LED_PIN  5U

static kestrel_sem_t tick_sem;

void TIM2_IRQHandler(void)
{
    TIM2->SR = ~TIM_SR_UIF;
    kestrel_sem_give(&tick_sem);
}

static void tim2_start_10hz(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    TIM2->PSC = 7200U - 1U; /* 72 MHz / 7200 = 10 kHz */
    TIM2->ARR = 1000U - 1U; /* 10 kHz / 1000 = 10 Hz */
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0U;
    TIM2->DIER = TIM_DIER_UIE;
    NVIC_SetPriority(TIM2_IRQn, 6U);
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->CR1 = TIM_CR1_CEN;
}

static void led_task(void *arg)
{
    (void)arg;
    bool on = false;
    tim2_start_10hz(); /* start interrupts only once the kernel is running */

    while (1) {
        kestrel_sem_take(&tick_sem, KESTREL_WAIT_FOREVER);
        on = !on;
        gpio_put(LED_PORT, LED_PIN, on);
    }
}

int main(void)
{
    clock_setup_72MHz();
    systick_init(SystemCoreClock);

    gpio_enable_port(LED_PORT);
    gpio_set_mode(LED_PORT, LED_PIN, GPIO_MODE_OUTPUT);
    kestrel_sem_init(&tick_sem, 0U, 1U);

    kestrel_new_task(led_task, NULL, 2U);
    kestrel_start();
}
