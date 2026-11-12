#include "ou_ns7520_phy.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    uint32_t writes[128];
    uint32_t addresses[128];
    unsigned writes_done;
    unsigned polls;
    unsigned fail_at_read;
    unsigned fail_at_write;
    unsigned reads_done;
    int hung;
    unsigned response_reg;
} emulator;
static int r32(void *context, uint32_t address, uint32_t *value) {
    emulator *em = context;
    unsigned index = em->reads_done++;
    if (index == em->fail_at_read) return -1;
    if (address == OU_PHY_MIND) {
        if (em->hung && em->polls++ > 0U) *value = OU_PHY_MIND_BUSY;
        else *value = 0U;
        return 0;
    }
    if (address == OU_PHY_MRDD) {
        switch (em->response_reg) {
            case 2U: *value = UINT32_C(0x2000); break;
            case 3U: *value = UINT32_C(0x5c90); break;
            case 1U: *value = OU_PHY_BMSR_LINK; break;
            default: return -1;
        }
        return 0;
    }
    return -1;
}
static int w32(void *context, uint32_t address, uint32_t value) {
    emulator *em = context;
    unsigned index = em->writes_done++;
    if (index == em->fail_at_write || index >= 128U) return -1;
    em->addresses[index] = address;
    em->writes[index] = value;
    if (address == OU_PHY_MADR) em->response_reg = value & 31U;
    return address == OU_PHY_MCMD || address == OU_PHY_MADR ? 0 : -1;
}
static void reset(emulator *e) {
    *e = (emulator){0};
    e->fail_at_read = 999999U;
    e->fail_at_write = 999999U;
}
static void test_read_and_identify(void) {
    emulator e; ou_phy_identity out; uint16_t v=0U;
    ou_phy_bus b={r32,w32,&e,1U,1U,3U};
    reset(&e);
    assert(ou_phy_read_register(&b,2U,&v)==0 && v==0x2000U);
    assert(e.writes_done==4U);
    assert(e.addresses[0]==OU_PHY_MCMD && e.writes[0]==0U);
    assert(e.addresses[1]==OU_PHY_MADR && e.writes[1]==0x302U);
    assert(e.addresses[2]==OU_PHY_MCMD && e.writes[2]==OU_PHY_MCMD_READ);
    assert(e.addresses[3]==OU_PHY_MCMD && e.writes[3]==0U);
    reset(&e);
    assert(ou_phy_identify(&b,&out)==0);
    assert(out.identity_valid==1U && out.link_up==1U);
    assert(out.id1==0x2000U && out.id2==0x5c90U && out.phy_address==3U);
    assert(e.writes_done==16U);
}
static void test_denied_conditions(void) {
    emulator e; uint16_t out=42;
    ou_phy_bus b={r32,w32,&e,0U,1U,3U};
    reset(&e);
    assert(ou_phy_read_register(&b,2U,&out)==-1 && out==0U && !e.writes_done);
    b.exclusive_mac_ownership=1U; b.phy_clocks_verified=0U;
    assert(ou_phy_read_register(&b,2U,&out)==-1 && !e.writes_done);
    b.phy_clocks_verified=1U;b.phy_address=0U;
    assert(ou_phy_read_register(&b,2U,&out)==0 && e.writes_done==4U);
    reset(&e);
    b.phy_address=32U;
    assert(ou_phy_read_register(&b,2U,&out)==-1 && !e.writes_done);
    b.phy_address=3U;
    assert(ou_phy_read_register(&b,32U,&out)==-1 && !e.writes_done);
    assert(ou_phy_read_register(&b,2U,NULL)==-1 && !e.writes_done);
}
static void test_errors_and_timeout(void) {
    emulator e; uint16_t out=42;
    ou_phy_bus b={r32,w32,&e,1U,1U,3U};
    unsigned i;
    for(i=0U;i<4U;++i) {
        reset(&e);e.fail_at_write=i;
        assert(ou_phy_read_register(&b,2U,&out)==-1);
        assert(out==0U);
    }
    for(i=0U;i<3U;++i) {
        reset(&e);e.fail_at_read=i;
        assert(ou_phy_read_register(&b,2U,&out)==-1);
        assert(out==0U);
    }
    reset(&e);e.hung=1;
    assert(ou_phy_read_register(&b,2U,&out)==-1);
    assert(out==0U);
    assert(e.writes[e.writes_done-1]==0U);
    assert(e.polls>=OU_PHY_POLL_LIMIT);
}
int main(void) {
    test_read_and_identify();
    test_denied_conditions();
    test_errors_and_timeout();
    puts("PASS: NS7520 Clause22 PHY MDIO read, clock/owner gates, abort and bounds");
    return 0;
}
