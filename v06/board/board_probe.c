#include "ou_board_probe.h"

static const uint32_t registers_to_read[OU_BOARD_REGISTER_COUNT] = {
    OU_REG_GEN_SYS_CONTROL, OU_REG_GEN_SYS_STATUS, OU_REG_GEN_INT_ENABLE,
    OU_REG_ENET_CONTROL, OU_REG_ENET_STATUS,
    OU_REG_SER1_CONTROL_A, OU_REG_SER2_CONTROL_A
};

int ou_board_application_window(const ou_mem_map *memory,
                                uint32_t ram_start, uint32_t ram_span_bytes) {
    unsigned i;
    if (!memory || memory->decoded != 1U || ram_span_bytes == 0U ||
        ram_start > UINT32_MAX - ram_span_bytes) return 0;
    for (i = 0U; i < OU_MEM_CS_COUNT; ++i) {
        const ou_mem_chip_select *cs = &memory->cs[i];
        if (cs->enabled != 1U || cs->dram != 1U ||
            cs->contiguous != 1U || cs->window_bytes == 0U ||
            (memory->overlapping_mask & (1U << i)) != 0U) continue;
        if (ram_start >= cs->decode_base &&
            ram_start - cs->decode_base <= cs->window_bytes &&
            ram_span_bytes <= cs->window_bytes - (ram_start - cs->decode_base))
            return 1;
    }
    return 0;
}

int ou_board_probe_read(ou_mem_read32 read32, void *context,
                        const ou_board_probe_policy *policy,
                        ou_board_probe *output) {
    uint32_t values[OU_BOARD_REGISTER_COUNT];
    ou_board_probe staged = {0};
    unsigned i;
    if (!output) return -1;
    *output = (ou_board_probe){0};
    if (!read32 || !policy || policy->explicitly_authorized != 1U ||
        policy->read_adapter_verified != 1U) return -1;
    for (i = 0U; i < OU_BOARD_REGISTER_COUNT; ++i) {
        if (read32(context, registers_to_read[i], &values[i]) != 0) return -1;
    }
    if (ou_mem_inspect(read32, context, &staged.memory) != 0) return -1;
    staged.system_control = values[0];
    staged.system_status = values[1];
    staged.interrupt_enable = values[2];
    staged.ethernet_control = values[3];
    staged.ethernet_status = values[4];
    staged.serial_1_control_a = values[5];
    staged.serial_2_control_a = values[6];
    staged.soc_revision_matches_reference =
        (uint8_t)((staged.system_status >> 24) == OU_BOARD_SOC_REVISION);
    staged.registers_observed = 1U;
    staged.application_load_window_decoded = (uint8_t)ou_board_application_window(
        &staged.memory, OU_NMC_APPLICATION_LOAD, UINT32_C(0x00100000));
    *output = staged;
    return 0;
}
