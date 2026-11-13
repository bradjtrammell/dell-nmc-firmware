#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

uint32_t __aeabi_uidiv(uint32_t, uint32_t);
void __aeabi_memcpy(void *, const void *, size_t);
void __aeabi_memcpy4(void *, const void *, size_t);
void __aeabi_memcpy8(void *, const void *, size_t);
void __aeabi_memclr(void *, size_t);
void __aeabi_memclr4(void *, size_t);
void __aeabi_memclr8(void *, size_t);

typedef void (*copy_fn)(void *, const void *, size_t);
typedef void (*clear_fn)(void *, size_t);
static const copy_fn copies[] = {__aeabi_memcpy, __aeabi_memcpy4, __aeabi_memcpy8};
static const clear_fn clears[] = {__aeabi_memclr, __aeabi_memclr4, __aeabi_memclr8};

static void test_copy(void) {
    uint8_t src[260], out[260];
    const size_t lengths[] = {0U, 1U, 2U, 3U, 4U, 7U, 8U, 16U, 31U, 64U, 255U};
    size_t i, j, k;
    for (i = 0U; i < 260U; ++i) src[i] = (uint8_t)(i ^ 0x5aU);
    for (i = 0U; i < sizeof(copies)/sizeof(copies[0]); ++i) {
        for (j = 0U; j < sizeof(lengths)/sizeof(lengths[0]); ++j) {
            for (k = 0U; k < 260U; ++k) out[k] = 0xa5U;
            copies[i](out + 2U, src + 2U, lengths[j]);
            assert(out[0] == 0xa5U && out[1] == 0xa5U);
            for (k = 0U; k < lengths[j]; ++k) assert(out[k + 2U] == src[k + 2U]);
            for (k = lengths[j] + 2U; k < 260U; ++k) assert(out[k] == 0xa5U);
        }
    }
}
static void test_clear(void) {
    uint8_t out[260];
    const size_t lengths[] = {0U, 1U, 2U, 3U, 4U, 7U, 8U, 16U, 31U, 64U, 255U};
    size_t i, j, k;
    for (i = 0U; i < sizeof(clears)/sizeof(clears[0]); ++i) {
        for (j = 0U; j < sizeof(lengths)/sizeof(lengths[0]); ++j) {
            for (k = 0U; k < 260U; ++k) out[k] = 0xa5U;
            clears[i](out + 2U, lengths[j]);
            assert(out[0] == 0xa5U && out[1] == 0xa5U);
            for (k = 0U; k < lengths[j]; ++k) assert(out[k + 2U] == 0U);
            for (k = lengths[j] + 2U; k < 260U; ++k) assert(out[k] == 0xa5U);
        }
    }
}
static void test_division(void) {
    static const uint32_t cases[] = {
        0U,1U,2U,3U,7U,15U,16U,17U,99U,255U,
        UINT32_C(0x7fffffff),UINT32_C(0x80000000),UINT32_MAX
    };
    uint32_t n=UINT32_C(0x91a7b8c3);
    unsigned i,j;
    for(i=0U;i<sizeof(cases)/sizeof(cases[0]);++i)
        for(j=1U;j<sizeof(cases)/sizeof(cases[0]);++j)
            assert(__aeabi_uidiv(cases[i],cases[j])==cases[i]/cases[j]);
    for(i=0U;i<10000U;++i) {
        uint32_t divisor;
        n=n*UINT32_C(1664525)+UINT32_C(1013904223);
        divisor=(n>>3U)|1U;
        assert(__aeabi_uidiv(n,divisor)==n/divisor);
    }
    assert(__aeabi_uidiv(UINT32_MAX,0U)==0U);
}
int main(void) {
    test_division();
    test_copy();
    test_clear();
    puts("PASS: EABI memory helpers and 32-bit unsigned software division (10k+ vectors)");
    return 0;
}
