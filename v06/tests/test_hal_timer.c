#include "ou_ns7520_hal.h"
#include "ou_ns7520_timer.h"
#include <assert.h>
#include <stdio.h>
typedef struct {uint32_t control,ctc;unsigned writes,reads;} emulator;
static int rd(void *p,uint32_t a,uint32_t *out){
 emulator *e=p;++e->reads;
 if(a==OU_TIMER2_CTRL){*out=e->control;return 0;}
 if(a==OU_TIMER2_STATUS){*out=e->ctc;return 0;}
 return -1;
}
static int wr(void *p,uint32_t a,uint32_t v){
 emulator *e=p;++e->writes;
 if(a==OU_TIMER2_CTRL){e->control=v;return 0;}
 if(a==OU_TIMER2_STATUS && v==OU_TIMER_TIP){e->ctc&=~OU_TIMER_TIP;return 0;}
 return -1;
}
static int barrier(void*p,uint32_t a,size_t n,uint8_t d){(void)p;(void)a;(void)n;(void)d;return 0;}
int main(void){
 ou_ns7520_hal hal;ou_ns7520_timer timer;emulator e={0};uint32_t millis=99U;
 assert(ou_ns7520_hal_init(&hal,rd,wr,barrier,&e,0x08000000U,0x10000U,1,1,1,1)==0);
 assert(ou_ns7520_hal_read32(&hal,OU_TIMER2_CTRL,&millis)==-1);
 assert(!millis && !e.reads);
 assert(ou_ns7520_hal_write32(&hal,OU_TIMER2_CTRL,123U)==-1&&!e.writes);
 assert(ou_ns7520_hal_timer_bind(&hal,0U,1U,1U)==-1);
 assert(ou_ns7520_hal_timer_bind(&hal,2U,1U,0U)==-1);
 assert(ou_ns7520_hal_timer_bind(&hal,2U,0U,1U)==-1);
 assert(ou_ns7520_hal_timer_bind(&hal,2U,1U,1U)==0);
 assert(ou_ns7520_hal_timer_bind(&hal,1U,1U,1U)==-1);
 assert(ou_ns7520_hal_read32(&hal,OU_TIMER1_CTRL,&millis)==-1);
 assert(ou_ns7520_hal_write32(&hal,OU_TIMER1_CTRL,123U)==-1);
 e.ctc=OU_TIMER_COUNT_MAX-100U;
 assert(ou_timer_start(&timer,ou_ns7520_hal_read32,ou_ns7520_hal_write32,
                       &hal,2U,10U,1U,1U)==0);
 e.ctc=OU_TIMER_COUNT_MAX-123U;
 assert(ou_timer_now(&timer,&millis)==0&&millis==2U);
 assert(ou_timer_stop(&timer)==0);
 assert(ou_ns7520_hal_timer_unbind(&hal)==0);
 assert(ou_ns7520_hal_read32(&hal,OU_TIMER2_CTRL,&millis)==-1);
 assert(ou_ns7520_hal_timer_unbind(&hal)==-1);
 assert(ou_ns7520_hal_timer_bind(&hal,1U,1U,1U)==0);
 assert(ou_ns7520_hal_write32(&hal,OU_TIMER2_CTRL,1U)==-1);
 puts("PASS: HAL timer ownership, whitelist and real monotonic clock integration");
 return 0;
}
