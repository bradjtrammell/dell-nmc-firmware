#include "ou_ns7520_board_mmio.h"

static int allowed_register(uint32_t address) {
    if(address == OU_REG_GEN_SYS_CONTROL ||
       address == OU_REG_GEN_SYS_STATUS ||
       address == OU_REG_GEN_INT_ENABLE ||
       address == OU_REG_ENET_CONTROL ||
       address == OU_REG_ENET_STATUS ||
       address == OU_REG_SER1_CONTROL_A ||
       address == OU_REG_SER2_CONTROL_A ||
       address == OU_MEM_CONTROLLER_BASE) return 1;
    /* MEM MMCR + 5 groups of BAR, ORA, ORB. Reject reserved offsets. */
    if(address>=OU_MEM_CONTROLLER_BASE+UINT32_C(0x10) &&
       address<=OU_MEM_CONTROLLER_BASE+UINT32_C(0x58)) {
        uint32_t offset=(address-OU_MEM_CONTROLLER_BASE-UINT32_C(0x10));
        return (offset % UINT32_C(0x10))<=UINT32_C(8) &&
               (offset % UINT32_C(4))==0U;
    }
    return 0;
}
static int ready(const ou_ns7520_probe_source *source) {
    return source && source->physical_read &&
           source->explicitly_authorized==1U &&
           source->peripheral_mapping_verified==1U &&
           source->recovery_access_verified==1U;
}
int ou_ns7520_probe_read32(void *context,uint32_t address,uint32_t *value) {
    ou_ns7520_probe_source *source=context;
    if(!value)return -1;
    *value=0U;
    if(!ready(source)||!allowed_register(address))return -1;
    return source->physical_read(source->context,address,value);
}
int ou_ns7520_board_snapshot(const ou_ns7520_probe_source *source,
                             ou_board_probe *snapshot) {
    ou_board_probe_policy policy={1U,1U};
    if(!snapshot)return -1;
    *snapshot=(ou_board_probe){0};
    if(!ready(source))return -1;
    return ou_board_probe_read(ou_ns7520_probe_read32,
                (void *)source,&policy,snapshot);
}
