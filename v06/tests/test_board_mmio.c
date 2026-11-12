#include "ou_ns7520_board_mmio.h"
#include <assert.h>
#include <stdio.h>

typedef struct {unsigned reads;unsigned fail_at;uint32_t last;} mock;
static const uint32_t addresses[OU_BOARD_READ_COUNT]={
 OU_REG_GEN_SYS_CONTROL,OU_REG_GEN_SYS_STATUS,OU_REG_GEN_INT_ENABLE,
 OU_REG_ENET_CONTROL,OU_REG_ENET_STATUS,OU_REG_SER1_CONTROL_A,
 OU_REG_SER2_CONTROL_A,
 OU_MEM_CONTROLLER_BASE,
 OU_MEM_CONTROLLER_BASE+0x10U,OU_MEM_CONTROLLER_BASE+0x14U,
 OU_MEM_CONTROLLER_BASE+0x18U,OU_MEM_CONTROLLER_BASE+0x20U,
 OU_MEM_CONTROLLER_BASE+0x24U,OU_MEM_CONTROLLER_BASE+0x28U,
 OU_MEM_CONTROLLER_BASE+0x30U,OU_MEM_CONTROLLER_BASE+0x34U,
 OU_MEM_CONTROLLER_BASE+0x38U,OU_MEM_CONTROLLER_BASE+0x40U,
 OU_MEM_CONTROLLER_BASE+0x44U,OU_MEM_CONTROLLER_BASE+0x48U,
 OU_MEM_CONTROLLER_BASE+0x50U,OU_MEM_CONTROLLER_BASE+0x54U,
 OU_MEM_CONTROLLER_BASE+0x58U};
static int physical(void *opaque,uint32_t address,uint32_t *out) {
 mock *m=opaque;unsigned i=m->reads++;
 if(!out||i>=OU_BOARD_READ_COUNT||i==m->fail_at||
    address!=addresses[i])return -1;
 m->last=address;
 /* Artificial CS1 DRAM 16 MiB aperture covering linked address. */
 if(i==1U)*out=UINT32_C(0x29000000);
 else if(i==11U)*out=UINT32_C(0x08000009);
 else if(i==12U)*out=UINT32_C(0xFF000000);
 else *out=0U;
 return 0;
}
int main(void) {
 mock mockbus={0};
 ou_ns7520_probe_source s={physical,&mockbus,0U,1U,1U};
 ou_board_probe p;
 uint32_t value=777U;
 unsigned i;
 mockbus.fail_at=OU_BOARD_READ_COUNT;
 assert(ou_ns7520_board_snapshot(&s,&p)==-1&&!mockbus.reads);
 s.explicitly_authorized=1U;s.recovery_access_verified=0U;
 assert(ou_ns7520_board_snapshot(&s,&p)==-1&&!mockbus.reads);
 s.recovery_access_verified=1U;s.peripheral_mapping_verified=0U;
 assert(ou_ns7520_board_snapshot(&s,&p)==-1&&!mockbus.reads);
 s.peripheral_mapping_verified=1U;
 assert(ou_ns7520_probe_read32(&s,UINT32_C(0xFFC0001C),&value)==-1&&!value);
 assert(ou_ns7520_probe_read32(&s,UINT32_C(0xFFD00004),&value)==-1);
 assert(ou_ns7520_probe_read32(&s,UINT32_C(0x02000000),&value)==-1);
 assert(ou_ns7520_probe_read32(&s,UINT32_C(0xFFB00020),&value)==-1);
 assert(ou_ns7520_probe_read32(&s,UINT32_C(0xFFC0005C),&value)==-1);
 assert(!mockbus.reads);
 assert(ou_ns7520_board_snapshot(&s,&p)==0);
 assert(mockbus.reads==OU_BOARD_READ_COUNT);
 assert(p.registers_observed&&p.soc_revision_matches_reference);
 assert(p.application_load_window_decoded&&p.memory.decoded);
 assert(!p.memory.capacity_verified && !p.h910p_board_verified);
 assert(!p.ups_transport_verified&&!p.recovery_verified&&!p.flash_install_permitted);
 assert(p.memory.cs[1].decode_base==UINT32_C(0x08000000));
 assert(p.memory.cs[1].window_bytes==UINT32_C(0x01000000));
 assert(ou_board_application_window(&p.memory,UINT32_C(0x08004000),UINT32_C(0x00100000)));
 for(i=0;i<OU_BOARD_READ_COUNT;++i){
  mockbus.reads=0U;mockbus.fail_at=i;
  p.registers_observed=1U;p.flash_install_permitted=1U;
  assert(ou_ns7520_board_snapshot(&s,&p)==-1);
  assert(mockbus.reads==i+1U);
  assert(!p.registers_observed&&!p.flash_install_permitted);
 }
 assert(ou_ns7520_board_snapshot(&s,0)==-1);
 puts("PASS: qualified read-only NS7520 board snapshot, 23-register whitelist, DRAM decode and fail-close");
 return 0;
}
