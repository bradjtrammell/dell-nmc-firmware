#include "openh910p.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check(const char *method, const char *path,
                  int expected_status, const char *token) {
    ou_snapshot sample = ou_simulated_snapshot();
    ou_reply result;
    char body[OU_JSON_CAPACITY];
    assert(ou_dispatch(method, path, &sample, body, sizeof(body), &result) == 0);
    assert(result.status == expected_status);
    assert(result.body_length == strlen(body));
    assert(result.body_length < sizeof(body));
    assert(strstr(body, token));
    assert(strstr(body, "\"simulated\":true"));
}

int main(void) {
    char small[16];
    ou_reply reply;
    ou_snapshot state = ou_simulated_snapshot();
    char json[OU_JSON_CAPACITY];
    check("GET", "/api/v1/health", 200, "\"status\":\"ok\"");
    check("GET", "/api/v1/device", 200, OU_FIRMWARE_VERSION);
    check("GET", "/api/v1/capabilities", 200, "\"configurationItems\":302");
    check("GET", "/api/v1/ups/status", 200, "\"batteryPercent\":97");
    check("GET", "/api/v1/ups/measurements", 200, "\"valueDecivolts\":1190");
    check("GET", "/api/v1/ups/outlets", 200, "\"controlEnabled\":false");
    check("GET", "/api/v1/events", 200, "Simulator started");
    check("GET", "/api/v1/configuration/fields", 200, "\"total\":302");
    check("GET", "/api/v1/configuration/fields/0", 200, "System.Network.HostName");
    check("GET", "/api/v1/configuration/fields/18", 200, "\"page\":18");
    check("GET", "/api/v1/configuration/fields/19", 404, "PAGE_NOT_FOUND");
    check("HEAD", "/api/v1/device", 200, "\"name\":\"OpenH910P\"");
    check("GET", "/get_object.xml", 404, "NOT_FOUND");
    check("GET", "/api/v1/nonexistent", 404, "NOT_FOUND");
    check("POST", "/api/v1/ups/outlets", 405, "READ_ONLY");
    check("PUT", "/api/v1/network", 405, "READ_ONLY");
    check("DELETE", "/api/v1/events", 405, "READ_ONLY");
    assert(ou_dispatch("GET", "/api/v1/device", &state, small, sizeof(small), &reply) == -1);
    assert(ou_dispatch(NULL, "/api/v1/device", &state, json, sizeof(json), &reply) == -1);
    state.output_load_percent = 62;
    assert(ou_dispatch("GET", "/api/v1/ups/status", &state, json, sizeof(json), &reply) == 0);
    assert(strstr(json, "\"loadPercent\":62"));
    puts("PASS: 20 native core and API safety checks");
    return 0;
}
