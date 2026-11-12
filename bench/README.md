# Benchmarks

Firmware that reproduces the numbers in the [README Results](../README.md#results).
All of them run on a NUCLEO-F303RE at 72 MHz, measure with the DWT cycle counter
(1 cycle = 13.9 ns) and print one line per second over the ST-LINK virtual COM port.

| Firmware | Measures | README value |
|---|---|---|
| `bench_ctx_switch` | Task → task switch with `kestrel_yield()` | min 178 / avg 198 / max 352 cycles |
| `bench_ctx_switch_fpu` | Same, both tasks have live FPU state (S16–S31 saved and restored) | 259 cycles, constant |
| `bench_isr_latency` | Interrupt → waiting task, CPU 100% busy | 416–422 cycles |
| `bench_jitter_idle` | Wake-up latency and period of a 1 ms task, idle system | latency 429–489 cycles, period 71963..72058 cycles |
| `bench_jitter` | Same, CPU 100% busy with queue, mutex and 10 kHz interrupt traffic | latency 520 / ~675 / 950 cycles, period 71612..72255 cycles, 0 missed |

## Build, flash, read

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # all published numbers are Release
cmake --build build -j
cmake --build build --target flash_bench_jitter  # or any firmware above

stty -F /dev/ttyACM0 115200 raw -echo
timeout 20 cat /dev/ttyACM0 > jitter.log
```

Each firmware prints a banner (`=== Kestrel bench_jitter ===`) after reset, then one line per second.
Every line has two parts: the last one-second window, and totals since reset (`total ...`).
Statistics are copied and reset inside `KESTREL_CRITICAL` by a reporter task, so a window never mixes two seconds.
The first lines after reset include start-up effects; read the totals after a few seconds.

## What each benchmark does

### `bench_ctx_switch`, `bench_ctx_switch_fpu`

Two tasks of priority 2 ping-pong with `kestrel_yield()`. Before yielding, a task stores the tick count and `DWT->CYCCNT`; the other task reads `DWT->CYCCNT` as soon as it resumes. The FPU variant (same source, `USE_FPU=1`) executes one `vmov s0, rX` in each task, so every switch takes the lazy-stacking path and saves S16–S31.

```
int t=12s n=<samples> dist=<discarded> min=<cycles> avg=<cycles> max=<cycles> | total min=<cycles> avg=<cycles> max=<cycles>
```

- `n` — samples kept in this second, `dist` — samples discarded because a tick fell between the two reads of `CYCCNT` (the SysTick handler would be counted as switch time). The reporter (priority 3) wakes once a second and also disturbs one sample.
- The numbers include about 14 cycles of measurement overhead (`kestrel_get_ticks()` and the `CYCCNT` store before the yield, the `CYCCNT` read after it).

### `bench_isr_latency`

TIM2 interrupts at 100 Hz with NVIC priority 6. The ISR clears its flag, stores `DWT->CYCCNT` and calls `kestrel_sem_give()`. A priority-3 task waiting on the semaphore records `CYCCNT − isr_time` as soon as `kestrel_sem_take()` returns. Two priority-1 busy loops keep the CPU at 100%, so every sample includes preempting a running task. TIM2 is started by the waiting task, after the kernel is running.

On the first interrupt only, the ISR also calls `kestrel_sem_take(&empty_sem, 10)` and `kestrel_mutex_lock(&m, KESTREL_NO_WAIT)`; both must return `KESTREL_ERR_IN_ISR`.

```
isr t=12s n=100 min=<cycles> avg=<cycles> max=<cycles> bg/s=<loops> | total min=<cycles> max=<cycles> | take=IN_ISR lock=IN_ISR
```

`take=WRONG` or `lock=WRONG` means a blocking call from an ISR was not refused.

### `bench_jitter_idle`, `bench_jitter`

A priority-5 task calls `kestrel_delay_until(&last, 1)`. The first thing it does after waking:

- **latency** = `(SysTick->LOAD − SysTick->VAL) × (CLKSOURCE ? 1 : 8)` — cycles since the tick that woke it;
- **period** = `DWT->CYCCNT` delta since the previous wake-up (ideal: 72 000 cycles);

then it busy-works 14 400 cycles (0.2 ms) and counts a **deadline miss** if the tick changed meanwhile. `missed` counts calls where `kestrel_delay_until()` returned `false` (the period had already passed). A reporter at priority 2 prints the statistics.

`bench_jitter` (`LOAD_ENABLED=1`) adds priority-1 load that keeps the CPU 100% busy:

- a producer and a consumer passing 16-byte items through a queue of 8, both with `KESTREL_WAIT_FOREVER`;
- two tasks locking and unlocking one mutex, with a 50-iteration `volatile` spin while holding it;
- TIM2 at 10 kHz (NVIC priority 6) giving a binary semaphore to a priority-1 task.

```
load t=12s lat min=<cycles> avg=<cycles> max=<cycles> per min=<cycles> max=<cycles> missed=0 dl=0 | q/s=<n> mtx/s=<n> isr/s=10000 took/s=<n> | total lat=<min>/<avg>/<max> (max <us>us) per=<min>..<max>
```

`q/s`, `mtx/s`, `isr/s` and `took/s` are queue transfers, mutex lock/unlock cycles, TIM2 interrupts and semaphores taken per second. `bench_jitter_idle` prints `idle` and zeros there.

**Why 16-byte items.** The queue copies items inside a critical section, so the item size bounds the worst-case latency of every higher-priority task. With 16-byte items the worst case is about 950 cycles; with 256-byte items it grows to about 5 500 cycles (76 µs). Pass pointers through the queue for large data.

## Caveats

- Code placement matters at the level of a few cycles: flash wait states and the prefetch buffer make the same code a little faster or slower when the linker moves it. Builds with different sources (or `-ffunction-sections` / `--gc-sections` dropping different functions) can differ by a few cycles from the README.
- Maxima are rare events. The README worst cases for `bench_jitter` come from 10-minute runs; a 20-second capture usually shows a slightly lower maximum.
- Use a `Release` build. `Debug` (`-Og`) numbers are higher and are not published.
