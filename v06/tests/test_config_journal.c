#include "ou_config_journal.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define SLOT_BYTES 8192U

typedef struct {
    uint8_t slots[2][SLOT_BYTES];
    unsigned writes;
    unsigned erases;
    int fail_after_bytes;
    int fail_next_read;
} emulated_flash;

static void factory_blank(emulated_flash *m) {
    unsigned s, i;
    *m = (emulated_flash){0};
    m->fail_after_bytes = -1;
    for (s = 0; s < 2U; ++s)
        for (i = 0; i < SLOT_BYTES; ++i) m->slots[s][i] = 0xffU;
}
static int rom_read(void *context, unsigned slot, uint32_t offset,
                    uint8_t *out, size_t length) {
    emulated_flash *m = context;
    size_t i;
    if (slot > 1U || offset > SLOT_BYTES || length > SLOT_BYTES - offset) return -1;
    if (m->fail_next_read) { m->fail_next_read = 0; return -1; }
    for (i = 0U; i < length; ++i) out[i] = m->slots[slot][offset + i];
    return 0;
}
static int nor_program(void *context, unsigned slot, uint32_t offset,
                       const uint8_t *bytes, size_t length) {
    emulated_flash *m = context;
    size_t i;
    if (slot > 1U || offset > SLOT_BYTES || length > SLOT_BYTES - offset) return -1;
    m->writes++;
    for (i = 0U; i < length; ++i) {
        if (m->fail_after_bytes == 0) return -1;
        if ((uint8_t)(~m->slots[slot][offset + i]) & bytes[i]) return -1;
        m->slots[slot][offset + i] &= bytes[i];
        if (m->fail_after_bytes > 0) m->fail_after_bytes--;
    }
    return 0;
}
static int nor_erase(void *context, unsigned slot) {
    emulated_flash *m = context;
    size_t i;
    if (slot > 1U) return -1;
    m->erases++;
    for (i = 0U; i < SLOT_BYTES; ++i) m->slots[slot][i] = 0xffU;
    return 0;
}
static ou_journal_io backend(emulated_flash *m) {
    ou_journal_io io = {0};
    io.context = m;
    io.read = rom_read;
    io.program = nor_program;
    io.erase = nor_erase;
    io.slot_bytes = SLOT_BYTES;
    io.erase_block_bytes = SLOT_BYTES;
    io.geometry_verified = 1U; /* synthetic fixture ONLY */
    io.dedicated_slots_verified = 1U; /* synthetic fixture ONLY */
    io.writes_enabled = 1U;
    return io;
}
static void check_payload(ou_journal_io *io, const uint8_t *expected, size_t count,
                          uint32_t expected_generation) {
    uint8_t result[OU_JOURNAL_MAX_PAYLOAD];
    ou_journal_record meta;
    size_t len;
    size_t i;
    assert(ou_journal_load(io, result, sizeof(result), &len, &meta) == 0);
    assert(len == count && meta.valid && meta.generation == expected_generation);
    for (i = 0U; i < count; ++i) assert(result[i] == expected[i]);
}

static void test_power_failure_and_corruption(void) {
    emulated_flash memory;
    ou_journal_io io;
    ou_journal_record meta;
    const uint8_t first[] = {1U, 2U, 3U, 4U, 5U};
    const uint8_t next[] = {6U, 7U, 8U, 9U, 10U, 11U};
    factory_blank(&memory);
    io = backend(&memory);
    assert(ou_journal_find(&io, &meta) == 0 && !meta.valid);
    assert(ou_journal_commit(&io, first, sizeof(first), &meta) == 0);
    assert(meta.valid && meta.slot == 0U && meta.generation == 1U);
    check_payload(&io, first, sizeof(first), 1U);
    memory.fail_after_bytes = 30;
    assert(ou_journal_commit(&io, next, sizeof(next), &meta) == -1);
    memory.fail_after_bytes = -1;
    check_payload(&io, first, sizeof(first), 1U);  /* old slot survived */
    assert(ou_journal_commit(&io, next, sizeof(next), &meta) == 0);
    assert(meta.valid && meta.slot == 1U && meta.generation == 2U);
    check_payload(&io, next, sizeof(next), 2U);
    /* Bit corruption in the latest payload restores the last known-good. */
    memory.slots[1][OU_JOURNAL_HEADER_BYTES] ^= 1U;
    check_payload(&io, first, sizeof(first), 1U);
    memory.slots[0][OU_JOURNAL_HEADER_BYTES] ^= 1U;
    assert(ou_journal_find(&io, &meta) == 0 && !meta.valid);
}

static void test_capacity_and_backend_guards(void) {
    emulated_flash memory;
    ou_journal_io io;
    ou_journal_record meta;
    uint8_t large[OU_JOURNAL_MAX_PAYLOAD];
    size_t i;
    unsigned previous_writes;
    factory_blank(&memory);
    io = backend(&memory);
    for (i = 0; i < sizeof(large); ++i) large[i] = (uint8_t)i;
    io.geometry_verified = 0;
    assert(ou_journal_commit(&io, large, sizeof(large), &meta) == -1);
    io.geometry_verified = 1;
    io.dedicated_slots_verified = 0;
    assert(ou_journal_commit(&io, large, sizeof(large), &meta) == -1);
    io.dedicated_slots_verified = 1;
    io.writes_enabled = 0;
    assert(ou_journal_commit(&io, large, sizeof(large), &meta) == -1);
    previous_writes = memory.writes;
    assert(memory.writes == previous_writes);
    io.writes_enabled = 1;
    assert(ou_journal_commit(&io, large, sizeof(large), &meta) == 0);
    check_payload(&io, large, sizeof(large), 1U);
    assert(ou_journal_commit(&io, large, sizeof(large) + 1U, &meta) == -1);
    io.erase_block_bytes = 4097U;
    assert(ou_journal_find(&io, &meta) == -1);
    io.erase_block_bytes = SLOT_BYTES;
    memory.fail_next_read = 1;
    assert(ou_journal_find(&io, &meta) == -1);
}

int main(void) {
    test_power_failure_and_corruption();
    test_capacity_and_backend_guards();
    puts("Two-slot journal: CRC, NOR write and power-loss recovery PASS");
    return 0;
}
