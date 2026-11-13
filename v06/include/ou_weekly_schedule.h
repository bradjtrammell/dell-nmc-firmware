#ifndef OU_WEEKLY_SCHEDULE_H
#define OU_WEEKLY_SCHEDULE_H
#include "ou_config_journal.h"

#define OU_WEEK_DAYS 7U
#define OU_WEEK_SLOT_SECONDS 1800U
#define OU_WEEK_DAY_SECONDS 86400U
#define OU_WEEK_DISABLED (-1)
#define OU_WEEK_RECORD_BYTES 64U
#define OU_WEEK_RECORD_VERSION 1U

/* OEM parity: seven daily ON/OFF pairs with -1 disabled, 30-minute
 * granularity, 00:00..23:30. Weekday 0=Sunday, 6=Saturday.
 * This is a planner and format, NOT a hardware power-control engine. */
typedef struct {
    int32_t off[OU_WEEK_DAYS];
    int32_t on[OU_WEEK_DAYS];
} ou_weekly_schedule;
typedef struct {
    uint8_t weekday;
    uint8_t power_on;
    uint32_t seconds_of_day;
    uint32_t seconds_until;
} ou_weekly_occurrence;

void ou_weekly_schedule_init(ou_weekly_schedule *schedule);
int ou_weekly_schedule_validate(const ou_weekly_schedule *schedule);
/* Returns 1 with the nearest event, 0 if no events, -1 on invalid input.
 * For exactly-now events, seconds_until=0; caller must implement a
 * separate stable execution cursor before any physical power action. */
int ou_weekly_schedule_next(const ou_weekly_schedule *schedule,
                            uint8_t weekday,uint32_t seconds_of_day,
                            ou_weekly_occurrence *next);
/* Versioned exact 64-byte big-endian image. Commit only through
 * independent verified SCHEDULE journal slots, never through CONFIG or
 * EVENT partitions. Decoding is all-or-nothing. */
int ou_weekly_schedule_encode(const ou_weekly_schedule *schedule,
                              uint8_t *bytes,size_t capacity);
int ou_weekly_schedule_decode(ou_weekly_schedule *schedule,
                              const uint8_t *bytes,size_t length);
typedef int (*ou_week_authorize_fn)(void *context);
int ou_weekly_schedule_checkpoint(const ou_weekly_schedule *schedule,
                                  const ou_journal_io *dedicated_schedule,
                                  ou_week_authorize_fn authorize,
                                  void *auth_context,
                                  ou_journal_record *record);
int ou_weekly_schedule_restore(ou_weekly_schedule *schedule,
                               const ou_journal_io *dedicated_schedule,
                               ou_journal_record *record);
#endif
