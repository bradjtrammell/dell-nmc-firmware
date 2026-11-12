#ifndef OU_NS7520_TIMER_H
#define OU_NS7520_TIMER_H
#include <stdint.h>
#define OU_TIMER1_CTRL UINT32_C(0xFFB00010)
#define OU_TIMER1_STATUS UINT32_C(0xFFB00014)
#define OU_TIMER2_CTRL UINT32_C(0xFFB00018)
#define OU_TIMER2_STATUS UINT32_C(0xFFB0001C)
#define OU_TIMER_TE UINT32_C(0x80000000)
#define OU_TIMER_TIP UINT32_C(0x40000000)
#define OU_TIMER_TCLK UINT32_C(0x08000000)
#define OU_TIMER_COUNT_MAX UINT32_C(0x07ffffff)
typedef int (*ou_timer_read)(void *, uint32_t, uint32_t *);
typedef int (*ou_timer_write)(void *, uint32_t, uint32_t);
typedef struct {
 ou_timer_read read;
 ou_timer_write write;
 void *opaque;
 uint32_t control, status, previous;
 uint32_t ticks_per_ms, remainder, now_ms;
 uint8_t active;
} ou_ns7520_timer;
/* Uses NS7520 27-bit countdown timer with system clock and no IRQ.
 * Caller MUST verify the system clock and own an idle timer exclusively.
 * If polling cannot distinguish wrapping, fail closed rather than guess time. */
int ou_timer_start(ou_ns7520_timer *timer, ou_timer_read read,
                   ou_timer_write write, void *opaque, unsigned timer_index,
                   uint32_t ticks_per_ms, uint8_t ownership_verified,
                   uint8_t clock_verified);
int ou_timer_now(void *opaque, uint32_t *ms);
int ou_timer_stop(ou_ns7520_timer *timer);
#endif
