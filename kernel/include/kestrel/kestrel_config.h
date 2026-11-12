#ifndef KESTREL_CONFIG_H
#define KESTREL_CONFIG_H

/*
 * Kestrel build-time configuration.
 *
 * Every option has a default and can be overridden without editing this file,
 * e.g. from CMake:  cmake .. -DKESTREL_MAX_TASKS=12 -DKESTREL_STACK_WORDS=512
 * The same values must be seen by the kernel and the application, so pass them
 * as compile definitions for the whole build, not in a single source file.
 */

/* Maximum number of application tasks (the idle task is not counted). */
#ifndef KESTREL_MAX_TASKS
#define KESTREL_MAX_TASKS 8
#endif

/* Stack size of every application task, in 32-bit words. */
#ifndef KESTREL_STACK_WORDS
#define KESTREL_STACK_WORDS 256
#endif

/* Stack size of the idle task, in 32-bit words. */
#ifndef KESTREL_IDLE_STACK_WORDS
#define KESTREL_IDLE_STACK_WORDS 64
#endif

/* 1: mutexes use priority inheritance, 0: plain mutexes. */
#ifndef KESTREL_CFG_PRIORITY_INHERITANCE
#define KESTREL_CFG_PRIORITY_INHERITANCE 1
#endif

/* Worst-case context-switch frame: 26 words hardware (with FPU) + 16 words S16-S31
 * + 9 words R4-R11 and EXC_RETURN = 51 words, plus room for function calls. */
#define KESTREL_MIN_STACK_WORDS 64

/* Idle never uses the FPU: 8 words hardware frame + 9 words saved by PendSV,
 * plus room for interrupts that stack onto it. */
#define KESTREL_MIN_IDLE_STACK_WORDS 32

_Static_assert(KESTREL_MAX_TASKS >= 1 && KESTREL_MAX_TASKS <= 254,
               "KESTREL_MAX_TASKS must be 1..254 (task id is uint8_t, 0xFF is reserved)");
_Static_assert(KESTREL_STACK_WORDS >= KESTREL_MIN_STACK_WORDS,
               "KESTREL_STACK_WORDS is too small for a context-switch frame with FPU state");
_Static_assert(KESTREL_IDLE_STACK_WORDS >= KESTREL_MIN_IDLE_STACK_WORDS,
               "KESTREL_IDLE_STACK_WORDS is too small");
_Static_assert(KESTREL_CFG_PRIORITY_INHERITANCE == 0 || KESTREL_CFG_PRIORITY_INHERITANCE == 1,
               "KESTREL_CFG_PRIORITY_INHERITANCE must be 0 or 1");

#endif
