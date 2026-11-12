#include "ou_flash_policy.h"

int ou_flash_layout_validate(const ou_flash_layout *layout) {
    unsigned i, j;
    uint32_t block;
    if (!layout || layout->physical_geometry_verified != 1U ||
        layout->boot_recovery_verified != 1U || layout->read_only != 1U)
        return -1;
    block = layout->erase_block_bytes;
    if (block < 4096U || block > 262144U || (block & (block - 1U)) != 0U ||
        layout->device_bytes < 524288U || (layout->device_bytes & (block - 1U)) != 0U)
        return -1;
    for (i = 0; i < OU_FLASH_REGION_COUNT; ++i) {
        const ou_flash_region *r = &layout->regions[i];
        /* A zero-length protected bootloader/recovery region is never safe.
         * Capacity is a hard upper bound; avoid unsigned overflow. */
        if (r->length < block || (r->offset & (block - 1U)) != 0U ||
            (r->length & (block - 1U)) != 0U || r->offset >= layout->device_bytes ||
            r->length > layout->device_bytes - r->offset)
            return -1;
        for (j = 0; j < i; ++j) {
            const ou_flash_region *other = &layout->regions[j];
            if (r->offset < other->offset + other->length &&
                other->offset < r->offset + r->length)
                return -1;
        }
    }
    return 0;
}
