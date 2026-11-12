#ifndef OU_NS7520_PHY_H
#define OU_NS7520_PHY_H
#include <stdint.h>

#define OU_PHY_MAC_BASE UINT32_C(0xFF800000)
#define OU_PHY_MCFG (OU_PHY_MAC_BASE + UINT32_C(0x420))
#define OU_PHY_MCMD (OU_PHY_MAC_BASE + UINT32_C(0x424))
#define OU_PHY_MADR (OU_PHY_MAC_BASE + UINT32_C(0x428))
#define OU_PHY_MRDD (OU_PHY_MAC_BASE + UINT32_C(0x430))
#define OU_PHY_MIND (OU_PHY_MAC_BASE + UINT32_C(0x434))
#define OU_PHY_MCMD_READ 1U
#define OU_PHY_MIND_BUSY 1U
#define OU_PHY_MIND_SCANNING 2U
#define OU_PHY_MIND_NOTVALID 4U
#define OU_PHY_MIND_LINKFAIL 8U
#define OU_PHY_POLL_LIMIT 1024U
#define OU_PHY_BMSR_LINK UINT16_C(0x0004)

typedef int (*ou_phy_read32)(void *, uint32_t, uint32_t *);
typedef int (*ou_phy_write32)(void *, uint32_t, uint32_t);
typedef struct {
    ou_phy_read32 read;
    ou_phy_write32 write;
    void *context;
    uint8_t exclusive_mac_ownership;
    uint8_t phy_clocks_verified;
    uint8_t phy_address;
} ou_phy_bus;

typedef struct {
    uint8_t phy_address;
    uint16_t id1;
    uint16_t id2;
    uint16_t bmsr;
    uint8_t link_up;
    uint8_t identity_valid;
} ou_phy_identity;

/* Writes only the MAC's MDIO transaction registers, NOT Ethernet packets,
 * the PHY configuration, or UPS power commands.
 * A verified on-device MMIO adapter is still required. */
int ou_phy_read_register(const ou_phy_bus *bus, uint8_t reg, uint16_t *out);
int ou_phy_identify(const ou_phy_bus *bus, ou_phy_identity *out);
/* Probe IEEE Clause22 addresses 0..31; fail on no device or ambiguity. */
int ou_phy_discover(const ou_phy_bus *bus, ou_phy_identity *out);
#endif
