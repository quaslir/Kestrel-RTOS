# Kestrel RTOS

![CI](https://github.com/quaslir/Kestrel-RTOS/actions/workflows/ci.yml/badge.svg)

A preemptive real-time kernel for ARM Cortex-M4F, written from scratch in C.
Runs on the STM32F303RE (NUCLEO-F303RE) with no HAL and no vendor drivers — only CMSIS register headers, a custom startup file and linker script.

**Goal:** prove with measurements that a high-priority task meets a **1 ms hard deadline** while lower-priority tasks keep the CPU ~90% busy.

**Status:** v1.0 — feature-complete for this goal.

---

## Results

STM32F303RE @ 72 MHz, `Release` build, measured with the DWT cycle counter by the firmware in [`bench/`](bench/README.md). The first 3 seconds after reset are discarded.

| Metric | Result |
|---|---|
| Context switch, task → task (`yield`) | **{CTX_AVG} cycles · {CTX_AVG_US} µs** (min {CTX_MIN}, max {CTX_MAX}) |
| Context switch with FPU state | **{FPU_AVG} cycles · {FPU_AVG_US} µs** (min {FPU_MIN}, max {FPU_MAX}) |
| Interrupt → waiting task | **{ISR_MIN}–{ISR_MAX} cycles · {ISR_US} µs**, CPU 100% busy |
| 1 ms periodic task, wake-up latency, idle system | **{IDLE_MIN}–{IDLE_MAX} cycles · {IDLE_US} µs**, period jitter ±{IDLE_J} cycles (±{IDLE_J_US} µs) |
| 1 ms periodic task, CPU 100% busy with queue, mutex and 10 kHz ISR traffic | **{LOAD_MIN}–{LOAD_MAX} cycles · {LOAD_US} µs**, period jitter ±{LOAD_J} cycles (±{LOAD_J_US} µs), **0 missed deadlines** in 10 min |
| Kernel object code, all APIs compiled (kernel + port) | **2 318 B** (`-Os`) · 2 640 B (`-O2`) |
| Kernel RAM, fixed | **434 B** (incl. 256 B idle stack) |
| RAM per task | **36 B** TCB + stack (1 KB by default) |

Context-switch numbers include ~14 cycles of measurement overhead. Queue items are copied inside a critical section, so the item size bounds the worst-case latency of higher-priority tasks; the benchmark uses 16-byte items ([details](bench/README.md#bench_jitter_idle-bench_jitter)).

### Compared with FreeRTOS

Same board, toolchain, compiler flags and test firmware, ported to FreeRTOS-Kernel V11.3.3 (ARM_CM4F port, static allocation, 32 priorities, same BASEPRI threshold, no timers/heap). Difference = FreeRTOS relative to Kestrel.

| Metric | Kestrel | FreeRTOS | Difference |
|---|---|---|---|
| Context switch (`yield`), avg | {CTX_AVG} cycles | 193 cycles | {D_CTX} % |
| Context switch with FPU state, avg | {FPU_AVG} cycles | 257 cycles | {D_FPU} % |
| Interrupt → waiting task | **{ISR_MIN}–{ISR_MAX} cycles** | 496–526 cycles | {D_ISR} % |
| 1 ms task wake-up latency, idle | **{IDLE_MIN}–{IDLE_MAX} cycles**, jitter {IDLE_J} cycles | 459–535 cycles, jitter 76 cycles | {D_IDLE} % |
| Kernel object code, all APIs compiled, `-Os` / `-O2`¹ | **2 318 / 2 640 B** | 6 719 / 7 776 B | +190 % / +195 % |
| Kernel RAM, fixed (incl. 256 B idle stack) | **434 B** | 1 096 B | +153 % |
| TCB | **36 B** | 84 B: adds a 16-byte task name, task-notification state, and two list items that each carry a pointer to their containing list | +133 % |

Under load both kernels met every deadline; the load mix differed between the two runs, so latencies under load are not compared.

{EXPLANATION}

¹ Sum of the kernel object files; after `--gc-sections` FreeRTOS links 2.2–4.8 KB depending on the APIs used.

---

## Features

- Preemptive scheduler, 32 priority levels, round-robin within a priority
- O(1) task selection: per-priority ready rings + ready bitmap + `CLZ`
- Lazy FPU context switching (S16–S31, per-task `EXC_RETURN`)
- `delay` and drift-free periodic `delay_until`, tick wraparound handled
- Semaphores, mutexes with priority inheritance, message queues — all with timeouts
- Wait lists ordered by priority; direct hand-off on release
- ISR-safe API: wake a task from an interrupt, switch on ISR exit
- Critical sections via `BASEPRI` — interrupts above the kernel threshold are never delayed
- Static allocation only, no heap
- 42 host unit tests (Unity, ASan/UBSan) in CI on every push

**Out of scope:** dynamic memory, file system, networking, SMP, tickless idle, other architectures.

---

## Architecture

```
kernel/    scheduler, time, wait lists, semaphore, mutex, queue     portable C
port/cm4f/ PendSV, SVC, SysTick, BASEPRI, CLZ, FPU                  Cortex-M4F
drivers/   clock, GPIO, SysTick, UART                               register level
startup/   vector table, reset handler, linker script
examples/  small example firmware
bench/     benchmark firmware behind the Results section
test/      host unit tests (Unity)
```

**Context switch.** SysTick, a blocking call or an ISR pends PendSV, which runs at the lowest exception priority after all other interrupts. PendSV saves R4–R11, `EXC_RETURN` and — only for tasks that used the FPU — S16–S31 onto the task's stack, picks the next task with `31 − CLZ(ready_mask)`, and restores it. The hardware saves and restores the rest of the frame.

**Blocking.** A task that cannot get a resource moves from its ready ring to the resource's wait list (sorted by priority) inside a critical section. On release the waker hands the resource directly to the most important waiter and writes its result, so the woken task cannot lose the resource to a third task.

### Key design decisions

| Decision | Reason |
|---|---|
| PendSV at the lowest priority | A context switch never interrupts another ISR; requests coalesce |
| `BASEPRI` instead of `PRIMASK` | The most urgent interrupts are never delayed by the kernel |
| Ready bitmap + `CLZ` + one ring per priority | Selection time independent of task count; fair round-robin per priority |
| Tick pends PendSV only when needed | No useless switches every millisecond |
| Direct hand-off on release | No retry loops, no lost wake-ups |
| Priority inheritance via base + effective priority | Bounded blocking; switchable at build time |
| `EXC_RETURN` saved per task | Tasks with and without FPU state mix freely; non-FPU tasks pay nothing |
| One API for tasks and ISRs (`IPSR` check) | Small API; a blocking call from an ISR returns `KESTREL_ERR_IN_ISR` |
| Pure logic (`ready.c`, `wait_list.c`) separated from hardware | Scheduler core is unit-tested on a PC |

---

## Using Kestrel in your project

Add the repository as a subdirectory and build your firmware with `kestrel_add_firmware()`, which links `Kestrel::kernel` and `Kestrel::drivers`, the startup file and the linker script, and adds a `flash_<name>` target:

```cmake
cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/kestrel/cmake/arm-none-eabi.cmake)
project(my_app C ASM)

set(KESTREL_MAX_TASKS 4 CACHE STRING "")   # optional, see the table below
add_subdirectory(kestrel)
kestrel_add_firmware(my_app src/main.c)
```

To use your own startup code and linker script instead, link `Kestrel::kernel` (and optionally `Kestrel::drivers`) into your own executable. Examples and benchmarks are not built when Kestrel is a subdirectory.

| Option | Default | Effect |
|---|---|---|
| `KESTREL_MAX_TASKS` | `8` | Maximum number of application tasks (idle task not counted) |
| `KESTREL_STACK_WORDS` | `256` | Stack of each task, in 32-bit words |
| `KESTREL_IDLE_STACK_WORDS` | `64` | Stack of the idle task, in 32-bit words |
| `KESTREL_PRIORITY_INHERITANCE` | `ON` | Priority inheritance in mutexes |

The values reach the code through `kestrel/kestrel_config.h`; invalid values fail the build.

**The port provides** `SVC_Handler`, `PendSV_Handler` and `SysTick_Handler` — do not define them in the application. The application starts the 1 kHz SysTick before `kestrel_start()` (`systick_init()` in the drivers does this).

**Interrupt priorities.** An interrupt that calls the kernel must have a priority numerically ≥ `PORT_MAX_SYSCALL_PRIO` (`0x50`), i.e. `NVIC_SetPriority(irq, 5)` or larger. More urgent interrupts are never masked by the kernel and must not call it. See [docs/API.md](docs/API.md#interrupts).

```c
#include <stddef.h>
#include "stm32f3xx.h"
#include "clock.h"
#include "systick.h"
#include "kestrel/task.h"
#include "kestrel/time.h"

static void worker(void *arg)
{
    (void)arg;
    uint32_t last = kestrel_get_ticks();
    while (1) {
        kestrel_delay_until(&last, 10U); /* runs every 10 ms */
    }
}

int main(void)
{
    clock_setup_72MHz();
    systick_init(SystemCoreClock);
    kestrel_new_task(worker, NULL, 1U);
    kestrel_start(); /* never returns */
}
```

More in [`examples/`](examples/): blinky, mutex, queue and interrupt-to-task. Full API: [docs/API.md](docs/API.md).

---

## Roadmap

| | Milestone | Status |
|---|---|---|
| M0 | Bare-metal foundation: startup, linker script, clock, UART | ✅ |
| M1 | First context switch, cooperative `yield` | ✅ |
| M2 | Preemption, `BASEPRI` critical sections | ✅ |
| M3 | Priorities, `delay` / `delay_until`, idle task | ✅ |
| M4 | Semaphores, mutexes with priority inheritance, queues | ✅ |
| M5 | Lazy FPU context, ISR-safe API | ✅ |
| M6 | Benchmarks, jitter under load, FreeRTOS comparison · **v1.0** | ✅ |

Possible extensions: audio demo, MPU isolation, HardFault reports, UART shell.

---

## Known limitations

- Priority inheritance is single-level and assumes one mutex held at a time.
- If a waiter times out, the mutex owner keeps the raised priority until it unlocks.
- A task that returns while holding a mutex does not release it.
- Interrupts above `PORT_MAX_SYSCALL_PRIO` must not call the kernel (documented, not checked).
- No long-duration stress test yet.

---

## Building

Requires `arm-none-eabi-gcc` (newlib), CMake ≥ 3.20, OpenOCD. CMSIS headers are fetched at configure time.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --build build --target flash_example_blinky
picocom -b 115200 /dev/ttyACM0
```

| Option | Default | Effect |
|---|---|---|
| `KESTREL_WARNINGS_AS_ERRORS` | `OFF` | Build Kestrel's code, examples and benchmarks with `-Werror` (CI uses `ON`) |
| `KESTREL_BUILD_EXAMPLES` | `ON` when top-level | Build the firmware in `examples/` |
| `KESTREL_BUILD_BENCHMARKS` | `ON` when top-level | Build the firmware in `bench/` |
| `KESTREL_OPENOCD_ARGS` | `-f board/st_nucleo_f3.cfg` | OpenOCD arguments of the `flash_<name>` targets |

Kernel configuration options are listed under [Using Kestrel in your project](#using-kestrel-in-your-project).

**Host tests**

```bash
cmake -S test -B build-test && cmake --build build-test -j
ctest --test-dir build-test --output-on-failure
```

**Debugging:** `openocd -f board/st_nucleo_f3.cfg`, then `arm-none-eabi-gdb build/examples/example_blinky.elf` → `target extended-remote :3333`.

---

## Reproducing the results

Every number in [Results](#results) comes from the firmware in [`bench/`](bench/). [bench/README.md](bench/README.md) explains what each benchmark measures, how to build, flash and read it, the expected output, and the measurement caveats.

---

## References

- ARMv7-M Architecture Reference Manual; PM0214 (Cortex-M4 programming manual); RM0316 (STM32F303)
- ARM AN298 — Cortex-M4(F) lazy stacking and context switching
- J. Yiu — *The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors*
- J. Labrosse — *µC/OS-III: The Real-Time Kernel*

## License

MIT
