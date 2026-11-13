# v0.6 weekly power schedule — native planner and review editor

The independent H910P v0.6 development branch now implements the **seven-day, half-hour weekly ON/OFF planning format** represented by the project’s existing configuration schema. The schedule **does not currently control physical outlets**: there is no qualified RTC, authenticated power-operation driver, or verified schedule flash partition on the installed H910P.

## Native model

`src/weekly_schedule.c` and `include/ou_weekly_schedule.h` contain a fixed-memory 7-day schedule with one independent `offSeconds` and `onSeconds` value per weekday, Sunday=0 to Saturday=6. Values are `-1` (operation disabled), or a multiple of 1,800 seconds in the range 0–84,600 (00:00–23:30). Same-day simultaneous ON/OFF is rejected; valid cross-midnight pairs are supported.

The `ou_weekly_schedule_next` planner accepts a **caller-verified weekday and seconds since midnight** and returns the nearest planned operation within a week, with its weekday, action, time and seconds until occurrence. It returns no event if the schedule is empty. Events exactly at the requested time return `seconds_until=0`. **This function does not execute the event**, consume a command, or prevent duplicate physical execution; an approved device-side scheduler will additionally require a trusted RTC/NTP policy, idempotent occurrence IDs, authorization and watchdog/recovery safeguards before enabling output control.

## Persistence and recovery

The canonical `O6SC` version-1 record is exactly 64 bytes, big-endian, and stores 14 ordered values. The decoder rejects bad magic/version/length, unknown reserved bytes, time values that are not half-hour aligned, and conflicts. It only replaces the caller’s schedule **after complete validation**.

`ou_weekly_schedule_checkpoint` requires a positive authorization callback and the existing journal’s verified geometry, dedicated partitions, and write-enabled gates; the record goes through the power-failure-safe two-slot journal. These slots must be **independent SCHEDULE slots**, never borrowed from boot firmware, CONFIG, or EVENT partitions. If a staged commit is interrupted or corrupted, `ou_weekly_schedule_restore` can recover the previously valid record. No deployed flash write path calls these functions.

## Management interface

`GET /api/v1/schedule/schema` reports the seven-day, 48-slot/day, 30-minute format and marks actual schedule read/write, RTC, persistence, and power-control qualifications as **false**. There is no read endpoint exposing fictional active schedule values and no execute endpoint.

The existing Weekly Schedule page now includes all 14 on/off selectors with accessible labels, inline conflict validation, and an export-only JSON review. The file explicitly includes `applied:false`, `installable:false`, the seven daily values and `timezone:"device-time-not-qualified"`. It is **not an installable firmware/configuration package**. All power-operation buttons remain disabled.

## Verification and future integration gates

Host tests validate the daily/weekly recurrence planner, same-day conflict detection, 30-minute alignment, disabled entries, midnight and week rollover, exact event boundaries, record decoding, all-or-nothing state, journal qualification gates, simulated write failure and corrupted newest-slot fallback. HTTP tests verify capability reporting, read-only routes, and nonexistent execution. The Node DOM suite tests all seven days, selectors, conflicts, export content and navigation persistence. ARM7 freestanding compilation/linking and sanitizers are required.

Still outstanding: trusted device clock and time zone/DST semantics; safe behavior for clock jumps; alarm deduplication; persisted device schedule state; verified authenticated output-control driver; a safe shutdown warning/recovery process; integration and testing on recoverable H910P hardware. **Do not confuse schedule planning with active power automation.**
