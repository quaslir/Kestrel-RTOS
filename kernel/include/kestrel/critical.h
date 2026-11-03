#ifndef KESTREL_CRITICAL_H
#define KESTREL_CRITICAL_H

#include <stdint.h>

uint32_t kestrel_enter_critical(void);
void kestrel_exit_critical(uint32_t prev);

#define KESTREL_CONCAT_(a, b) a##b
#define KESTREL_CONCAT(a, b)  KESTREL_CONCAT_(a, b)

#define KESTREL_CRITICAL_IMPL(key, once)                           \
    for (uint32_t key = kestrel_enter_critical(), once = 1U; once; \
         kestrel_exit_critical(key), once = 0U)

#define KESTREL_CRITICAL                                                  \
    KESTREL_CRITICAL_IMPL(KESTREL_CONCAT(kestrel_crit_key_, __COUNTER__), \
                          KESTREL_CONCAT(kestrel_crit_once_, __COUNTER__))

#endif
