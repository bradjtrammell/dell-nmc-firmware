#include "ou_config_journal.h"

#define MAGIC UINT32_C(0x4f553643) /* "OU6C" */
#define COMMITTED UINT32_C(0xa55ac33c)
#define ERASED UINT32_MAX
#define CHUNK 64U

static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static void write_be32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}
static uint32_t crc_step(uint32_t crc, const uint8_t *bytes, size_t n) {
    size_t i;
    unsigned bit;
    for (i = 0; i < n; ++i) {
        crc ^= bytes[i];
        for (bit = 0; bit < 8U; ++bit)
            crc = (crc >> 1U) ^ ((crc & 1U) ? UINT32_C(0xedb88320) : 0U);
    }
    return crc;
}
static uint32_t crc_all(const uint8_t *bytes, size_t n) {
    return crc_step(UINT32_MAX, bytes, n) ^ UINT32_MAX;
}
static int good_read_io(const ou_journal_io *io) {
    uint32_t block;
    if (!io || !io->read || io->geometry_verified != 1U ||
        io->dedicated_slots_verified != 1U) return 0;
    block = io->erase_block_bytes;
    if (block < 4096U || block > 262144U || (block & (block - 1U)) != 0U)
        return 0;
    if (io->slot_bytes < OU_JOURNAL_HEADER_BYTES + OU_JOURNAL_MAX_PAYLOAD ||
        (io->slot_bytes & (block - 1U)) != 0U) return 0;
    return 1;
}

/* Return 1 valid, 0 invalid/uncommitted, -1 I/O failure. */
static int inspect_slot(const ou_journal_io *io, unsigned slot,
                        ou_journal_record *result) {
    uint8_t header[OU_JOURNAL_HEADER_BYTES];
    uint8_t buffer[CHUNK];
    uint32_t generation, len, crc, pos, running;
    size_t step;
    *result = (ou_journal_record){0};
    if (io->read(io->context, slot, 0U, header, sizeof(header)) != 0)
        return -1;
    if (read_be32(header) != MAGIC || read_be32(header + 4U) != OU_JOURNAL_VERSION ||
        read_be32(header + 20U) != COMMITTED) return 0;
    generation = read_be32(header + 8U);
    len = read_be32(header + 12U);
    crc = read_be32(header + 16U);
    if (!generation || len == 0U || len > OU_JOURNAL_MAX_PAYLOAD ||
        len > io->slot_bytes - OU_JOURNAL_HEADER_BYTES) return 0;
    running = UINT32_MAX;
    for (pos = 0U; pos < len; pos += (uint32_t)step) {
        step = len - pos < CHUNK ? len - pos : CHUNK;
        if (io->read(io->context, slot, OU_JOURNAL_HEADER_BYTES + pos,
                     buffer, step) != 0) return -1;
        running = crc_step(running, buffer, step);
    }
    if ((running ^ UINT32_MAX) != crc) return 0;
    result->valid = 1U;
    result->slot = (uint8_t)slot;
    result->generation = generation;
    result->length = len;
    result->crc32 = crc;
    return 1;
}

int ou_journal_find(const ou_journal_io *io, ou_journal_record *meta) {
    ou_journal_record a, b;
    int ca, cb;
    if (!meta) return -1;
    *meta = (ou_journal_record){0};
    if (!good_read_io(io)) return -1;
    ca = inspect_slot(io, 0U, &a);
    cb = inspect_slot(io, 1U, &b);
    if (ca < 0 || cb < 0) return -1;
    if (ca == 0 && cb == 0) return 0;
    if (ca == 1 && (cb == 0 || (uint32_t)(a.generation - b.generation) < UINT32_C(0x80000000)))
        *meta = a;
    else *meta = b;
    return 0;
}

int ou_journal_load(const ou_journal_io *io, uint8_t *output,
                    size_t capacity, size_t *out_length, ou_journal_record *meta) {
    ou_journal_record current;
    size_t i;
    if (meta) *meta = (ou_journal_record){0};
    if (out_length) *out_length = 0U;
    if (!output || !out_length || !meta || capacity > OU_JOURNAL_MAX_PAYLOAD)
        return -1;
    for (i = 0U; i < capacity; ++i) output[i] = 0U;
    if (ou_journal_find(io, &current) != 0 || !current.valid || current.length > capacity)
        return -1;
    if (io->read(io->context, current.slot, OU_JOURNAL_HEADER_BYTES,
                 output, current.length) != 0 || crc_all(output, current.length) != current.crc32) {
        for (i = 0U; i < capacity; ++i) output[i] = 0U;
        return -1;
    }
    *out_length = current.length;
    *meta = current;
    return 0;
}

int ou_journal_commit(const ou_journal_io *io,
                      const uint8_t *payload, size_t length,
                      ou_journal_record *meta) {
    ou_journal_record previous, checked;
    uint8_t header[OU_JOURNAL_HEADER_BYTES];
    uint8_t buffer[CHUNK];
    uint8_t marker[4];
    uint32_t generation, crc, pos;
    unsigned target;
    size_t step, i;
    if (meta) *meta = (ou_journal_record){0};
    if (!meta || !good_read_io(io) || !io->program || !io->erase ||
        io->writes_enabled != 1U || !payload || !length ||
        length > OU_JOURNAL_MAX_PAYLOAD) return -1;
    if (ou_journal_find(io, &previous) != 0) return -1;
    target = previous.valid ? (unsigned)(previous.slot ^ 1U) : 0U;
    generation = previous.valid ? previous.generation + 1U : 1U;
    if (!generation) generation = 1U;
    crc = crc_all(payload, length);
    for (i = 0U; i < sizeof(header); ++i) header[i] = 0xffU;
    write_be32(header, MAGIC);
    write_be32(header + 4U, OU_JOURNAL_VERSION);
    write_be32(header + 8U, generation);
    write_be32(header + 12U, (uint32_t)length);
    write_be32(header + 16U, crc);
    if (io->erase(io->context, target) != 0) return -1;
    if (io->program(io->context, target, 0U, header, sizeof(header)) != 0)
        return -1;
    if (io->program(io->context, target, OU_JOURNAL_HEADER_BYTES,
                    payload, length) != 0) return -1;
    /* Verify entire staged header and payload before writing commit marker;
     * avoid writing any IO on verification failure. */
    if (io->read(io->context, target, 0U, buffer, OU_JOURNAL_HEADER_BYTES) != 0)
        return -1;
    for (i = 0U; i < 20U; ++i) if (buffer[i] != header[i]) return -1;
    for (i = 20U; i < OU_JOURNAL_HEADER_BYTES; ++i) if (buffer[i] != 0xffU) return -1;
    for (pos = 0U; pos < length; pos += (uint32_t)step) {
        step = length - pos < CHUNK ? length - pos : CHUNK;
        if (io->read(io->context, target, OU_JOURNAL_HEADER_BYTES + pos, buffer, step) != 0)
            return -1;
        for (i = 0U; i < step; ++i) if (buffer[i] != payload[pos + i]) return -1;
    }
    write_be32(marker, COMMITTED);
    if (io->program(io->context, target, 20U, marker, sizeof(marker)) != 0)
        return -1;
    if (inspect_slot(io, target, &checked) != 1 ||
        checked.generation != generation || checked.length != length ||
        checked.crc32 != crc) return -1;
    *meta = checked;
    return 0;
}
