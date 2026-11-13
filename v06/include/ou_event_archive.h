#ifndef OU_EVENT_ARCHIVE_H
#define OU_EVENT_ARCHIVE_H
#include "ou_events.h"
#include "ou_config_journal.h"

/* Portable, endian-stable full-ring checkpoint. Not an on-card flash driver.
 * Requires a SEPARATE qualified two-slot NOR partition exclusively for event
 * data, never the existing configuration or bootloader sectors. */
#define OU_EVENT_ARCHIVE_VERSION 1U
#define OU_EVENT_ARCHIVE_HEADER 24U
#define OU_EVENT_ARCHIVE_ROW_BYTES 13U
#define OU_EVENT_ARCHIVE_BYTES (OU_EVENT_ARCHIVE_HEADER + OU_EVENTS_CAPACITY * OU_EVENT_ARCHIVE_ROW_BYTES)

/* Encode/decode use the entire fixed-size canonical payload, so the
 * underlying two-slot journal can validate CRC before any state changes. */
int ou_event_archive_encode(const ou_event_log *log, uint8_t *bytes,
                            size_t capacity);
int ou_event_archive_decode(ou_event_log *log, const uint8_t *bytes,
                            size_t length);

/* Restore to a new boot epoch: older event timestamps retain the prior
 * bootGeneration, and new records get the next generation. Do not restore
 * prior online/utility states or active sessions after reboot. -1 on any
 * invalid/corrupt journal, leaving the original log unchanged. */
int ou_event_archive_restore(ou_event_log *log, const ou_journal_io *dedicated_io);

/* User-independent explicit checkpoint; NEVER called automatically for each
 * event (flash wear). Physical adapter must establish dedicated slots,
 * real erase geometry, independent recovery, and explicit writes_enabled.
 * Two-slot journal commits the marker only after full readback verification. */
int ou_event_archive_checkpoint(const ou_event_log *log,
                                const ou_journal_io *dedicated_io,
                                ou_journal_record *record);
#endif
