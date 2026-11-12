#include <stddef.h>
#include <stdint.h>

#include "stm32f3xx.h"

#include "clock.h"
#include "systick.h"
#include "uart.h"

#include "kestrel/mutex.h"
#include "kestrel/task.h"
#include "kestrel/time.h"

static kestrel_mutex_t uart_mutex;

typedef struct {
    const char *line;
    uint32_t period;
} job_t;

static const job_t jobs[3] = {
    { "[high] sensor read\r\n", 100U },
    { "[mid]  control step\r\n", 250U },
    { "[low]  status report\r\n", 1000U },
};

static void worker(void *arg)
{
    const job_t *job = (const job_t *)arg;
    uint32_t last = kestrel_get_ticks();

    while (1) {
        kestrel_delay_until(&last, job->period);

        if (kestrel_mutex_lock(&uart_mutex, KESTREL_WAIT_FOREVER) == KESTREL_OK) {
            uart_puts(job->line);
            kestrel_mutex_unlock(&uart_mutex);
        }
    }
}

int main(void)
{
    clock_setup_72MHz();
    systick_init(SystemCoreClock);
    uart_init(115200U);
    kestrel_mutex_init(&uart_mutex);

    kestrel_new_task(worker, (void *)&jobs[0], 3U);
    kestrel_new_task(worker, (void *)&jobs[1], 2U);
    kestrel_new_task(worker, (void *)&jobs[2], 1U);
    kestrel_start();
}
