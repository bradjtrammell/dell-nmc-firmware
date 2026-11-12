#include <stddef.h>
#include <stdint.h>

/* Freestanding ARM7 EABI memory helpers. There is no libc/compiler-rt in the
 * target image. Avoid compiler-generated libc calls inside these functions.
 * memcpy variants require nonoverlapping source and destination ranges. */
#if defined(__clang__)
#define OU_EABI_NO_OPT __attribute__((optnone))
#else
#define OU_EABI_NO_OPT
#endif

OU_EABI_NO_OPT
void __aeabi_memclr4(void *dest, size_t bytes) {
    volatile uint8_t *out = (volatile uint8_t *)dest;
    while (bytes--) *out++ = 0U;
}

OU_EABI_NO_OPT
void __aeabi_memcpy4(void *dest, const void *src, size_t bytes) {
    volatile uint8_t *out = (volatile uint8_t *)dest;
    const volatile uint8_t *in = (const volatile uint8_t *)src;
    while (bytes--) *out++ = *in++;
}

OU_EABI_NO_OPT
void __aeabi_memcpy(void *dest, const void *src, size_t bytes) {
    __aeabi_memcpy4(dest, src, bytes);
}

OU_EABI_NO_OPT
void __aeabi_memcpy8(void *dest, const void *src, size_t bytes) {
    __aeabi_memcpy4(dest, src, bytes);
}

OU_EABI_NO_OPT
void __aeabi_memclr(void *dest, size_t bytes) {
    __aeabi_memclr4(dest, bytes);
}

OU_EABI_NO_OPT
void __aeabi_memclr8(void *dest, size_t bytes) {
    __aeabi_memclr4(dest, bytes);
}
