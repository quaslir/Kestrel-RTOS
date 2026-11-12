# Kestrel RTOS

![CI](https://github.com/quaslir/Kestrel-RTOS/actions/workflows/ci.yml/badge.svg)

A preemptive real-time kernel for ARM Cortex-M4F, written from scratch in C.
Runs on the STM32F303RE (NUCLEO-F303RE) with no HAL and no vendor drivers — only CMSIS register headers, a custom startup file and linker script.

**Goal:** prove with measurements that a high-priority task meets a **1 ms hard deadline** while lower-priority tasks keep the CPU ~90% busy.

> **Status:** M0–M5 complete · M6 (benchmarks) in progress · [API reference](docs/API.md) · [Development log](docs/DEVLOG.md)

---

## Results

STM32F303RE @ 72 MHz, `Release` build, measured with the DWT cycle counter.

| Metric | Result |
|---|---|
| Context switch, task → task (`yield`) | **198 cycles · 2.75 µs** (min 178, max 352) |
| Context switch with FPU state | **259 cycles · 3.6 µs** (constant) |
| Interrupt → waiting task | **416–422 cycles · 5.8 µs**, CPU 100% busy |
| 1 ms periodic task, wake-up latency, idle system | **429–489 cycles · 6.0–6.8 µs**, period jitter ±0.8 µs |
| 1 ms periodic task, CPU 100% busy with queue, mutex and 10 kHz ISR traffic | **520–950 cycles · 7.2–13.2 µs**, period jitter ±5.4 µs, **0 missed deadlines** in 10 min |
| Priority inversion, blocking time of high-priority task | 310 ms → **40 ms** with inheritance |
| Kernel flash (kernel + port) | **2 338 B** (`-Os`) · 2 660 B (`-O2`) |
| Kernel RAM, fixed | **434 B** (incl. 256 B idle stack) |
| RAM per task | **36 B** TCB + stack (1 KB in the benchmarks) |

Context-switch numbers include ~14 cycles of measurement overhead. Queue items are copied inside a critical section, so item size bounds worst-case latency: with 256-byte items the 1 ms task's worst case grows to 76 µs ([details](docs/DEVLOG.md#m62--jitter-under-load-and-what-a-queue-item-costs)). A 24-hour stress test and a FreeRTOS comparison on the same board follow in M6.

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

**Planned (v1.1):** stack overflow detection, decoded HardFault reports, MPU task isolation, UART shell.
**Out of scope:** dynamic memory, file system, networking, SMP, tickless idle, other architectures.

---

## Architecture

```
app/       demo and test firmware
kernel/    scheduler, time, wait lists, semaphore, mutex, queue     portable C
port/cm4f/ PendSV, SVC, SysTick, BASEPRI, CLZ, FPU                  Cortex-M4F
drivers/   clock, GPIO, UART                                        register level
startup/   vector table, reset handler, linker script
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

Interrupts that call the kernel must have priority numerically ≥ `PORT_MAX_SYSCALL_PRIO` (`0x50`). See [docs/API.md](docs/API.md#interrupts).

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
| M6 | Benchmarks, jitter under load, 24 h stress test, FreeRTOS comparison | 🚧 |
| M7 | Audio demo (ADC → DMA → 1 ms processing task → DAC) at ~90% load · **v1.0** | |
| M8 | Stack monitoring, HardFault reports, MPU isolation, shell · **v1.1** *(optional)* | |

---

## Known limitations

- Priority inheritance is single-level and assumes one mutex held at a time.
- If a waiter times out, the mutex owner keeps the raised priority until it unlocks.
- A task that returns while holding a mutex does not release it.
- Interrupts above `PORT_MAX_SYSCALL_PRIO` must not call the kernel (documented, not checked).

---

## Building

Requires `arm-none-eabi-gcc` (newlib), CMake ≥ 3.20, OpenOCD.

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j && make flash_kestrel_demo
picocom -b 115200 /dev/ttyACM0
```

| Option | Default | Effect |
|---|---|---|
| `KESTREL_PRIORITY_INHERITANCE` | `ON` | `OFF` reproduces the priority-inversion demo |

**Host tests**

```bash
cmake -S test -B build-test && cmake --build build-test -j
ctest --test-dir build-test --output-on-failure
```

**Debugging:** `openocd -f board/st_nucleo_f3.cfg`, then `arm-none-eabi-gdb build/app/demo/kestrel_demo.elf` → `target extended-remote :3333`.

---

## References

- ARMv7-M Architecture Reference Manual; PM0214 (Cortex-M4 programming manual); RM0316 (STM32F303)
- ARM AN298 — Cortex-M4(F) lazy stacking and context switching
- J. Yiu — *The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors*
- J. Labrosse — *µC/OS-III: The Real-Time Kernel*

## License

MIT
