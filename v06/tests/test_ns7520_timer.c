#include "ou_ns7520_timer.h"
#include <assert.h>
#include <stdio.h>
typedef struct {uint32_t control,ctc;unsigned writes;int reject_write;} mock;
static int rd(void *ctx,uint32_t reg,uint32_t*value){mock*m=ctx;
 if(reg==OU_TIMER2_CTRL){*value=m->control;return 0;}
 if(reg==OU_TIMER2_STATUS){*value=m->ctc;return 0;}
 return -1;
}
static int wr(void *ctx,uint32_t reg,uint32_t value){mock*m=ctx;
 ++m->writes;if(m->reject_write)return -1;
 if(reg==OU_TIMER2_CTRL){m->control=value;return 0;}
 if(reg==OU_TIMER2_STATUS&&value==OU_TIMER_TIP){m->ctc&=~OU_TIMER_TIP;return 0;}
 return -1;
}
int main(void){mock m={0};ou_ns7520_timer t;uint32_t ms=99U;
 m.ctc=OU_TIMER_COUNT_MAX-100U;
 assert(ou_timer_start(&t,rd,wr,&m,2U,10U,0U,1U)==-1&&!m.writes);
 assert(ou_timer_start(&t,rd,wr,&m,2U,10U,1U,1U)==0);
 assert(t.active&&t.previous==OU_TIMER_COUNT_MAX-100U);
 m.ctc=OU_TIMER_COUNT_MAX-125U;
 assert(ou_timer_now(&t,&ms)==0&&ms==2U&&t.remainder==5U);
 m.ctc=OU_TIMER_COUNT_MAX-135U;
 assert(ou_timer_now(&t,&ms)==0&&ms==3U&&t.remainder==5U);
 m.ctc=(OU_TIMER_COUNT_MAX-50U)|OU_TIMER_TIP;
 assert(ou_timer_now(&t,&ms)==0&&ms>3U&&!(m.ctc&OU_TIMER_TIP));
 m.ctc=(OU_TIMER_COUNT_MAX-60U)|OU_TIMER_TIP;
 assert(ou_timer_now(&t,&ms)==-1);
 assert(ou_timer_stop(&t)==0&&!t.active&&m.control==0U);
 assert(ou_timer_now(&t,&ms)==-1&&ms==0U);
 /* Verify bounded max-interval conversion, including 1 tick/ms. */
 m.control=OU_TIMER_TE|OU_TIMER_TCLK|OU_TIMER_COUNT_MAX;
 t.active=1U;t.previous=OU_TIMER_COUNT_MAX;
 t.remainder=0U;t.now_ms=0U;t.ticks_per_ms=1U;m.ctc=0U;
 assert(ou_timer_now(&t,&ms)==0&&ms==OU_TIMER_COUNT_MAX&&t.remainder==0U);
 t.previous=OU_TIMER_COUNT_MAX;t.remainder=0U;t.now_ms=0U;
 t.ticks_per_ms=251U;m.ctc=0U;
 assert(ou_timer_now(&t,&ms)==0&&ms==OU_TIMER_COUNT_MAX/251U);
 assert(t.remainder==OU_TIMER_COUNT_MAX%251U);
 m.control=OU_TIMER_TE;
 assert(ou_timer_start(&t,rd,wr,&m,2U,10U,1U,1U)==-1);
 puts("PASS: NS7520 real timer register sequencing, monotonic ms and wrap failure");
 return 0;
}
