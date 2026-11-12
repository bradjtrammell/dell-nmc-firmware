#ifndef OU_VLAN_RECORD_H
#define OU_VLAN_RECORD_H
#include "ou_vlan.h"
#include "ou_config_journal.h"
#define OU_VLAN_RECORD_VERSION 1U
#define OU_VLAN_RECORD_HEADER 24U
#define OU_VLAN_RECORD_INTERFACE 59U
#define OU_VLAN_RECORD_MAX_SIZE (OU_VLAN_RECORD_HEADER + OU_VLAN_MAX_INTERFACES * OU_VLAN_RECORD_INTERFACE)

/* Deterministic big-endian versioned image with CRC32, max 3800 bytes.
 * All scratch storage belongs to the caller; no dynamic allocation. */
int ou_vlan_record_encode(const ou_vlan_table *policy, uint8_t *buffer,
                          size_t capacity, size_t *written);
int ou_vlan_record_decode(ou_vlan_table *policy, const uint8_t *buffer,
                          size_t length);
/* These reuse the NOR two-slot journal and its physical flash
 * geometry/partition/write-enable gates. No physical flash driver. */
int ou_vlan_journal_save(const ou_journal_io *io, const ou_vlan_table *policy,
                         uint8_t *scratch, size_t capacity,
                         ou_journal_record *meta);
int ou_vlan_journal_load(const ou_journal_io *io, ou_vlan_table *policy,
                         uint8_t *scratch, size_t capacity,
                         ou_journal_record *meta);
#endif
