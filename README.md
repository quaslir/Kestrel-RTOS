# Kestrel RTOS

**A preemptive real-time kernel for ARM Cortex-M4F, written from scratch in C.**
Runs on the STM32F303RE (NUCLEO-F303RE). No HAL, no vendor drivers — only CMSIS register headers, a custom startup file and linker script.

> **Status:** 🚧 In development — current milestone: **M0 (bare-metal foundation)**

---

## Why

Most firmware engineers use an RTOS; few know what happens inside one.
Kestrel exists to answer one question with code, tests and measurements:

> **Can a high-priority task meet a 1 ms hard deadline while lower-priority tasks keep the CPU 90% busy — and can I prove it?**

Everything in this repository — the kernel, the test suite, the benchmarks and the demo — is built to answer that question.

---

## Features

### v1.0 — core kernel
- [ ] Static task creation (no heap), up to 16 tasks
- [ ] 16 priority levels, preemptive scheduling, round-robin within a priority
- [ ] O(1) task selection (ready bitmap + `CLZ`)
- [ ] Context switching via PendSV, including lazy FPU context (S16–S31)
- [ ] 1 kHz system tick, `delay` and drift-free periodic `delay_until`
- [ ] Tick-counter wraparound handled correctly
- [ ] Binary and counting semaphores with timeouts
- [ ] Mutexes with priority inheritance
- [ ] Message queues with blocking send/receive and timeouts
- [ ] ISR-safe API (wake a task from an interrupt, switch on ISR exit)
- [ ] Critical sections via `BASEPRI` (zero-latency interrupts above the threshold)
- [ ] Idle task with `WFI`

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
| Host tests | Unity (C), Python + pyserial for hardware-in-the-loop runs |

---

## Architecture

```
┌───────────────────────────────────────────────┐
│ app/      demo application, test firmware     │
├───────────────────────────────────────────────┤
│ kernel/   scheduler, tasks, time,             │  portable C
│           semaphores, mutexes, queues         │
├───────────────────────────────────────────────┤
│ port/     PendSV, SVC, SysTick, FPU, MPU,     │  Cortex-M4F specific
│           critical sections (C + assembly)    │
├───────────────────────────────────────────────┤
│ drivers/  clock, GPIO, UART, timers, ADC,     │  register level
│           DAC, DMA                            │
├───────────────────────────────────────────────┤
│ startup/  vector table, reset handler,        │
│           linker script                       │
└───────────────────────────────────────────────┘
```

### Repository layout
```
kestrel/
├── kernel/        portable kernel sources and public API
├── port/cm4f/     architecture-specific code
├── drivers/       register-level peripheral drivers
├── startup/       startup code and linker script
├── app/
│   ├── demo/      audio demo (final deliverable)
│   └── tests/     on-target test firmware
├── tests/host/    unit tests that run on a PC
├── tools/         HIL test runner, benchmark scripts
└── docs/          design notes, diagrams, scope captures, results
```

### Key design decisions
| Decision | Reason |
|---|---|
| PendSV at the lowest exception priority | A context switch never interrupts another ISR; several switch requests coalesce into one |
| `BASEPRI` instead of `PRIMASK` for critical sections | Highest-priority interrupts are never delayed by the kernel |
| Static allocation only | Deterministic, no fragmentation, memory usage known at link time |
| Ready bitmap + `CLZ` | Task selection time is independent of the number of tasks |
| Task stacks in CCM RAM, DMA buffers in SRAM | CCM is fast and zero-wait, but not reachable by DMA |
| Separate `kernel/` and `port/` | Shows which parts are portable and which are tied to the architecture |

*(Each decision gets a short write-up in `docs/` once implemented.)*

---

## Roadmap

Each milestone has an **exit criterion**. A milestone is done only when its criterion is met and its tests pass. Estimates assume part-time work alongside studies.

### M0 — Bare-metal foundation · ~2 weeks
- [x] Linker script (FLASH, SRAM, CCMRAM; `.data` / `.bss` / stack)
- [x] Startup: vector table, reset handler, `.data` copy, `.bss` zeroing, FPU enable
- [x] Clock: HSE bypass from ST-LINK → PLL → 72 MHz (with flash wait states)
- [x] GPIO, UART2 (virtual COM port), SysTick at 1 kHz
- [x] CMake build, `flash` and `debug` targets

**Exit:** LED blinks from SysTick, text appears on the virtual COM port, section sizes match expectations.

### M1 — First context switch · ~2 weeks
- [x] Task control block and per-task stacks
- [x] Initial (fake) exception frame for new tasks
- [x] Start the first task on PSP via `SVC`
- [x] Cooperative `yield` through PendSV

**Exit:** two tasks alternate via `yield`; visible as two GPIO traces on the scope.

### M2 — Preemption · ~1 week
- [ ] SysTick requests PendSV; round-robin between equal-priority tasks
- [ ] Critical sections with `BASEPRI`

**Exit:** three tasks with infinite loops (no `yield`) each get 1 ms slices.

### M3 — Priorities and time · ~2 weeks
- [ ] Ready lists per priority, ready bitmap, `CLZ` selection
- [ ] Task states, `delay`, `delay_until`, sleep list, tick wraparound
- [ ] Idle task with `WFI`
- [ ] Host unit tests for lists, bitmap and time comparison

**Exit:** a periodic high-priority task wakes on time within ±1 tick while lower tasks are busy.

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

## Testing

| Level | What | Where |
|---|---|---|
| Unit | Lists, bitmap, queue buffer, tick arithmetic, inheritance logic | Host (CI on every commit) |
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

*To be written at M0.*

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
