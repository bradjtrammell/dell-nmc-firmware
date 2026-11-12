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
static ou_dma_bd rbd,tbd;
static uint8_t rx[1518],tx[1518];
static uint32_t now=10;
static int fail_clock=0, fail_poll=0;
static unsigned stops=0,starts=0,iterations=0;
static int readreg(void*ctx,uint32_t a,uint32_t*v){(void)ctx;(void)a;*v=0;return 0;}
static int writereg(void*ctx,uint32_t a,uint32_t v){(void)ctx;(void)a;(void)v;return 0;}
static int barr(void*ctx,uint32_t a,size_t n,uint8_t d){(void)ctx;(void)a;(void)n;(void)d;return 0;}
static int clk(void *ctx,uint32_t *v){(void)ctx;if(fail_clock)return -1;*v=now;return 0;}
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
    c=cfg();
    assert(ou_board_runtime_start(&runner,&c)==-1);
    assert(starts==0);
    assert(ou_ns7520_hal_init(&hal,readreg,writereg,barr,0,
             0x08000000U,0x100000U,1,1,1,1)==0);
    c=cfg();c.rx_descriptor_bus=0x09002000U;
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    c=cfg();c.tcp_boot_seed=0;
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    c=cfg();fail_clock=1;
    assert(ou_board_runtime_start(&runner,&c)==-1 && starts==0);
    fail_clock=0;
    assert(ou_board_runtime_start(&runner,&c)==0 && runner.running==1);
    assert(ou_board_runtime_start(&runner,&c)==-1);
    assert(starts==1 && runner.ethernet.tcp_enabled);
    ++now;
    assert(ou_board_runtime_step(&runner)==1);
    assert(iterations==1 && runner.successful_polls==1);
    now=2;
    assert(ou_board_runtime_step(&runner)==-1);
    assert(!runner.running && stops==1 && runner.failed_polls==1);
    now=100;fail_clock=0;
    assert(ou_board_runtime_start(&runner,&c)==0);
    fail_poll=1;
    assert(ou_board_runtime_step(&runner)==-1);
    assert(!runner.running && stops==2 && runner.failed_polls==2);
    assert(ou_board_runtime_stop(&runner)==-1);
    puts("PASS: native runtime fail-closed startup, DMA bounds, monotonic clock and stop");
    return 0;
}
