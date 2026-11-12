#ifndef OU_NS7520_MEM_H
#define OU_NS7520_MEM_H

#include <stddef.h>
#include <stdint.h>

/* Digi NS7520 Hardware Reference Rev G, Table 35, pp. 87-88.
 * This module is PURE: no register pointers, flash writes, or MMIO reads.
 * A future *verified* board adapter supplies read32; do not wire this
 * to the installed UPS without proving address mapping/recovery first. */
#define OU_MEM_CONTROLLER_BASE UINT32_C(0xFFC00000)
#define OU_MEM_CS_COUNT 5U
#define OU_MEM_READS 16U

typedef int (*ou_mem_read32)(void *opaque, uint32_t address, uint32_t *value);

typedef struct {
    uint32_t bar;
    uint32_t ora;
    uint32_t orb;
    uint32_t decode_base;
    uint32_t decode_mask;
    uint32_t window_bytes; /* 0 = noncontiguous or unrepresentable */
    uint8_t enabled;
    uint8_t dram;
    uint8_t write_protected;
    uint8_t bus_width_bits; /* 8/16/32, 0 if reserved */
    uint8_t contiguous;
} ou_mem_chip_select;

typedef struct {
    uint32_t mmcr;
    ou_mem_chip_select cs[OU_MEM_CS_COUNT];
    uint8_t overlapping_mask; /* bit i set if CS i overlaps another enabled CS */
    uint8_t decoded;
    uint8_t capacity_verified; /* Always 0: decode window != physical installed chip */
} ou_mem_map;

/* On callback error the output is zeroed; never return partial register state.
 * No MMIO is performed by this module; overlap is conservative for aliases. */
int ou_mem_inspect(ou_mem_read32 read32, void *ctx, ou_mem_map *output);

/* Decode an already captured set of controller registers; all fields little
 * significance are from the processor's logical 32-bit register values,
 * independent of endianness of the capture transport. */
int ou_mem_decode(uint32_t mmcr, uint32_t registers[OU_MEM_CS_COUNT][3],
                  ou_mem_map *output);

#endif
