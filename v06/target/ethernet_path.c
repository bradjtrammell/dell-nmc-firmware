#include "ou_ethernet_path.h"

static uint8_t valid_identity(const ou_net_identity *id) {
    unsigned i; uint8_t zeros=1U;
    if (!id || id->enabled!=1U || (id->mac[0]&1U) ||
        id->ipv4[0]==0U || id->ipv4[0]==127U ||
        id->ipv4[0]>=224U) return 0U;
    for (i=0U;i<6U;++i) if (id->mac[i]!=0U) zeros=0U;
    return (uint8_t)!zeros;
}
static void incr(uint32_t *v) { if (*v!=UINT32_MAX) ++*v; }
/* Verify external bus regions cannot alias across either RX/TX DMA queue. */
static int overlap(uint32_t a, uint32_t alen, uint32_t b, uint32_t blen) {
    uint64_t aa = a, bb = b;
    return aa < bb + blen && bb < aa + alen;
}
static int prepare_queues(ou_ethernet_path *path,
                          ou_dma_bd *rx_desc, uint32_t rx_desc_bus,
                          uint8_t *rx_data, uint32_t rx_data_bus,
                          ou_dma_bd *tx_desc, uint32_t tx_desc_bus,
                          uint8_t *tx_data, uint32_t tx_data_bus) {
    unsigned i;
    if (overlap(rx_desc_bus,1024U,tx_desc_bus,1024U) ||
        overlap(rx_data_bus,OU_DMA_MAX_FRAME,tx_data_bus,OU_DMA_MAX_FRAME) ||
        overlap(rx_desc_bus,1024U,tx_data_bus,OU_DMA_MAX_FRAME) ||
        overlap(tx_desc_bus,1024U,rx_data_bus,OU_DMA_MAX_FRAME) ||
        ou_dma_prepare(&path->rx,rx_desc,rx_desc_bus,rx_data,
                       rx_data_bus,OU_DMA_MAX_FRAME,0U)!=0 ||
        ou_dma_prepare(&path->tx,tx_desc,tx_desc_bus,tx_data,
                       tx_data_bus,OU_DMA_MAX_FRAME,1U)!=0) {
        *path=(ou_ethernet_path){0};
        return -1;
    }
    for (i=0;i<6U;++i) path->identity.mac[i]=path->vlans.mac[i];
    for (i=0;i<4U;++i)
        path->identity.ipv4[i]=path->vlans.interfaces[0].ipv4[i];
    path->identity.enabled=1U;
    return 0;
}
int ou_ethernet_path_init_vlans(ou_ethernet_path *path,
                          const ou_vlan_table *config,
                          ou_dma_bd *rx_desc, uint32_t rx_desc_bus,
                          uint8_t *rx_data, uint32_t rx_data_bus,
                          ou_dma_bd *tx_desc, uint32_t tx_desc_bus,
                          uint8_t *tx_data, uint32_t tx_data_bus) {
    if (!path || !config || (const void *)path == (const void *)config) return -1;
    *path=(ou_ethernet_path){0};
    if (ou_vlan_configure(&path->vlans,config->mac,config->interfaces,
                          config->count,config->native_vid,
                          config->trunk_enabled,config->accept_tagged_native,
                          config->accept_priority_tags,
                          config->priority_reply_tagged)!=0) return -1;
    return prepare_queues(path,rx_desc,rx_desc_bus,rx_data,rx_data_bus,
                          tx_desc,tx_desc_bus,tx_data,tx_data_bus);
}
int ou_ethernet_path_init(ou_ethernet_path *path,
                          const ou_net_identity *identity,
                          ou_dma_bd *rx_desc, uint32_t rx_desc_bus,
                          uint8_t *rx_data, uint32_t rx_data_bus,
                          ou_dma_bd *tx_desc, uint32_t tx_desc_bus,
                          uint8_t *tx_data, uint32_t tx_data_bus) {
    ou_vlan_interface legacy={0};
    unsigned i;
    if (!path) return -1;
    *path=(ou_ethernet_path){0};
    if (!valid_identity(identity)) return -1;
    /* Host compatibility adapter only; a physical trunk must use
     * ou_ethernet_path_init_vlans with explicit masks and ACL policy. */
    legacy.vid=1U;legacy.prefix_length=24U;legacy.allow_local_subnet=1U;
    legacy.allowed_services=OU_VLAN_SERVICE_ARP|OU_VLAN_SERVICE_PING;
    for (i=0;i<4U;++i)legacy.ipv4[i]=identity->ipv4[i];
    if (ou_vlan_configure(&path->vlans,identity->mac,&legacy,1U,1U,
                          0U,0U,0U,0U)!=0) return -1;
    return prepare_queues(path,rx_desc,rx_desc_bus,rx_data,rx_data_bus,
                          tx_desc,tx_desc_bus,tx_data,tx_data_bus);
}
int ou_ethernet_path_start(ou_ethernet_path *path, const ou_dma_bus *bus) {
    if(!path || !path->rx.prepared || !path->tx.prepared || path->started ||
       !path->vlans.count)
        return -1;
    if(ou_dma_start(bus,&path->rx)!=0) return -1;
    path->started=1U;
    return 0;
}
int ou_ethernet_path_poll(ou_ethernet_path *path, const ou_dma_bus *bus) {
    const uint8_t *frame=0;
    size_t length=0U,tx_len=0U;
    int status,net;
    if(!path || !path->started) return -1;
    if (path->tx.active) {
        status=ou_dma_tx_poll(bus,&path->tx);
        if(status<0) {incr(&path->errors);return -1;}
    }
    status=ou_dma_rx_poll(bus,&path->rx,&frame,&length);
    if(status<0) {incr(&path->errors);return -1;}
    if(!status)return 0;
    incr(&path->received);
    if(path->tx.active || path->tx.data_pending) {
        incr(&path->ignored);
        return ou_dma_rx_release(bus,&path->rx)==0 ? 0 : -1;
    }
    net=ou_vlan_reply(&path->vlans,frame,length,path->response,
                     sizeof(path->response),&tx_len);
    if(net>0) {
        if(ou_dma_tx_copy(&path->tx,path->response,tx_len)!=0 ||
           ou_dma_start(bus,&path->tx)!=0) {
            incr(&path->errors);
            return -1;
        }
        incr(&path->answered);
    } else if (net==0) incr(&path->ignored);
    else {incr(&path->errors);return -1;}
    if(ou_dma_rx_release(bus,&path->rx)!=0) {incr(&path->errors);return -1;}
    return net>0 ? 1 : 0;
}
int ou_ethernet_path_stop(ou_ethernet_path *path, const ou_dma_bus *bus) {
    int a,b;
    if(!path || !path->started) return -1;
    a=ou_dma_stop(bus,&path->rx);
    b=ou_dma_stop(bus,&path->tx);
    path->started=0U;
    return a==0&&b==0 ? 0 : -1;
}
