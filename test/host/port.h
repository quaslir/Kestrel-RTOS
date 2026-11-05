#ifndef KESTREL_PORT_H
#define KESTREL_PORT_H

#include <stdint.h>

static inline uint8_t port_highest_priority(uint32_t mask)
{
    return (uint8_t)(31U - (uint32_t)__builtin_clz(mask));
}

#endif
