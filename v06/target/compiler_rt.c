#include <stddef.h>
#include <stdint.h>

/* Minimal ARM EABI runtime helper required by Clang's structure-zeroing.
 * No dependence on Dell or a proprietary ARM runtime. */
__attribute__((optnone))
void __aeabi_memclr4(void *dest, size_t bytes) {
    volatile uint8_t *out = (volatile uint8_t *)dest;
    while (bytes--) *out++ = 0U;
}
