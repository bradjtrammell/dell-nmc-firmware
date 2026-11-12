#include "ou_board_probe.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    uint32_t values[OU_BOARD_READ_COUNT];
    unsigned reads;
    unsigned fail_at;
} emulator;

static const uint32_t expected_addresses[OU_BOARD_READ_COUNT] = {
    OU_REG_GEN_SYS_CONTROL, OU_REG_GEN_SYS_STATUS, OU_REG_GEN_INT_ENABLE,
    OU_REG_ENET_CONTROL, OU_REG_ENET_STATUS,
    OU_REG_SER1_CONTROL_A, OU_REG_SER2_CONTROL_A,
    OU_MEM_CONTROLLER_BASE,
    OU_MEM_CONTROLLER_BASE+0x10U, OU_MEM_CONTROLLER_BASE+0x14U,
    OU_MEM_CONTROLLER_BASE+0x18U, OU_MEM_CONTROLLER_BASE+0x20U,
    OU_MEM_CONTROLLER_BASE+0x24U, OU_MEM_CONTROLLER_BASE+0x28U,
    OU_MEM_CONTROLLER_BASE+0x30U, OU_MEM_CONTROLLER_BASE+0x34U,
    OU_MEM_CONTROLLER_BASE+0x38U, OU_MEM_CONTROLLER_BASE+0x40U,
    OU_MEM_CONTROLLER_BASE+0x44U, OU_MEM_CONTROLLER_BASE+0x48U,
    OU_MEM_CONTROLLER_BASE+0x50U, OU_MEM_CONTROLLER_BASE+0x54U,
    OU_MEM_CONTROLLER_BASE+0x58U
};
static void init(emulator *e) {
    unsigned i;
    *e = (emulator){0};
    e->fail_at = OU_BOARD_READ_COUNT;
    for (i = 0; i < OU_BOARD_READ_COUNT; ++i) e->values[i] = 0U;
    e->values[1] = UINT32_C(0x29800000);
    e->values[8] = UINT32_C(0x02000003);
    e->values[9] = UINT32_C(0xFFE00004);
}
static int reading(void *context, uint32_t address, uint32_t *value) {
    emulator *e = context;
    unsigned i = e->reads++;
    if (!value || i >= OU_BOARD_READ_COUNT || i == e->fail_at ||
        address != expected_addresses[i]) return -1;
    *value = e->values[i];
    return 0;
}
static void test_whitelist_and_safe_unknowns(void) {
    emulator e;
    ou_board_probe out;
    ou_board_probe_policy p = {1U, 1U};
    init(&e);
    assert(ou_board_probe_read(reading, &e, &p, &out) == 0);
    assert(e.reads == OU_BOARD_READ_COUNT);
    assert(out.system_status == UINT32_C(0x29800000));
    assert(out.soc_revision_matches_reference && out.registers_observed);
    assert(out.memory.decoded && !out.memory.capacity_verified);
    assert(out.memory.cs[0].window_bytes == 2097152U);
    assert(!out.application_load_window_decoded);
    assert(!out.h910p_board_verified && !out.ethernet_phy_verified);
    assert(!out.ups_transport_verified && !out.recovery_verified);
    assert(!out.flash_install_permitted);
    init(&e);
    e.values[1] = UINT32_C(0x31000000);
    assert(ou_board_probe_read(reading, &e, &p, &out) == 0);
    assert(!out.soc_revision_matches_reference);
}
static void test_fail_closed_every_read(void) {
    emulator e;
    ou_board_probe out;
    ou_board_probe_policy p = {0};
    unsigned i;
    init(&e);
    assert(ou_board_probe_read(reading, &e, &p, &out) == -1 && !e.reads);
    p.explicitly_authorized = 1U;
    assert(ou_board_probe_read(reading, &e, &p, &out) == -1 && !e.reads);
    p.read_adapter_verified = 2U;
    assert(ou_board_probe_read(reading, &e, &p, &out) == -1 && !e.reads);
    p.read_adapter_verified = 1U;
    assert(ou_board_probe_read(NULL, &e, &p, &out) == -1);
    assert(ou_board_probe_read(reading, &e, &p, NULL) == -1);
    for (i = 0; i < OU_BOARD_READ_COUNT; ++i) {
        init(&e);
        e.fail_at = i;
        out.system_status = UINT32_MAX;
        out.flash_install_permitted = 1U;
        assert(ou_board_probe_read(reading, &e, &p, &out) == -1);
        assert(e.reads == i+1U);
        assert(!out.system_status && !out.memory.decoded);
        assert(!out.flash_install_permitted);
    }
}
static void test_ram_window_not_ram_proof(void) {
    ou_mem_map m = {0};
    uint32_t r[OU_MEM_CS_COUNT][3] = {{0}};
    r[1][0] = UINT32_C(0x08000009);
    r[1][1] = UINT32_C(0xFF000000);
    assert(ou_mem_decode(0U, r, &m) == 0);
    assert(ou_board_application_window(&m, OU_NMC_APPLICATION_LOAD, 1048576U));
    assert(!m.capacity_verified);
    assert(!ou_board_application_window(&m, UINT32_C(0x08F00000), 2097152U));
    assert(!ou_board_application_window(&m, UINT32_C(0xFFFFFFF0), 1024U));
    assert(!ou_board_application_window(&m, OU_NMC_APPLICATION_LOAD, 0U));
    r[2][0] = UINT32_C(0x08000009);
    r[2][1] = UINT32_C(0xFF000000);
    assert(ou_mem_decode(0U, r, &m) == 0);
    assert(!ou_board_application_window(&m, OU_NMC_APPLICATION_LOAD, 1048576U));
}
int main(void) {
    test_whitelist_and_safe_unknowns();
    test_fail_closed_every_read();
    test_ram_window_not_ram_proof();
    puts("NS7520 read-only board inventory and RAM mapping checks: PASS");
    return 0;
}
