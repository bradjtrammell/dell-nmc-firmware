#include "openh910p.h"

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
    int error;
} json_writer;

static int same(const char *left, const char *right) {
    if (!left || !right) return 0;
    while (*left && *right && *left == *right) { left++; right++; }
    return *left == '\0' && *right == '\0';
}

static void put_char(json_writer *writer, char ch) {
    if (writer->error) return;
    if (writer->length + 1U >= writer->capacity) { writer->error = 1; return; }
    writer->data[writer->length++] = ch;
}

static void put_str(json_writer *writer, const char *value) {
    if (!value) { writer->error = 1; return; }
    while (*value) put_char(writer, *value++);
}

static void put_uint(json_writer *writer, uint32_t value) {
    char temporary[10];
    unsigned index = 0;
    do {
        temporary[index++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value && index < sizeof(temporary));
    while (index) put_char(writer, temporary[--index]);
}

static void put_bool(json_writer *writer, uint8_t value) {
    put_str(writer, value ? "true" : "false");
}

/* Kept in the freestanding core for eventual hardware-provided strings. */
static void put_quoted(json_writer *writer, const char *value) {
    static const char digits[] = "0123456789abcdef";
    if (!value) { writer->error = 1; return; }
    put_char(writer, '"');
    for (; *value; value++) {
        unsigned char c = (unsigned char)*value;
        if (c == '"' || c == '\\') {
            put_char(writer, '\\'); put_char(writer, (char)c);
        } else if (c < 0x20U) {
            put_str(writer, "\\u00");
            put_char(writer, digits[c >> 4]);
            put_char(writer, digits[c & 15U]);
        } else {
            put_char(writer, (char)c);
        }
    }
    put_char(writer, '"');
}

static void property_uint(json_writer *writer, const char *key,
                          uint32_t value, int comma) {
    if (comma) put_char(writer, ',');
    put_quoted(writer, key); put_char(writer, ':'); put_uint(writer, value);
}

static void property_bool(json_writer *writer, const char *key,
                          uint8_t value, int comma) {
    if (comma) put_char(writer, ',');
    put_quoted(writer, key); put_char(writer, ':'); put_bool(writer, value);
}

static void property_string(json_writer *writer, const char *key,
                            const char *value, int comma) {
    if (comma) put_char(writer, ',');
    put_quoted(writer, key); put_char(writer, ':'); put_quoted(writer, value);
}

static void common(json_writer *w, const char *resource, uint32_t seq) {
    put_char(w, '{');
    property_uint(w, "schemaVersion", OU_API_VERSION, 0);
    property_string(w, "resource", resource, 1);
    property_string(w, "source", "simulator", 1);
    property_bool(w, "simulated", 1, 1);
    property_uint(w, "sequence", seq, 1);
}

static void one_outlet(json_writer *w, uint32_t id,
                       const char *name, uint8_t on, int comma) {
    if (comma) put_char(w, ',');
    put_char(w, '{');
    property_uint(w, "id", id, 0);
    property_string(w, "label", name, 1);
    property_string(w, "state", on ? "on" : "off", 1);
    property_bool(w, "switchable", id != 1U, 1);
    property_bool(w, "controlEnabled", 0, 1);
    put_char(w, '}');
}

ou_snapshot ou_simulated_snapshot(void) {
    ou_snapshot state;
    state.sequence = 1U;
    state.uptime_seconds = 3486U;
    state.battery_percent = 97U;
    state.estimated_runtime_seconds = 1560U;
    state.input_voltage_decivolts = 1190U;
    state.output_voltage_decivolts = 1200U;
    state.output_load_percent = 57U;
    state.ac_present = 1U;
    state.battery_present = 1U;
    state.segment_1_on = 1U;
    state.segment_2_on = 1U;
    return state;
}

int ou_dispatch(const char *method, const char *path,
                const ou_snapshot *snapshot, char *body,
                size_t capacity, ou_reply *reply) {
    json_writer w;
    if (!method || !path || !snapshot || !body || !reply || capacity < 2U)
        return -1;
    w.data = body;
    w.capacity = capacity;
    w.length = 0U;
    w.error = 0;
    reply->status = 200;
    reply->content_type = "application/json; charset=utf-8";
    reply->body_length = 0U;

    if (!same(method, "GET") && !same(method, "HEAD")) {
        reply->status = 405;
        common(&w, "error", snapshot->sequence);
        property_string(&w, "code", "READ_ONLY", 1);
        property_string(&w, "message", "Firmware development simulator refuses all mutations", 1);
    } else if (same(path, "/api/v1/health")) {
        common(&w, "health", snapshot->sequence);
        property_string(&w, "status", "ok", 1);
        property_string(&w, "hardware", "not-connected", 1);
        property_uint(&w, "uptimeSeconds", snapshot->uptime_seconds, 1);
    } else if (same(path, "/api/v1/device")) {
        common(&w, "device", snapshot->sequence);
        property_string(&w, "name", "OpenH910P", 1);
        property_string(&w, "firmwareVersion", OU_FIRMWARE_VERSION, 1);
        property_string(&w, "platform", "H910P ARM7TDMI target", 1);
        property_string(&w, "hardwareConnection", "none", 1);
        property_string(&w, "manufacturer", "Independent development project", 1);
    } else if (same(path, "/api/v1/capabilities")) {
        common(&w, "capabilities", snapshot->sequence);
        property_bool(&w, "upsTelemetry", 1, 1);
        property_bool(&w, "physicalTelemetry", 0, 1);
        property_bool(&w, "configurationWritable", 0, 1);
        property_bool(&w, "outletControl", 0, 1);
        property_bool(&w, "firmwareInstall", 0, 1);
        property_bool(&w, "smtpDelivery", 0, 1);
        property_string(&w, "transport", "native-json-c", 1);
        put_str(&w, ",\"legacyOemInventory\":{\"configurationGroups\":24,\"configurationItems\":302,\"objectNames\":420}");
    } else if (same(path, "/api/v1/ups/status")) {
        common(&w, "ups.status", snapshot->sequence);
        property_string(&w, "powerSource", snapshot->ac_present ? "utility" : "battery", 1);
        property_bool(&w, "batteryPresent", snapshot->battery_present, 1);
        property_uint(&w, "batteryPercent", snapshot->battery_percent, 1);
        property_uint(&w, "estimatedRuntimeSeconds", snapshot->estimated_runtime_seconds, 1);
        property_uint(&w, "loadPercent", snapshot->output_load_percent, 1);
    } else if (same(path, "/api/v1/ups/measurements")) {
        common(&w, "ups.measurements", snapshot->sequence);
        put_str(&w, ",\"inputVoltage\":{");
        property_uint(&w, "valueDecivolts", snapshot->input_voltage_decivolts, 0);
        property_string(&w, "unit", "dV", 1);
        put_str(&w, "},\"outputVoltage\":{");
        property_uint(&w, "valueDecivolts", snapshot->output_voltage_decivolts, 0);
        property_string(&w, "unit", "dV", 1);
        put_char(&w, '}');
    } else if (same(path, "/api/v1/ups/outlets")) {
        common(&w, "ups.outlets", snapshot->sequence);
        put_str(&w, ",\"outlets\":[");
        one_outlet(&w, 1U, "UPS output", 1U, 0);
        one_outlet(&w, 2U, "Load segment 1", snapshot->segment_1_on, 1);
        one_outlet(&w, 3U, "Load segment 2", snapshot->segment_2_on, 1);
        put_char(&w, ']');
    } else if (same(path, "/api/v1/events")) {
        common(&w, "events", snapshot->sequence);
        put_str(&w, ",\"events\":[");
        put_str(&w, "{\"id\":1,\"severity\":\"info\",\"message\":\"Simulator started; no UPS commands are enabled\"},");
        put_str(&w, "{\"id\":2,\"severity\":\"info\",\"message\":\"Native JSON API available in development mode\"}]");
    } else {
        reply->status = 404;
        common(&w, "error", snapshot->sequence);
        property_string(&w, "code", "NOT_FOUND", 1);
        property_string(&w, "message", "Unknown API resource", 1);
    }
    put_char(&w, '}');
    if (w.error) { body[0] = '\0'; return -1; }
    body[w.length] = '\0';
    reply->body_length = w.length;
    return 0;
}
