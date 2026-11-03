#include <stdint.h>

void *memcpy(void *dest, const void *src, uintptr_t size)
{
    unsigned char *out      = dest;
    const unsigned char *in = src;
    for (uintptr_t i = 0; i < size; ++i) out[i] = in[i];
    return dest;
}

void *memset(void *dest, int value, uintptr_t size)
{
    unsigned char *out = dest;
    for (uintptr_t i = 0; i < size; ++i) out[i] = (unsigned char)value;
    return dest;
}
