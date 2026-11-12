#ifndef OU_BOARD_PROBE_H
#define OU_BOARD_PROBE_H
#include <stdint.h>
#include "ou_ns7520_mem.h"

/* Digi NS7520 Hardware Reference Rev G; only whitelisted register reads.
 * No FIFO reads, network transmissions, memory writes, or power control. */
#define OU_BOARD_REGISTER_COUNT 7U
#define OU_BOARD_SOC_REVISION UINT32_C(0x29)
#define OU_BOARD_READ_COUNT (OU_BOARD_REGISTER_COUNT + OU_MEM_READS)
#define OU_NMC_APPLICATION_LOAD UINT32_C(0x08004000)
#define OU_REG_GEN_SYS_CONTROL UINT32_C(0xFFB00000)
#define OU_REG_GEN_SYS_STATUS  UINT32_C(0xFFB00004)
#define OU_REG_GEN_INT_ENABLE UINT32_C(0xFFB00030)
#define OU_REG_ENET_CONTROL   UINT32_C(0xFF800000)
#define OU_REG_ENET_STATUS    UINT32_C(0xFF800004)
#define OU_REG_SER1_CONTROL_A UINT32_C(0xFFD00000)
#define OU_REG_SER2_CONTROL_A UINT32_C(0xFFD00040)

typedef struct {
    uint32_t system_control;
    uint32_t system_status;
    uint32_t interrupt_enable;
    uint32_t ethernet_control;
    uint32_t ethernet_status;
    uint32_t serial_1_control_a;
    uint32_t serial_2_control_a;
    ou_mem_map memory;
    uint8_t soc_revision_matches_reference;
    uint8_t registers_observed;
    uint8_t application_load_window_decoded;
    /* Never set by a register snapshot: these require separate proof. */
    uint8_t h910p_board_verified;
    uint8_t ethernet_phy_verified;
    uint8_t ups_transport_verified;
    uint8_t recovery_verified;
    uint8_t flash_install_permitted;
} ou_board_probe;

typedef struct {
    uint8_t explicitly_authorized;
    uint8_t read_adapter_verified;
} ou_board_probe_policy;

/* Pure logical DRAM decoder check; NOT physical capacity verification. */
int ou_board_application_window(const ou_mem_map *memory,
                                uint32_t ram_start, uint32_t ram_span_bytes);
/* Read-only callback on independently authorized recoverable hardware.
 * There is NO on-device callback in v0.6 currently. All failures clear output. */
int ou_board_probe_read(ou_mem_read32 read32, void *context,
                        const ou_board_probe_policy *policy,
                        ou_board_probe *output);
#endif
