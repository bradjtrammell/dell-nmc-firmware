#include "ou_telemetry_guard.h"
#include <assert.h>
#include <stdio.h>

static ou_driver_report sample(uint32_t sequence, uint32_t receive_ms,
                               uint32_t uptime_s) {
    ou_driver_report r = {0};
    r.version = OU_REPORT_VERSION;
    r.verified = 1U; /* synthetic, validated-only fixture */
    r.sequence = sequence;
    r.received_monotonic_ms = receive_ms;
    r.uptime_seconds = uptime_s;
    r.runtime_seconds = 1200U;
    r.battery_percent = 87U;
    r.load_percent = 23U;
    r.input_decivolts = 1200U;
    r.output_decivolts = 1200U;
    r.utility_present = 1U;
    r.battery_present = 1U;
    r.segment_1_on = 1U;
    r.segment_2_on = 1U;
    return r;
}

static void assert_unavailable(ou_telemetry_guard *guard, uint32_t now) {
    ou_snapshot out = ou_simulated_snapshot();
    assert(ou_telemetry_guard_read(guard, now, &out) == -1);
    assert(out.origin == OU_ORIGIN_UNAVAILABLE && out.sample_ready == 0U);
}

static void test_valid_freshness_and_timeout(void) {
    ou_telemetry_guard gate;
    ou_snapshot out;
    ou_driver_report a = sample(1U, 1000U, 2U);
    ou_telemetry_guard_reset(&gate);
    assert_unavailable(&gate, 1000U);
    assert(ou_telemetry_guard_accept(&gate, &a, 1001U) == 0);
    assert(ou_telemetry_guard_read(&gate, 6000U, &out) == 0);
    assert(out.origin == OU_ORIGIN_NATIVE && out.sample_ready == 1U);
    assert(out.battery_percent == 87U);
    assert_unavailable(&gate, 6001U);
    assert(gate.live == 0U && gate.source_seen == 1U);
    assert(gate.accepted_count == 1U && gate.rejected_count == 0U);
}

static void test_duplicates_rewind_and_recovery(void) {
    ou_telemetry_guard gate;
    ou_driver_report a = sample(11U, 1000U, 10U);
    ou_driver_report duplicate = sample(11U, 1100U, 10U);
    ou_driver_report older = sample(10U, 1200U, 10U);
    ou_driver_report new_sample = sample(12U, 1300U, 10U);
    ou_telemetry_guard_reset(&gate);
    assert(ou_telemetry_guard_accept(&gate, &a, 1000U) == 0);
    assert(ou_telemetry_guard_accept(&gate, &duplicate, 1100U) == -1);
    assert_unavailable(&gate, 1100U);
    assert(ou_telemetry_guard_accept(&gate, &older, 1200U) == -1);
    assert_unavailable(&gate, 1200U);
    assert(ou_telemetry_guard_accept(&gate, &new_sample, 1300U) == 0);
    assert(gate.accepted_count == 2U && gate.rejected_count == 2U);
    assert(ou_telemetry_guard_accept(&gate, &new_sample, 1301U) == -1);
    assert_unavailable(&gate, 1301U);
}

static void test_invalid_and_future_frames(void) {
    ou_telemetry_guard gate;
    ou_driver_report a = sample(1U, 800U, 1U);
    ou_driver_report b = sample(2U, 900U, 2U);
    ou_telemetry_guard_reset(&gate);
    assert(ou_telemetry_guard_accept(&gate, &a, 800U) == 0);
    b.verified = 0U;
    assert(ou_telemetry_guard_accept(&gate, &b, 900U) == -1);
    assert_unavailable(&gate, 900U);
    b.verified = 1U;
    b.battery_percent = 101U;
    assert(ou_telemetry_guard_accept(&gate, &b, 900U) == -1);
    b.battery_percent = 87U;
    b.received_monotonic_ms = 1000U;
    assert(ou_telemetry_guard_accept(&gate, &b, 999U) == -1);
    assert(ou_telemetry_guard_accept(&gate, &b, 1000U) == 0);
    assert(gate.rejected_count == 3U);
    assert(ou_telemetry_guard_accept(&gate, NULL, 1001U) == -1);
    assert_unavailable(&gate, 1001U);
}

static void test_clock_and_uptime_rewind(void) {
    ou_telemetry_guard gate;
    ou_driver_report a = sample(1U, 500U, 60U);
    ou_driver_report b = sample(2U, 400U, 61U);
    ou_telemetry_guard_reset(&gate);
    assert(ou_telemetry_guard_accept(&gate, &a, 500U) == 0);
    assert(ou_telemetry_guard_accept(&gate, &b, 600U) == -1);
    b.received_monotonic_ms = 601U;
    b.uptime_seconds = 1U;
    assert(ou_telemetry_guard_accept(&gate, &b, 602U) == -1);
    /* A transport owner may reset ONLY after a verified new session. */
    ou_telemetry_guard_reset(&gate);
    assert(ou_telemetry_guard_accept(&gate, &b, 602U) == 0);
    assert(gate.accepted_count == 1U && gate.rejected_count == 0U);
}

static void test_unsigned_wrap(void) {
    ou_telemetry_guard gate;
    ou_snapshot out;
    ou_driver_report a = sample(UINT32_MAX, UINT32_MAX - 2U, 99U);
    ou_driver_report b = sample(1U, 2U, 100U);
    ou_telemetry_guard_reset(&gate);
    assert(ou_telemetry_guard_accept(&gate, &a, UINT32_MAX - 1U) == 0);
    assert(ou_telemetry_guard_accept(&gate, &b, 3U) == 0);
    assert(ou_telemetry_guard_read(&gate, 3U, &out) == 0);
    assert(out.sequence == 1U);
    assert_unavailable(&gate, 5003U);
}

static void test_null_and_counter_saturation(void) {
    ou_telemetry_guard gate;
    ou_driver_report r = sample(1U, 1U, 1U);
    ou_telemetry_guard_reset(NULL);
    assert(ou_telemetry_guard_accept(NULL, &r, 1U) == -1);
    assert(ou_telemetry_guard_read(NULL, 1U, NULL) == -1);
    gate = (ou_telemetry_guard){0};
    gate.rejected_count = UINT32_MAX;
    gate.accepted_count = UINT32_MAX;
    assert(ou_telemetry_guard_accept(&gate, NULL, 1U) == -1);
    assert(gate.rejected_count == UINT32_MAX);
    assert(ou_telemetry_guard_accept(&gate, &r, 1U) == 0);
    assert(gate.accepted_count == UINT32_MAX);
}

int main(void) {
    test_valid_freshness_and_timeout();
    test_duplicates_rewind_and_recovery();
    test_invalid_and_future_frames();
    test_clock_and_uptime_rewind();
    test_unsigned_wrap();
    test_null_and_counter_saturation();
    puts("Native telemetry guard: fresh/invalid/replay/wrap/fail-closed PASS");
    return 0;
}
