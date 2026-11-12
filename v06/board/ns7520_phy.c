#include "ou_ns7520_phy.h"

static int valid(const ou_phy_bus *bus) {
    return bus && bus->read && bus->write &&
           bus->exclusive_mac_ownership == 1U &&
           bus->phy_clocks_verified == 1U &&
           bus->phy_address <= 31U;
}

int ou_phy_read_register(const ou_phy_bus *bus, uint8_t reg, uint16_t *out) {
    uint32_t mind, result;
    unsigned count;
    int started = 0, rc = -1;
    if (!out) return -1;
    *out = 0U;
    if (!valid(bus) || reg > 31U) return -1;
    /* An existing driver may own the MAC/PHY. Never seize a busy bus. */
    if (bus->read(bus->context, OU_PHY_MIND, &mind) != 0 ||
        (mind & (OU_PHY_MIND_BUSY | OU_PHY_MIND_SCANNING)) != 0U) return -1;
    if (bus->write(bus->context, OU_PHY_MCMD, 0U) != 0) return -1;
    started = 1;
    if (bus->write(bus->context, OU_PHY_MADR,
                   ((uint32_t)bus->phy_address << 8) | (uint32_t)reg) != 0)
        goto finish;
    if (bus->write(bus->context, OU_PHY_MCMD, OU_PHY_MCMD_READ) != 0)
        goto finish;
    for (count = 0U; count < OU_PHY_POLL_LIMIT; ++count) {
        if (bus->read(bus->context, OU_PHY_MIND, &mind) != 0) goto finish;
        if ((mind & (OU_PHY_MIND_BUSY | OU_PHY_MIND_NOTVALID)) == 0U) {
            if (bus->read(bus->context, OU_PHY_MRDD, &result) != 0) goto finish;
            *out = (uint16_t)(result & 0xFFFFU);
            rc = 0;
            break;
        }
    }
finish:
    /* Clear READ after an interrupted transaction or timeout. */
    if (started && bus->write(bus->context, OU_PHY_MCMD, 0U) != 0) rc = -1;
    if (rc != 0) *out = 0U;
    return rc;
}

int ou_phy_identify(const ou_phy_bus *bus, ou_phy_identity *out) {
    ou_phy_identity temp = {0};
    uint16_t status1;
    if (!out) return -1;
    *out = temp;
    if (!valid(bus)) return -1;
    temp.phy_address = bus->phy_address;
    /* IEEE 802.3 Clause 22 PHY identification: registers 2 and 3. */
    if (ou_phy_read_register(bus, 2U, &temp.id1) != 0 ||
        ou_phy_read_register(bus, 3U, &temp.id2) != 0) return -1;
    if (temp.id1 == 0U || temp.id1 == UINT16_MAX) return -1;
    /* PHY BMSR link bit may be latch-low; read twice. */
    if (ou_phy_read_register(bus, 1U, &status1) != 0 ||
        ou_phy_read_register(bus, 1U, &temp.bmsr) != 0) return -1;
    (void)status1;
    temp.link_up = (uint8_t)((temp.bmsr & OU_PHY_BMSR_LINK) != 0U);
    temp.identity_valid = 1U;
    *out = temp;
    return 0;
}

/* Read-only PHY discovery. No PHY reset/config writes are performed.
 * An ambiguous MDIO bus never causes us to select an arbitrary device. */
int ou_phy_discover(const ou_phy_bus *bus, ou_phy_identity *out) {
    ou_phy_bus query;
    ou_phy_identity result={0};
    ou_phy_identity candidate={0};
    uint8_t address;
    int discovered=0;
    if (!out) return -1;
    *out=result;
    if (!bus || !bus->read || !bus->write ||
        bus->exclusive_mac_ownership!=1U ||
        bus->phy_clocks_verified!=1U) return -1;
    query=*bus;
    for (address=0U;address<32U;++address) {
        query.phy_address=address;
        if (ou_phy_identify(&query,&candidate)!=0) continue;
        if (discovered) return -1;
        result=candidate;
        discovered=1;
    }
    if (!discovered) return -1;
    *out=result;
    return 0;
}
