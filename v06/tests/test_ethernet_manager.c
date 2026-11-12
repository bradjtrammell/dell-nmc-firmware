#include "ou_ethernet_manager.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {uint32_t egcr,mac1,rxctrl,txctrl;unsigned reads,writes;} mock;
static int rd(void*p,uint32_t a,uint32_t*o) {
    mock*m=p;
    ++m->reads;
    if(a==OU_DMA_RX_CTRL)*o=m->rxctrl;
    else if(a==OU_DMA_TX_CTRL)*o=m->txctrl;
    else if(a==OU_MAC_EGCR)*o=m->egcr;
    else if(a==OU_MAC_MAC1)*o=m->mac1;
    else return -1;
    return 0;
}
static int wr(void*p,uint32_t a,uint32_t v) {
    mock*m=p;
    ++m->writes;
    if(a==OU_DMA_RX_CTRL)m->rxctrl=v;
    else if(a==OU_DMA_TX_CTRL)m->txctrl=v;
    else if(a==OU_MAC_EGCR)m->egcr=v;
    else if(a==OU_MAC_MAC1)m->mac1=v;
    return 0;
}
static int sy(void*p,uint32_t a,size_t n,uint8_t d) {
    (void)p;(void)a;(void)n;(void)d;return 0;
}
int main(void) {
    static const uint8_t mac[6]={2,3,4,5,6,7};
    static ou_ethernet_path path;
    ou_vlan_table vlans={0};
    ou_vlan_interface interfaces[2]={0};
    ou_dma_bd rxbd,txbd;
    uint8_t rx[1518],tx[1518];
    mock m={0};
    ou_dma_bus bus={rd,wr,sy,&m,1,1,1};
    ou_mac_state state={0};
    ou_mac_config config={0};
    interfaces[0].vid=101;
    interfaces[0].ipv4[0]=192;interfaces[0].ipv4[1]=168;
    interfaces[0].ipv4[2]=10;interfaces[0].ipv4[3]=17;
    interfaces[0].prefix_length=24;interfaces[0].allow_local_subnet=1;
    interfaces[0].allowed_services=OU_VLAN_SERVICE_ARP;
    interfaces[1]=interfaces[0];interfaces[1].vid=202;
    interfaces[1].ipv4[0]=10;interfaces[1].ipv4[1]=20;
    interfaces[1].ipv4[2]=0;interfaces[1].ipv4[3]=9;
    assert(ou_vlan_configure(&vlans,mac,interfaces,2,0,1,0,0,0)==0);
    assert(ou_ethernet_path_init_vlans(&path,&vlans,&rxbd,0x08008000,rx,
             0x08010000,&txbd,0x08008400,tx,0x08011000)==0);
    memcpy(config.station_address,mac,6);
    config.link_confirmed=1;
    config.timing_registers_verified=1;
    config.ipgt=0x15;
    config.ipgr=0x12;
    config.clrt=0x370f;
    config.full_duplex=1;
    config.max_frame=1518;
    assert(ou_ethernet_manager_start(&path,&bus,&config,&state)==-1);
    assert(!path.started&&!state.enabled&&!m.writes);
    config.max_frame=1522;
    config.station_address[5]++;
    assert(ou_ethernet_manager_start(&path,&bus,&config,&state)==-1);
    assert(!m.writes);
    config.station_address[5]--;
    assert(ou_ethernet_manager_start(&path,&bus,&config,&state)==0);
    assert(path.started&&state.enabled);
    assert(m.egcr==(OU_MAC_EGCR_ERX|OU_MAC_EGCR_ERXDMA|
                     OU_MAC_EGCR_ETX|OU_MAC_EGCR_ETXDMA));
    assert(ou_ethernet_manager_stop(&path,&bus,&state)==0);
    assert(!path.started&&!state.enabled);
    puts("PASS: 802.1Q trunk MAC MAXF=1522, matching station MAC and DMA/MAC start");
    return 0;
}
