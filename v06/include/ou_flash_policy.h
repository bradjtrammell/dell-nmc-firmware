#ifndef OU_FLASH_POLICY_H
#define OU_FLASH_POLICY_H
#include <stdint.h>

/* Neither the H910P part size nor erase geometry is verified. The layout
 * validator must never infer them from a NET+OS update image length. */
#define OU_FLASH_REGION_COUNT 5U

enum ou_flash_region_id {
    OU_FLASH_BOOTLOADER = 0,
    OU_FLASH_RECOVERY = 1,
    OU_FLASH_APPLICATION = 2,
    OU_FLASH_CONFIGURATION = 3,
    OU_FLASH_LOGS = 4
};

typedef struct {
    uint32_t offset;
    uint32_t length;
} ou_flash_region;

typedef struct {
    uint32_t device_bytes;
    uint32_t erase_block_bytes;
    ou_flash_region regions[OU_FLASH_REGION_COUNT];
    uint8_t physical_geometry_verified;
    uint8_t boot_recovery_verified;
    uint8_t read_only;
    uint8_t reserved;
} ou_flash_layout;

/* Policy validation only: NO FLASH I/O, NO UPDATER, NO ERASE.
 * Returns zero only for physical geometry + independently tested recovery
 * and disjoint, erase-aligned mandatory regions. */
int ou_flash_layout_validate(const ou_flash_layout *layout);

#endif
