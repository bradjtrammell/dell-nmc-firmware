#include "ou_provider.h"

int ou_provider_accept(const ou_driver_report *report,
                       uint32_t now_monotonic_ms, ou_snapshot *output) {
    uint32_t age;
    if (!output) return -1;
    *output = ou_unavailable_snapshot();
    if (!report || report->version != OU_REPORT_VERSION || !report->verified)
        return -1;
    /* On a wrap-safe monotonically increasing millisecond clock, unsigned
     * subtraction yields age modulo 2^32; distant future data is rejected. */
    age = now_monotonic_ms - report->received_monotonic_ms;
    if (age > OU_SAMPLE_MAX_AGE_MS || report->sequence == 0U ||
        report->battery_percent > 100U || report->load_percent > 200U ||
        report->input_decivolts > 3000U || report->output_decivolts > 3000U ||
        report->runtime_seconds > 604800U ||
        report->utility_present > 1U || report->battery_present > 1U ||
        report->segment_1_on > 1U || report->segment_2_on > 1U)
        return -1;
    output->sequence = report->sequence;
    output->uptime_seconds = report->uptime_seconds;
    output->battery_percent = report->battery_percent;
    output->estimated_runtime_seconds = report->runtime_seconds;
    output->input_voltage_decivolts = report->input_decivolts;
    output->output_voltage_decivolts = report->output_decivolts;
    output->output_load_percent = report->load_percent;
    output->ac_present = report->utility_present;
    output->battery_present = report->battery_present;
    output->segment_1_on = report->segment_1_on;
    output->segment_2_on = report->segment_2_on;
    output->origin = OU_ORIGIN_NATIVE;
    output->sample_ready = 1U;
    return 0;
}
