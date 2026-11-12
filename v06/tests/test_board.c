#include "ou_ns7520_mem.h"
#include "ou_ups_link.h"
#include "ou_flash_policy.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t mock_registers[1U + OU_MEM_CS_COUNT * 3U];
static unsigned reads;
static unsigned fail_after;
static int mock_read(void *opaque, uint32_t address, uint32_t *out) {
    size_t offset;
    (void)opaque;
    if (!out || address < OU_MEM_CONTROLLER_BASE || (address & 3U)) return -1;
    if (reads++ == fail_after) return -1;
    if (address == OU_MEM_CONTROLLER_BASE) { *out = mock_registers[0]; return 0; }
    offset = (size_t)(address - OU_MEM_CONTROLLER_BASE);
    if (offset < 0x10U || offset >= 0x60U || ((offset & 15U) > 8U)) return -1;
    *out = mock_registers[1U + ((offset - 0x10U) / 16U) * 3U + ((offset & 15U) / 4U)];
    return 0;
}

static void test_memory_decode(void) {
    ou_mem_map out;
    uint32_t r[OU_MEM_CS_COUNT][3] = {{0}};
    /* Configured decode 0x02000000-0x021fffff (2MiB), static 16-bit bus.
     * This is a synthetic controller state, not a claim about card chips. */
    r[0][0] = UINT32_C(0x02000003);  /* enable + write-protect */
    r[0][1] = UINT32_C(0xFFE00004);  /* 2MiB mask + 16-bit bus */
    r[1][0] = UINT32_C(0x08000009);  /* RAM region + enabled + DRAM */
    r[1][1] = UINT32_C(0xFF000000);  /* 16MiB decode aperture */
    assert(ou_mem_decode(1U, r, &out) == 0);
    assert(out.decoded == 1U && out.capacity_verified == 0U);
    assert(out.cs[0].decode_base == UINT32_C(0x02000000));
    assert(out.cs[0].window_bytes == 2097152U);
    assert(out.cs[0].bus_width_bits == 16U && out.cs[0].write_protected);
    assert(out.cs[1].dram && out.cs[1].window_bytes == 16777216U);
    assert(out.overlapping_mask == 0U);
    /* Same decoding window on another enabled CS is a severe conflict. */
    r[2][0] = UINT32_C(0x02000001);
    r[2][1] = UINT32_C(0xFFE00000);
    assert(ou_mem_decode(0, r, &out) == 0);
    assert((out.overlapping_mask & ((1U << 0) | (1U << 2))) != 0U);
    /* Hole in mask can make noncontiguous aliases; never report size. */
    r[2][1] = UINT32_C(0xF0F00000);
    assert(ou_mem_decode(0, r, &out) == 0);
    assert(out.cs[2].contiguous == 0U && out.cs[2].window_bytes == 0U);
}

static void test_register_map(void) {
    ou_mem_map out;
    unsigned i;
    for (i = 0U; i < 16U; ++i) mock_registers[i] = 0U;
    mock_registers[1] = UINT32_C(0x02000001);
    mock_registers[2] = UINT32_C(0xFFE00004);
    reads = 0U;
    fail_after = 100U;
    assert(ou_mem_inspect(mock_read, 0, &out) == 0);
    assert(reads == OU_MEM_READS);
    assert(out.cs[0].window_bytes == 2097152U);
    assert(out.capacity_verified == 0U);
    reads = 0U;
    fail_after = 8U;
    assert(ou_mem_inspect(mock_read, 0, &out) == -1);
    assert(out.decoded == 0U && out.cs[0].bar == 0U);
    assert(ou_mem_inspect(0, 0, &out) == -1);
    assert(ou_mem_inspect(mock_read, 0, 0) == -1);
}

static void test_opaque_rx_only(void) {
    ou_link_observer o;
    ou_link_record entry;
    uint8_t bytes[OU_LINK_MAX_REPORT_BYTES + 1U];
    unsigned i;
    for (i = 0U; i < sizeof(bytes); ++i) bytes[i] = (uint8_t)i;
    ou_link_init(&o);
    assert(ou_link_observe(&o, OU_LINK_CHANNEL_UNKNOWN, 0, 1, 0) == -1);
    assert(ou_link_observe(&o, OU_LINK_CHANNEL_UNKNOWN, bytes, sizeof(bytes), 0) == -1);
    assert(o.rejected_reports == 2 && o.reports_seen == 0);
    for (i = 0U; i < 40U; ++i) {
        assert(ou_link_observe(&o, OU_LINK_CHANNEL_SERIAL_A, bytes, 6, i) == 0);
    }
    assert(o.count == OU_LINK_LOG_CAPACITY && o.reports_seen == 40U);
    assert(o.overwritten_records == 8U && o.bytes_seen == 240U);
    assert(ou_link_recent(&o, 0U, &entry) == 0 && entry.monotonic_ms == 39U);
    assert(entry.size == 6U && entry.channel == OU_LINK_CHANNEL_SERIAL_A);
    assert(ou_link_recent(&o, 31U, &entry) == 0 && entry.monotonic_ms == 8U);
    assert(ou_link_recent(&o, 32U, &entry) == -1);
    /* No payload is retained: records contain metadata only. */
    assert(sizeof(entry) == 12U);
}

static void test_flash_safety_policy(void) {
    ou_flash_layout p = {0};
    p.device_bytes = 4194304U;
    p.erase_block_bytes = 65536U;
    p.physical_geometry_verified = 1U;
    p.boot_recovery_verified = 1U;
    p.read_only = 1U;
    p.regions[OU_FLASH_BOOTLOADER] = (ou_flash_region){0U, 65536U};
    p.regions[OU_FLASH_RECOVERY] = (ou_flash_region){65536U, 65536U};
    p.regions[OU_FLASH_APPLICATION] = (ou_flash_region){131072U, 3145728U};
    p.regions[OU_FLASH_CONFIGURATION] = (ou_flash_region){3276800U, 65536U};
    p.regions[OU_FLASH_LOGS] = (ou_flash_region){3342336U, 131072U};
    assert(ou_flash_layout_validate(&p) == 0);
    p.physical_geometry_verified = 0U;
    assert(ou_flash_layout_validate(&p) == -1);
    p.physical_geometry_verified = 1U;
    p.boot_recovery_verified = 0U;
    assert(ou_flash_layout_validate(&p) == -1);
    p.boot_recovery_verified = 1U;
    p.regions[OU_FLASH_CONFIGURATION].offset = 131072U;
    assert(ou_flash_layout_validate(&p) == -1);
    p.regions[OU_FLASH_CONFIGURATION].offset = 3276800U;
    p.regions[OU_FLASH_APPLICATION].length = UINT32_MAX;
    assert(ou_flash_layout_validate(&p) == -1);
    p.regions[OU_FLASH_APPLICATION].length = 3145728U;
    p.erase_block_bytes = 12345U;
    assert(ou_flash_layout_validate(&p) == -1);
    assert(ou_flash_layout_validate(0) == -1);
}

int main(void) {
    test_memory_decode();
    test_register_map();
    test_opaque_rx_only();
    test_flash_safety_policy();
    puts("NS7520 read-only memory decoder and receive observer: PASS");
    return 0;
}
