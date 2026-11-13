#include "ou_board_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Boot coordinator tests stub external network routines. Existing Ethernet,
 * VLAN and TCP acceptance suites exercise the complete network stack. */
static ou_board_runtime runner;
static ou_vlan_table policy;
static ou_ns7520_hal hal;
static ou_mac_config mac;
static ou_board_probe snapshot;
static ou_dma_bd rbd,tbd;
static uint8_t rx[1518],tx[1518];
static uint32_t now=10;
static int fail_clock=0, fail_poll=0;
static uint16_t phy_bmcr=0U;static int fail_phy_link=0;static unsigned phyaddr=0,phyreg=0;
static unsigned stops=0,starts=0,iterations=0;
static ou_ups_monitor mock_monitor;
static unsigned ups_ticks=0U,ups_revokes=0U;
static int ups_failed=0;
int ou_ups_monitor_step(ou_ups_monitor *monitor,uint32_t ms){
    (void)ms;assert(monitor==&mock_monitor);
    ++ups_ticks;
    return ups_failed?-1:1;
}
void ou_ups_monitor_revoke(ou_ups_monitor *monitor){
    assert(monitor==&mock_monitor);
    ++ups_revokes;
    monitor->initialized=0U;
}
static int readreg(void*ctx,uint32_t a,uint32_t*v){(void)ctx;
 if(a==OU_PHY_MIND){*v=0;return 0;}
 if(a==OU_PHY_MRDD){
  if(phyaddr!=0U)*v=0xffffU;
  else if(phyreg==2U)*v=0x2000U;
  else if(phyreg==3U)*v=0x5c90U;
  else if(phyreg==0U)*v=phy_bmcr;
  else if(phyreg==1U)*v=fail_phy_link?0U:OU_PHY_BMSR_LINK;
  else *v=0U;
  return 0;
 }
 *v=0;return 0;
}
static int writereg(void*ctx,uint32_t a,uint32_t v){(void)ctx;
 if(a==OU_PHY_MADR){phyaddr=(v>>8U)&31U;phyreg=v&31U;}
 return 0;
}
static int barr(void*ctx,uint32_t a,size_t n,uint8_t d){(void)ctx;(void)a;(void)n;(void)d;return 0;}
static int clk(void *ctx,uint32_t *v){(void)ctx;if(fail_clock)return -1;*v=now;return 0;}
static int dma_match(void *ctx,const void *p,uint32_t addr,size_t bytes){
 (void)ctx;
 return (p==&rbd && addr==0x08002000U && bytes==sizeof(ou_dma_bd)) ||
        (p==&tbd && addr==0x08002400U && bytes==sizeof(ou_dma_bd)) ||
        (p==rx && addr==0x08003000U && bytes==OU_DMA_MAX_FRAME) ||
        (p==tx && addr==0x08004000U && bytes==OU_DMA_MAX_FRAME);
}
int ou_vlan_validate(const ou_vlan_table*t){return t && t->count==1U;}
int ou_ethernet_path_init_vlans(ou_ethernet_path*p,const ou_vlan_table*v,
    ou_dma_bd*rb,uint32_t rba,uint8_t*rxbuf,uint32_t rxba,
    ou_dma_bd*tb,uint32_t tba,uint8_t*txbuf,uint32_t txba){
    (void)rb;(void)rba;(void)rxbuf;(void)rxba;(void)tb;(void)tba;(void)txbuf;(void)txba;
    p->vlans=*v;return 0;
}
int ou_ethernet_path_tcp_enable(ou_ethernet_path*p,uint32_t seed,ou_tcp_app cb,void *ctx){
    (void)ctx;assert(seed && cb);p->tcp_enabled=1;return 0;
}
int ou_ethernet_manager_start(ou_ethernet_path*p,const ou_dma_bus*b,const ou_mac_config*c,ou_mac_state*m){
    (void)b;(void)c;++starts;p->started=1;m->enabled=1;return 0;
}
int ou_ethernet_path_poll_at(ou_ethernet_path*p,const ou_dma_bus*b,uint32_t ms){
    (void)p;(void)b;(void)ms;++iterations;return fail_poll?-1:1;
}
int ou_ethernet_manager_stop(ou_ethernet_path*p,const ou_dma_bus*b,ou_mac_state*m){
    (void)b;++stops;p->started=0;m->enabled=0;return 0;
}
static int app(void*p,const uint8_t*req,size_t n,uint32_t t,char*out,size_t cap,size_t*len){
    (void)p;(void)req;(void)n;(void)t;(void)out;(void)cap;(void)len;return 0;
}
static ou_board_boot_config cfg(void){
    ou_board_boot_config c={0};
    c.vlans=&policy;c.mac=&mac;c.hal=&hal;
    c.board_snapshot=&snapshot;c.dma_resolve=dma_match;
    c.rx_descriptor=&rbd;c.rx_descriptor_bus=0x08002000U;
    c.rx_data=rx;c.rx_data_bus=0x08003000U;
    c.tx_descriptor=&tbd;c.tx_descriptor_bus=0x08002400U;
    c.tx_data=tx;c.tx_data_bus=0x08004000U;
    c.clock_ms=clk;c.tcp_boot_seed=0x123456U;
    c.http_application=app;c.enable_tcp_http=1;
    return c;
}
int main(void){
    ou_board_boot_config c;
    memset(&runner,0,sizeof(runner));memset(&policy,0,sizeof(policy));
    policy.count=1;
    mac.link_confirmed=1U;
    {
      uint32_t regs[OU_MEM_CS_COUNT][3]={{0}};
      regs[1][0]=UINT32_C(0x08000009);
      regs[1][1]=UINT32_C(0xFF000000);
      assert(ou_mem_decode(0U,regs,&snapshot.memory)==0);
      snapshot.registers_observed=1U;
      snapshot.soc_revision_matches_reference=1U;
      snapshot.application_load_window_decoded=1U;
    }
    c=cfg();
    assert(ou_board_runtime_start(&runner,&c)==-1);
    assert(starts==0);
    assert(ou_ns7520_hal_init(&hal,readreg,writereg,barr,0,
             0x08000000U,0x100000U,1,1,1,1)==0);
    c=cfg();
    /* These fields are intentionally zero for passive MMIO capture.
     * A real H910P BSP must qualify the installed card AND its rescue
     * path independently of the generic NS7520 chip revision. */
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    snapshot.h910p_board_verified=1U;
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    snapshot.ethernet_phy_verified=1U;
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    snapshot.recovery_verified=1U;
    c=cfg();c.rx_descriptor_bus=0x09002000U;
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    c=cfg();c.rx_data=tx; /* Same bus value but wrong CPU buffer. */
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    c=cfg();c.dma_resolve=0;
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    c=cfg();snapshot.soc_revision_matches_reference=0U;
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    snapshot.soc_revision_matches_reference=1U;
    c=cfg();snapshot.application_load_window_decoded=0U;
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    snapshot.application_load_window_decoded=1U;
    c=cfg();c.tcp_boot_seed=0;
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    c=cfg();fail_clock=1;
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    fail_clock=0;
    fail_phy_link=1;
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    fail_phy_link=0;
    phy_bmcr=OU_PHY_BMCR_FULL_DUPLEX;
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    phy_bmcr=0U;
    assert(ou_board_runtime_start(&runner,&c)==0 && runner.running==1);
    assert(runner.phy.identity_valid && runner.phy.link_up &&
           runner.phy.phy_address==0U);
    assert(ou_board_runtime_start(&runner,&c)==-1);
    assert(starts==1 && runner.ethernet.tcp_enabled);
    ++now;
    assert(ou_board_runtime_step(&runner)==1);
    assert(iterations==1 && runner.successful_polls==1);
    now=1012;fail_phy_link=1;
    assert(ou_board_runtime_step(&runner)==-1);
    assert(!runner.running && stops==1 && runner.failed_polls==1);
    fail_phy_link=0;
    now=2;
    assert(ou_board_runtime_step(&runner)==-1);
    assert(!runner.running && stops==1 && runner.failed_polls==1);
    now=100;fail_clock=0;
    assert(ou_board_runtime_start(&runner,&c)==0);
    fail_poll=1;
    assert(ou_board_runtime_step(&runner)==-1);
    assert(!runner.running && stops==2 && runner.failed_polls==2);
    assert(ou_board_runtime_stop(&runner)==-1);
    /* An optional, verified UPS monitor is now part of the native board
     * loop. Its failure cannot take HTTP/management off the network. */
    now=2000U;fail_poll=0;ups_failed=0;
    mock_monitor=(ou_ups_monitor){0};
    c=cfg();c.ups_monitor=&mock_monitor;
    assert(ou_board_runtime_start(&runner,&c)==-1);
    mock_monitor.initialized=1U;
    assert(ou_board_runtime_start(&runner,&c)==-1);
    mock_monitor.link.initialized=1U;
    mock_monitor.hid.initialized=1U;
    mock_monitor.report_count=1U;
    assert(ou_board_runtime_start(&runner,&c)==0);
    assert(runner.running==1 && runner.ups_monitor==&mock_monitor);
    assert(ou_board_runtime_step(&runner)==1);
    assert(runner.ups_polls==1U && ups_ticks==1U && ups_revokes==0U);
    ups_failed=1;
    now=2001U;
    assert(ou_board_runtime_step(&runner)==1);
    assert(runner.running==1 && runner.ups_failures==1U);
    assert(ups_ticks==2U && ups_revokes==1U && !runner.ups_monitor);
    now=2002U;
    assert(ou_board_runtime_step(&runner)==1 && ups_ticks==2U);
    assert(ou_board_runtime_stop(&runner)==0);
    assert(!runner.running && ups_revokes==1U);
    puts("PASS: native runtime fail-closed startup, DMA bounds, monotonic clock and stop");
    return 0;
}
