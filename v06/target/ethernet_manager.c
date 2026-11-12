#include "ou_ethernet_manager.h"
static int same(const uint8_t *a,const uint8_t *b,size_t size) {
    size_t i;
    for(i=0;i<size;++i) if(a[i]!=b[i]) return 0;
    return 1;
}
int ou_ethernet_manager_start(ou_ethernet_path *path,const ou_dma_bus *bus,
                              const ou_mac_config *cfg,ou_mac_state *state) {
    if(!path||!bus||!cfg||!state||path->started||!path->vlans.count ||
       !same(cfg->station_address,path->vlans.mac,6U) ||
       (path->vlans.trunk_enabled && cfg->max_frame!=1522U) ||
       (!path->vlans.trunk_enabled && cfg->max_frame<1518U)) return -1;
    if(ou_mac_configure(bus,cfg,state)!=0) return -1;
    if(ou_ethernet_path_start(path,bus)!=0) {
        (void)ou_mac_disable(bus,state);
        return -1;
    }
    if(ou_mac_activate(bus,state,&path->rx,&path->tx)!=0) {
        (void)ou_mac_disable(bus,state);
        (void)ou_ethernet_path_stop(path,bus);
        return -1;
    }
    return 0;
}
int ou_ethernet_manager_stop(ou_ethernet_path *path,const ou_dma_bus *bus,
                             ou_mac_state *state) {
    int a,b;
    if(!path||!bus||!state||!path->started)return -1;
    a=ou_mac_disable(bus,state);
    b=ou_ethernet_path_stop(path,bus);
    return a==0&&b==0 ? 0 : -1;
}
