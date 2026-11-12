#include "ou_ns7520_phy.h"
#include <assert.h>
#include <stdio.h>
typedef struct {unsigned reg;uint16_t bmcr,bmsr,anar,lpar;int fail;} mock;
static int rd(void *p,uint32_t addr,uint32_t *v){mock*m=p;
 if(m->fail)return -1;
 if(addr==OU_PHY_MIND){*v=0U;return 0;}
 if(addr==OU_PHY_MRDD){
  if(m->reg==0U)*v=m->bmcr;
  else if(m->reg==1U)*v=m->bmsr;
  else if(m->reg==4U)*v=m->anar;
  else if(m->reg==5U)*v=m->lpar;
  else return -1;
  return 0;
 }
 return -1;
}
static int wr(void*p,uint32_t addr,uint32_t val){mock*m=p;
 if(addr==OU_PHY_MADR){m->reg=(unsigned)(val&31U);return 0;}
 return addr==OU_PHY_MCMD?0:-1;
}
static void check(unsigned tech,uint8_t speed,uint8_t duplex){
 mock m={0};ou_phy_bus b={rd,wr,&m,1U,1U,0U};ou_phy_link link;
 m.bmcr=OU_PHY_BMCR_AN_ENABLE;
 m.bmsr=OU_PHY_BMSR_LINK|OU_PHY_BMSR_AN_COMPLETE;
 m.anar=(uint16_t)tech;m.lpar=(uint16_t)tech;
 assert(ou_phy_resolve_link(&b,&link)==0);
 assert(link.speed100==speed && link.full_duplex==duplex);
 assert(link.link_up && link.autonegotiated);
}
int main(void){
 mock m={0};ou_phy_bus b={rd,wr,&m,1U,1U,0U};ou_phy_link link;
 check(OU_PHY_AN_100_FULL,1,1);
 check(OU_PHY_AN_100_HALF,1,0);
 check(OU_PHY_AN_10_FULL,0,1);
 check(OU_PHY_AN_10_HALF,0,0);
 m.bmcr=OU_PHY_BMCR_AN_ENABLE;
 m.bmsr=OU_PHY_BMSR_LINK|OU_PHY_BMSR_AN_COMPLETE;
 m.anar=OU_PHY_AN_100_FULL;m.lpar=OU_PHY_AN_10_HALF;
 assert(ou_phy_resolve_link(&b,&link)==-1 && !link.link_up);
 m.anar=m.lpar=OU_PHY_AN_100_FULL;
 m.bmsr=OU_PHY_BMSR_LINK;
 assert(ou_phy_resolve_link(&b,&link)==-1);
 m.bmcr=0;m.bmsr=OU_PHY_BMSR_LINK;
 assert(ou_phy_resolve_link(&b,&link)==0 && !link.speed100 && !link.full_duplex);
 m.bmcr=OU_PHY_BMCR_SPEED100|OU_PHY_BMCR_FULL_DUPLEX;
 assert(ou_phy_resolve_link(&b,&link)==0 && link.speed100 && link.full_duplex);
 m.bmcr|=OU_PHY_BMCR_ISOLATE;
 assert(ou_phy_resolve_link(&b,&link)==-1);
 m.bmcr=OU_PHY_BMCR_LOOPBACK;
 assert(ou_phy_resolve_link(&b,&link)==-1);
 m.bmcr=OU_PHY_BMCR_SPEED1000;
 assert(ou_phy_resolve_link(&b,&link)==-1);
 m.bmcr=0;m.bmsr=OU_PHY_BMSR_REMOTE_FAULT|OU_PHY_BMSR_LINK;
 assert(ou_phy_resolve_link(&b,&link)==-1);
 m.fail=1;
 assert(ou_phy_resolve_link(&b,&link)==-1);
 assert(ou_phy_resolve_link(&b,0)==-1);
 puts("PASS: Clause22 autoneg/forced 10/100, duplex and link fault fail-close");
 return 0;
}
