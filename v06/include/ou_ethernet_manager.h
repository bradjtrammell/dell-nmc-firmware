#ifndef OU_ETHERNET_MANAGER_H
#define OU_ETHERNET_MANAGER_H
#include "ou_ethernet_path.h"
#include "ou_ns7520_mac.h"

/* Enforce MAC address and 1522-byte MAXF before enabling tagged RX/TX.
 * The physical adapter still must verify clocks, PHY link and DMA mapping.
 * No default H910P configuration values are invented here. */
int ou_ethernet_manager_start(ou_ethernet_path *path, const ou_dma_bus *bus,
                              const ou_mac_config *config, ou_mac_state *state);
int ou_ethernet_manager_stop(ou_ethernet_path *path, const ou_dma_bus *bus,
                             ou_mac_state *state);
#endif
