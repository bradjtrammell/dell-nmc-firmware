#include "ou_ns7520_phy.h"
#include <assert.h>
#include <stdio.h>
typedef struct {unsigned address,reg,reads,writes;uint32_t present_mask;} mock;
static int readreg(void*p,uint32_t a,uint32_t*v){mock*m=p;++m->reads;
 if(a==OU_PHY_MIND){*v=0;return 0;}
 if(a==OU_PHY_MRDD){
  if(!(m->present_mask&(UINT32_C(1)<<m->address))){*v=0xffff;return 0;}
  *v=m->reg==2?0x2000U:m->reg==3?0x5c90U:m->reg==1?OU_PHY_BMSR_LINK:0U;
  return 0;
 }
 return -1;
}
static int writereg(void*p,uint32_t a,uint32_t v){mock*m=p;++m->writes;
 if(a==OU_PHY_MADR){m->address=(v>>8)&31U;m->reg=v&31U;return 0;}
 if(a==OU_PHY_MCMD)return 0;
 return -1;
}
int main(void){mock m={0};ou_phy_identity found;uint16_t v=0;
 ou_phy_bus b={readreg,writereg,&m,1U,1U,0U};
 m.present_mask=1U;
 assert(ou_phy_read_register(&b,2U,&v)==0&&v==0x2000);
 assert(ou_phy_discover(&b,&found)==0);
 assert(found.phy_address==0U&&found.identity_valid&&found.link_up);
 m.present_mask=(UINT32_C(1)<<31);
 assert(ou_phy_discover(&b,&found)==0&&found.phy_address==31);
 m.present_mask=1U|(UINT32_C(1)<<3);
 assert(ou_phy_discover(&b,&found)==-1&&!found.identity_valid);
 m.present_mask=0;
 assert(ou_phy_discover(&b,&found)==-1&&!found.identity_valid);
 b.exclusive_mac_ownership=0;
 assert(ou_phy_discover(&b,&found)==-1&&!found.identity_valid);
 b.exclusive_mac_ownership=1;b.phy_clocks_verified=0;
 assert(ou_phy_discover(&b,&found)==-1&&!found.identity_valid);
 puts("PASS: IEEE Clause22 PHY address 0..31, unique discovery and ambiguity denial");
 return 0;
}
