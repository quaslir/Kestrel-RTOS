# Kestrel RTOS

![CI](https://github.com/<логин>/Kestrel-RTOS/actions/workflows/ci.yml/badge.svg)

**A preemptive real-time kernel for ARM Cortex-M4F, written from scratch in C.**
Runs on the STM32F303RE (NUCLEO-F303RE). No HAL, no vendor drivers — only CMSIS register headers, a custom startup file and linker script.

> **Status:** 🚧 In development — M0–M3 done, next milestone: **M4 (synchronisation)**

---

## Why

Most firmware engineers use an RTOS; few know what happens inside one.
Kestrel exists to answer one question with code, tests and measurements:

> **Can a high-priority task meet a 1 ms hard deadline while lower-priority tasks keep the CPU 90% busy — and can I prove it?**

Everything in this repository — the kernel, the test suite, the benchmarks and the demo — is built to answer that question.

**First evidence (M3):** with two background tasks keeping the CPU at **100%**, a priority-3 task woken every 1 ms by `delay_until` ran **1000 times per second with 0 missed periods** (tick resolution). See [Development log](#development-log).

---

## Features

### v1.0 — core kernel
- [x] Static task creation (no heap), compile-time `MAX_TASKS`
- [x] 32 priority levels, preemptive scheduling, round-robin within a priority
- [x] O(1) task selection (per-priority ready rings + ready bitmap + `CLZ`)
- [ ] Context switching via PendSV, including lazy FPU context (S16–S31) — *integer context done, FPU in M5*
- [x] 1 kHz system tick, `delay` and drift-free periodic `delay_until` (reports missed periods)
- [x] Tick-counter wraparound handled correctly
- [x] Task exit: a task that returns is removed from scheduling
- [ ] Binary and counting semaphores with timeouts
- [ ] Mutexes with priority inheritance
- [ ] Message queues with blocking send/receive and timeouts
- [ ] ISR-safe API (wake a task from an interrupt, switch on ISR exit)
- [x] Critical sections via `BASEPRI` (zero-latency interrupts above the threshold), nestable
- [x] Idle task with `WFI`

### v1.1 — robustness & observability
- [ ] Stack painting, high-water marks and overflow detection
- [ ] HardFault handler with decoded fault report (CFSR, HFSR, MMFAR, BFAR, stacked PC)
- [ ] MPU task isolation with unprivileged tasks and `SVC` system calls
- [ ] Faulting task is terminated; the rest of the system keeps running
- [ ] Per-task CPU usage and a UART shell (`ps`, `top`, `stack`)

### Explicitly out of scope
To keep the project finite, the following will **not** be implemented:
dynamic memory allocation, a file system, networking, multi-core (SMP) support, tickless idle, ports to other architectures, a POSIX layer.

---

## Hardware & tools

| | |
|---|---|
| Board | NUCLEO-F303RE (STM32F303RE, Cortex-M4F @ 72 MHz, 512 KB flash, 64 KB SRAM + 16 KB CCM) |
| Toolchain | `arm-none-eabi-gcc`, CMake |
| Debug / flash | OpenOCD + GDB over the on-board ST-LINK |
| Measurement | DWT cycle counter, oscilloscope on GPIO trace pins |
| Host tests | Unity (C) on the PC, run in CI on every push |

---

## Architecture

```
┌───────────────────────────────────────────────┐
│ app/      demo application, test firmware     │
├───────────────────────────────────────────────┤
│ kernel/   scheduler, tasks, time,             │  portable C
│           ready rings, (sync primitives)      │
├───────────────────────────────────────────────┤
│ port/     PendSV, SVC, SysTick, CLZ,          │  Cortex-M4F specific
│           critical sections (C + assembly)    │
├───────────────────────────────────────────────┤
│ drivers/  clock, GPIO, UART, SysTick setup    │  register level
├───────────────────────────────────────────────┤
│ startup/  vector table, reset handler,        │
│           linker script                       │
└───────────────────────────────────────────────┘
```

### Context switch

```
SysTick (1 kHz)                     task calls delay / delay_until / yield
   │ ticks++                                   │
   │ wake sleeping tasks → ready rings         │ remove from ready ring
   │ set PendSV pending                        │ set PendSV pending
   └──────────────┬────────────────────────────┘
                  ▼
PendSV (lowest priority, runs after all other ISRs)
   1. push R4–R11 onto the current task's PSP, save SP in its TCB
   2. kestrel_schedule(): prio = 31 − CLZ(ready_mask), take head of ready_head[prio], advance head
   3. load SP of the new task, pop R4–R11, write PSP
   4. exception return → hardware restores R0–R3, R12, LR, PC, xPSR
```

### Repository layout
```
Kestrel-RTOS/
├── kernel/
│   ├── include/kestrel/   public API (task.h, time.h, critical.h)
│   └── src/               kernel sources + private headers (kestrel_internal.h, ready.h)
├── port/cm4f/             architecture-specific code (PendSV, SVC, SysTick, BASEPRI, CLZ)
├── drivers/               register-level peripheral drivers
├── startup/               startup code and linker script
├── app/demo/              demo / test firmware
├── test/                  host unit tests (Unity) + host stand-in for port.h
├── cmake/                 toolchain file, dependencies (CMSIS, Unity)
└── .github/workflows/     CI: firmware build + host tests
```

The kernel, port and drivers are built as a CMake `OBJECT` library. Public headers are `PUBLIC`, kernel internals are `PRIVATE`, so the application physically cannot include them. `OBJECT` (not `STATIC`) guarantees that interrupt handlers in their own object files are always linked, even though the vector table only references them weakly.

### Key design decisions
| Decision | Reason |
|---|---|
| PendSV at the lowest exception priority | A context switch never interrupts another ISR; several switch requests coalesce into one |
| `BASEPRI` instead of `PRIMASK` for critical sections | Highest-priority interrupts are never delayed by the kernel |
| Critical sections return the previous `BASEPRI` | Correct nesting without a global counter, usable from ISRs |
| Static allocation only | Deterministic, no fragmentation, memory usage known at link time |
| Ready bitmap + `CLZ` + one ring per priority | Task selection time is independent of the number of tasks; each priority keeps its own round-robin position |
| Absolute wake-up ticks, compared as `(int32_t)(now − wake) >= 0` | Survives the 32-bit tick wraparound (~49.7 days at 1 kHz) |
| `kernel/` split into pure logic (`ready.c`) and hardware glue | The scheduler core is unit-tested on a PC |
| Task stacks in CCM RAM, DMA buffers in SRAM *(planned)* | CCM is fast and zero-wait, but not reachable by DMA |

---

## Roadmap

Each milestone has an **exit criterion**. A milestone is done only when its criterion is met and its tests pass. Estimates assume part-time work alongside studies.

### M0 — Bare-metal foundation · ~2 weeks ✅
- [x] Linker script (FLASH, SRAM, CCMRAM; `.data` / `.bss` / stack)
- [x] Startup: vector table, reset handler, `.data` copy, `.bss` zeroing, FPU enable
- [x] Clock: HSE bypass from ST-LINK → PLL → 72 MHz (with flash wait states)
- [x] GPIO, UART2 (virtual COM port), SysTick at 1 kHz
- [x] CMake build, `flash` and `debug` targets

**Exit:** LED blinks from SysTick, text appears on the virtual COM port, section sizes match expectations.

### M1 — First context switch · ~2 weeks ✅
- [x] Task control block and per-task stacks
- [x] Initial (fake) exception frame for new tasks
- [x] Start the first task on PSP via `SVC`
- [x] Cooperative `yield` through PendSV

**Exit:** two tasks alternate via `yield`; visible as two GPIO traces on the scope.

### M2 — Preemption · ~1 week ✅
- [x] SysTick requests PendSV; round-robin between equal-priority tasks
- [x] Critical sections with `BASEPRI`

**Exit:** three tasks with infinite loops (no `yield`) each get 1 ms slices.

### M3 — Priorities and time · ~2 weeks ✅
- [x] Ready lists per priority, ready bitmap, `CLZ` selection
- [x] Task states, `delay`, `delay_until`, tick wraparound
- [x] Idle task with `WFI`
- [x] Host unit tests for ready rings and selection (17 tests, Unity, CI)

**Exit:** a periodic high-priority task wakes on time within ±1 tick while lower tasks are busy. — **met:** 1000 wake-ups/s, 0 missed, background at 100% CPU.

### M4 — Synchronisation · ~3 weeks
- [ ] Semaphores, mutexes with priority inheritance, queues — all with timeouts
- [ ] Wait lists ordered by priority
- [ ] Priority-inversion demo (with and without inheritance)

**Exit:** all synchronisation tests pass; the inversion demo shows bounded blocking time on the scope.

### M5 — FPU and interrupts · ~2 weeks
- [ ] Lazy FPU stacking (EXC_RETURN bit 4, S16–S31)
- [ ] ISR-safe API and context switch on ISR exit
- [ ] UART RX driver with a queue, ADC + DAC + DMA double-buffer driver

**Exit:** FPU integrity test passes after 10⁶ switches; an interrupt wakes a task without waiting for the next tick.

### M6 — Verification and benchmarks · ~2 weeks
- [ ] Full on-target test suite with a one-command HIL runner
- [ ] 24-hour stress test (random tasks, delays, interrupts, checksums)
- [ ] Measurements: context switch (with/without FPU), interrupt-to-task latency, jitter, footprint
- [ ] Same benchmark application ported to FreeRTOS for comparison

**Exit:** results table below is filled in; stress test runs 24 h with zero faults and zero missed deadlines.

### M7 — Demo and release v1.0 · ~1 week
- [ ] Audio demo: ADC → DMA → processing task (1 ms deadline) → DMA → DAC, with background tasks loading the CPU to ~90%
- [ ] Demo video, scope captures, architecture diagrams
- [ ] Documentation pass, known limitations
- [ ] Tag **v1.0**

**Exit:** the question in [Why](#why) is answered with measured evidence.

### M8 — Robustness, release v1.1 · ~3 weeks *(optional)*
- [ ] Stack monitoring, decoded HardFault reports
- [ ] MPU isolation, unprivileged tasks, `SVC` system calls, fault containment
- [ ] UART shell with per-task CPU and stack usage
- [ ] Tag **v1.1**

**Exit:** a task that writes outside its memory is terminated and reported; all other tasks keep their deadlines.

### 🏁 End of project
After **v1.1** (or v1.0 if M8 is skipped) the repository is **feature-frozen**. Only bug fixes and documentation updates are accepted. New ideas go into a separate project.

**Total estimate:** ~15 weeks for v1.0, ~18 weeks including v1.1.

---

## Development log

Short notes on what each milestone revealed. Raw output from the virtual COM port (115200 baud).

### M2 — preemption breaks shared resources
Two tasks print 22-character lines with no `yield`. At 115200 baud one character takes ~87 µs, so a 1 ms time slice fits ~11 characters — every line is cut in half:

```
AAABBBBBBBBB
BAAAAAAAAAAABBBBBBBBBBBBAAAAAA
```

Wrapping the print in a `BASEPRI` critical section makes the lines whole, but each line keeps the tick masked for ~1.9 ms. SysTick has a single pending bit, so the second tick in that window is **lost** and kernel time runs slow. Lesson: critical sections must last microseconds; long resources need a mutex (M4) or a buffered driver (M5).

### M3 — `delay` drifts, `delay_until` does not
Both tasks do ~3 ticks of work plus a 1000-tick sleep:

```
delay       t=4023  dt=1005
delay_until t=4504  dt=1000
delay       t=5028  dt=1005
delay_until t=5504  dt=1000
```

`delay` counts from the moment it is called, so work time (including the print itself) accumulates: +5 ticks per period, +300 ticks per minute. `delay_until` counts from the previous wake-up and stays on its grid.

### M3 — a test found starvation
Five tasks: two background loops at priority 1, LED and reporter at 2, a 1 ms periodic task at 3. With the first priority scheduler (linear scan, round-robin relative to the current task):

```
t=4003 crit/s=1000 late_max=0 bg0/s=7836845 bg1/s=0
```

After every 1 ms preemption the search restarted after the high-priority task, so it always found `bg0` first — `bg1` never ran. Fix: one ready ring per priority, each with its own cursor, selected with `31 − CLZ(ready_mask)`:

```
t=12003 crit/s=1000 missed=0 bg0/s=3929234 bg1/s=3935996
```

The scenario is kept as a regression test (`test_starvation_regression`).

---

## Testing

| Level | What | Where |
|---|---|---|
| Unit | Ready rings, bitmap, selection *(done)*; queue buffer, tick arithmetic, inheritance logic *(planned)* | Host (CI on every push) |
| Functional | Priority order, preemption, delay accuracy, timeouts, queues, inheritance, ISR wake-up, FPU and register integrity | Target, HIL runner |
| Measurement | Cycles via DWT, timing via GPIO + oscilloscope | Target |
| Stress | 24 h randomised load with checksums and deadline monitoring | Target |
| Comparison | Identical application on FreeRTOS | Target |

---

## Results

*To be filled in at M6.*

| Metric | Kestrel | FreeRTOS |
|---|---|---|
| Context switch, no FPU (cycles) | — | — |
| Context switch, with FPU (cycles) | — | — |
| Interrupt → task latency (µs) | — | — |
| Periodic task jitter under load (µs) | — | — |
| Kernel flash footprint (bytes) | — | — |
| Kernel RAM footprint (bytes) | — | — |

---

## Building

### Firmware
Requires `arm-none-eabi-gcc` (with newlib), CMake ≥ 3.20, OpenOCD.

```bash
mkdir -p build && cd build
cmake .. -G "Unix Makefiles"
make -j
make flash_kestrel_demo
```

Serial output: `picocom -b 115200 /dev/ttyACM0`

### Debugging
```bash
openocd -f board/st_nucleo_f3.cfg            # terminal 1
arm-none-eabi-gdb build/app/demo/kestrel_demo.elf   # terminal 2
(gdb) target extended-remote :3333
(gdb) monitor reset halt
(gdb) load
```

### Host unit tests
Requires a native `gcc` and CMake. Unity is fetched automatically.

```bash
cmake -S test -B build-test -G "Unix Makefiles"
cmake --build build-test -j
ctest --test-dir build-test --output-on-failure
```

Tests are built with AddressSanitizer and UndefinedBehaviorSanitizer.

---

## References

- RM0316 — STM32F303 reference manual
- PM0214 — STM32 Cortex-M4 programming manual
- ARMv7-M Architecture Reference Manual
- ARM AN298 — Cortex-M4(F) lazy stacking and context switching
- UM1724 — STM32 Nucleo-64 boards user manual
- J. Yiu — *The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors*
- J. Labrosse — *µC/OS-III: The Real-Time Kernel*
- G. Buttazzo — *Hard Real-Time Computing Systems*
- C. L. Liu, J. W. Layland — *Scheduling Algorithms for Multiprogramming in a Hard-Real-Time Environment* (1973)

---

## License

MIT
