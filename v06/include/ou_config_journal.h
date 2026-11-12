#ifndef OU_CONFIG_JOURNAL_H
#define OU_CONFIG_JOURNAL_H

#include <stddef.h>
#include <stdint.h>

/* Portable two-slot NOR-friendly settings journal. NO flash device driver.
 * The real H910P flash regions and erase geometry are NOT verified. */
#define OU_JOURNAL_MAX_PAYLOAD 4096U
#define OU_JOURNAL_HEADER_BYTES 24U
#define OU_JOURNAL_VERSION 1U

typedef int (*ou_journal_read_fn)(void *ctx, unsigned slot, uint32_t offset,
                                  uint8_t *output, size_t len);
typedef int (*ou_journal_program_fn)(void *ctx, unsigned slot, uint32_t offset,
                                     const uint8_t *input, size_t len);
typedef int (*ou_journal_erase_fn)(void *ctx, unsigned slot);

typedef struct {
    void *context;
    ou_journal_read_fn read;
    ou_journal_program_fn program;
    ou_journal_erase_fn erase;
    uint32_t slot_bytes;
    uint32_t erase_block_bytes;
    uint8_t geometry_verified;
    uint8_t dedicated_slots_verified;
    uint8_t writes_enabled;
} ou_journal_io;

typedef struct {
    uint32_t generation;
    uint32_t length;
    uint32_t crc32;
    uint8_t slot;
    uint8_t valid;
} ou_journal_record;

/* Reads only, verifies magic/version/commit marker and streaming CRC.
 * Return 0 with meta.valid=0 if neither slot is usable; -1 on I/O failure. */
int ou_journal_find(const ou_journal_io *io, ou_journal_record *meta);

/* Read selected valid payload, verify CRC again; never return partial data.
 * out_length and meta are reset on errors. May read firmware-independent
 * schema bytes but does not attempt to parse the contents as JSON. */
int ou_journal_load(const ou_journal_io *io, uint8_t *output,
                    size_t capacity, size_t *out_length, ou_journal_record *meta);

/* Writes only when physical geometry + dedicated partition are confirmed and
 * an explicit write-enabled flag is set. Stages inactive slot; verifies
 * programmed payload + header, and writes 32-bit commit marker LAST.
 * Preserves old slot on any failure; backend must honor NOR 1->0 semantics. */
int ou_journal_commit(const ou_journal_io *io,
                      const uint8_t *payload, size_t length,
                      ou_journal_record *meta);

#endif
