#include "ou_provider.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    ou_driver_report sample = {0};
    ou_snapshot snapshot;
    ou_reply reply;
    char body[OU_JSON_CAPACITY];
    sample.version = OU_REPORT_VERSION;
    sample.verified = 1;
    sample.sequence = 7;
    sample.received_monotonic_ms = 123000;
    sample.uptime_seconds = 700;
    sample.battery_percent = 94;
    sample.load_percent = 52;
    sample.input_decivolts = 1200;
    sample.output_decivolts = 1190;
    sample.runtime_seconds = 1320;
    sample.utility_present = 1;
    sample.battery_present = 1;
    sample.segment_1_on = 1;
    sample.segment_2_on = 1;
    assert(ou_provider_accept(&sample, 124000, &snapshot) == 0);
    assert(snapshot.origin == OU_ORIGIN_NATIVE);
    assert(ou_dispatch("GET", "/api/v1/ups/status", &snapshot, body,
                       sizeof(body), &reply) == 0);
    assert(reply.status == 200);
    assert(strstr(body, "\"source\":\"native\""));
    assert(strstr(body, "\"simulated\":false"));
    assert(strstr(body, "\"batteryPercent\":94"));
    assert(ou_dispatch("GET", "/api/v1/capabilities", &snapshot, body,
                       sizeof(body), &reply) == 0);
    assert(strstr(body, "\"physicalTelemetry\":true"));
    assert(ou_dispatch("GET", "/api/v1/events", &snapshot, body,
                       sizeof(body), &reply) == 0);
    assert(reply.status == 503);
    assert(strstr(body, "EVENT_DRIVER_UNAVAILABLE"));
    sample.battery_percent = 101;
    assert(ou_provider_accept(&sample, 124000, &snapshot) == -1);
    assert(snapshot.origin == OU_ORIGIN_UNAVAILABLE);
    assert(ou_dispatch("GET", "/api/v1/ups/status", &snapshot, body,
                       sizeof(body), &reply) == 0);
    assert(reply.status == 503);
    assert(strstr(body, "TELEMETRY_UNAVAILABLE"));
    sample.battery_percent = 94;
    sample.verified = 0;
    assert(ou_provider_accept(&sample, 124000, &snapshot) == -1);
    sample.verified = 1;
    assert(ou_provider_accept(&sample, 128001, &snapshot) == -1);
    assert(ou_provider_accept(&sample, 128000, &snapshot) == 0);
    sample.version = 0;
    assert(ou_provider_accept(&sample, 124000, &snapshot) == -1);
    sample.version = 1;
    sample.sequence = 0;
    assert(ou_provider_accept(&sample, 124000, &snapshot) == -1);
    sample.sequence = 7;
    sample.output_decivolts = 65535;
    assert(ou_provider_accept(&sample, 124000, &snapshot) == -1);
    assert(ou_provider_accept(NULL, 124000, &snapshot) == -1);
    assert(ou_provider_accept(&sample, 124000, NULL) == -1);
    puts("PASS: validated native provider, stale-data rejection and JSON safety");
    return 0;
}
