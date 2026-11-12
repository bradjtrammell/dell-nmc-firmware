#include "ou_ns7520_timer.h"
int ou_timer_start(ou_ns7520_timer *timer, ou_timer_read read,
                   ou_timer_write write, void *opaque, unsigned timer_index,
                   uint32_t ticks_per_ms, uint8_t ownership_verified,
                   uint8_t clock_verified) {
 uint32_t previous_ctrl=0U,status=0U;
 if(!timer)return -1;
 *timer=(ou_ns7520_timer){0};
 if(!read||!write||ownership_verified!=1U||clock_verified!=1U||
    ticks_per_ms==0U||ticks_per_ms>OU_TIMER_COUNT_MAX||
    (timer_index!=1U && timer_index!=2U))return -1;
 timer->read=read;timer->write=write;timer->opaque=opaque;
 timer->control=timer_index==1U?OU_TIMER1_CTRL:OU_TIMER2_CTRL;
 timer->status=timer_index==1U?OU_TIMER1_STATUS:OU_TIMER2_STATUS;
 timer->ticks_per_ms=ticks_per_ms;
 if(read(opaque,timer->control,&previous_ctrl)!=0||
    (previous_ctrl&OU_TIMER_TE))return -1;
 if(write(opaque,timer->control,OU_TIMER_TCLK|OU_TIMER_COUNT_MAX)!=0||
    write(opaque,timer->control,OU_TIMER_TE|OU_TIMER_TCLK|OU_TIMER_COUNT_MAX)!=0||
    write(opaque,timer->status,OU_TIMER_TIP)!=0||
    read(opaque,timer->status,&status)!=0){
    (void)write(opaque,timer->control,0U);
    return -1;
 }
 timer->previous=status&OU_TIMER_COUNT_MAX;
 timer->active=1U;
 return 0;
}
/* Fixed 32-bit long division: 32 iterations regardless of tick delta,
 * avoiding an unbounded ARM7 spin and runtime libgcc division dependency. */
static uint32_t ticks_to_ms(uint32_t ticks,uint32_t per,uint32_t *remaining){
 uint32_t whole=0U,part=0U;unsigned i;
 for(i=0U;i<32U;++i){
   part=(part<<1U)|((ticks>>(31U-i))&1U);
   if(part>=per){part-=per;whole|=UINT32_C(1)<<(31U-i);}
 }
 *remaining=part;
 return whole;
}
int ou_timer_now(void *opaque,uint32_t *ms){
 ou_ns7520_timer *timer=opaque;
 uint32_t control,status,current,elapsed;
 if(!ms)return -1;
 *ms=0U;
 if(!timer||!timer->active||!timer->read||!timer->write)return -1;
 if(timer->read(timer->opaque,timer->control,&control)!=0||
    control!=(OU_TIMER_TE|OU_TIMER_TCLK|OU_TIMER_COUNT_MAX)||
    timer->read(timer->opaque,timer->status,&status)!=0)return -1;
 current=status&OU_TIMER_COUNT_MAX;
 if(current>timer->previous){
   if(!(status&OU_TIMER_TIP))return -1;
   elapsed=timer->previous+1U+(OU_TIMER_COUNT_MAX-current);
 }else{
   if(status&OU_TIMER_TIP)return -1;
   elapsed=timer->previous-current;
 }
 if((status&OU_TIMER_TIP)&&
    timer->write(timer->opaque,timer->status,OU_TIMER_TIP)!=0)return -1;
 timer->previous=current;
 timer->remainder+=elapsed;
 /* Avoid division/libgcc on bare ARM7. */
 {
   uint32_t whole,remaining=0U;
   whole=ticks_to_ms(timer->remainder,timer->ticks_per_ms,&remaining);
   timer->remainder=remaining;
   timer->now_ms+=whole;
 }
 *ms=timer->now_ms;
 return 0;
}
int ou_timer_stop(ou_ns7520_timer *timer){
 if(!timer||!timer->active||!timer->write)return -1;
 if(timer->write(timer->opaque,timer->control,0U)!=0)return -1;
 timer->active=0U;
 return 0;
}
