#include "ou_ns7520_mem.h"

/* Decode for a contiguous address mask, ones at the high end of 32 bits.
 * The NS7520 may also intentionally alias disjoint memory ranges. A 32-bit
 * mask of all zeros is not a meaningful 4 GiB installed chip measurement. */
static uint32_t contiguous_window(uint32_t mask) {
    uint32_t inverse = ~mask;
    uint32_t span;
    if (mask == 0U || (inverse & (inverse + 1U)) != 0U) return 0U;
    if (inverse == UINT32_MAX) return 0U;
    span = inverse + 1U;
    if (span < 4096U) return 0U;
    return span;
}

int ou_mem_decode(uint32_t mmcr, uint32_t registers[OU_MEM_CS_COUNT][3],
                  ou_mem_map *output) {
    unsigned i, j;
    if (!registers || !output) return -1;
    *output = (ou_mem_map){0};
    output->mmcr = mmcr;
    for (i = 0; i < OU_MEM_CS_COUNT; ++i) {
        const uint32_t bar = registers[i][0];
        const uint32_t ora = registers[i][1];
        const uint32_t mask = ora & UINT32_C(0xFFFFF000);
        ou_mem_chip_select *c = &output->cs[i];
        const uint32_t width = (ora >> 2U) & 3U;
        c->bar = bar;
        c->ora = ora;
        c->orb = registers[i][2];
        c->decode_mask = mask;
        c->decode_base = bar & mask;
        c->enabled = (uint8_t)(bar & 1U);
        c->write_protected = (uint8_t)((bar >> 1U) & 1U);
        c->dram = (uint8_t)((bar >> 3U) & 1U);
        c->bus_width_bits = (uint8_t)(width == 0U ? 32U : width == 1U ? 16U : width == 2U ? 8U : 0U);
        c->window_bytes = contiguous_window(mask);
        c->contiguous = (uint8_t)(c->window_bytes != 0U);
    }
    /* Conservative intersection test: if the two masked comparisons can be
     * satisfied by the same 32-bit address then their decode regions overlap. */
    for (i = 0; i < OU_MEM_CS_COUNT; ++i) {
        if (!output->cs[i].enabled) continue;
        for (j = i + 1U; j < OU_MEM_CS_COUNT; ++j) {
            const ou_mem_chip_select *a = &output->cs[i];
            const ou_mem_chip_select *b = &output->cs[j];
            if (!b->enabled) continue;
            if (((a->decode_base ^ b->decode_base) & a->decode_mask & b->decode_mask) == 0U) {
                output->overlapping_mask |= (uint8_t)((1U << i) | (1U << j));
            }
        }
    }
    output->decoded = 1U;
    /* Decoding a controller window is NEVER proof of flash/RAM chip capacity. */
    output->capacity_verified = 0U;
    return 0;
}

int ou_mem_inspect(ou_mem_read32 read32, void *ctx, ou_mem_map *output) {
    uint32_t mmcr = 0U;
    uint32_t registers[OU_MEM_CS_COUNT][3] = {{0}};
    unsigned i, j;
    if (!output) return -1;
    *output = (ou_mem_map){0};
    if (!read32 || read32(ctx, OU_MEM_CONTROLLER_BASE, &mmcr) != 0)
        return -1;
    for (i = 0; i < OU_MEM_CS_COUNT; ++i) {
        for (j = 0; j < 3U; ++j) {
            const uint32_t address = OU_MEM_CONTROLLER_BASE + 0x10U + (i * 0x10U) + (j * 4U);
            if (read32(ctx, address, &registers[i][j]) != 0)
                return -1;
        }
    }
    return ou_mem_decode(mmcr, registers, output);
}
