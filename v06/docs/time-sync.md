# H910P independent native network time synchronization

The native firmware now implements a bounded, freestanding **NTPv4 time-source validator** and UTC interpolation model, integrated with native service status and the Time management destination. This is **not** an activated physical RTC or internet-accessible NTP listener. The installed H910P has no qualified network source, trusted real-time clock or time-zone database.

## Wire request and validation

`src/time_sync.c` and `include/ou_time_sync.h` generate a 48-byte NTPv4 client request, using a mandatory hardware/OS-qualified secure nonce callback for the 64-bit transmit field. No predictable default seed, no assumed wall-clock timestamp, and no simultaneous outstanding request are permitted.

Response acceptance requires the **separately qualified isolated LAN source**, a verified source address, a trusted monotonic clock, a predefined NTP era (0 or 1) from an independent policy, and a matching originate nonce. It rejects missing/late responses, bad lengths, unsynchronized leap indicator, wrong protocol mode or version, kiss-of-death/invalid stratum, zero server timestamps, non-monotonic server receive/transmit times, excessive root dispersion, and implausible UTC dates. Bounded response RTT is at most 5,000 ms.

The recovered server transmit instant is converted from RFC 5905/NTP 1900-epoch seconds to UTC Unix seconds. NTP era is **explicit**, not guessed from a rebooted clock: this avoids the February 2036 era ambiguity. The model accounts for half the observed response delay for bootstrap interpolation, reports root dispersion plus RTT uncertainty and estimates ongoing UTC from the monotonic timer. A previously accepted time is withdrawn from the public API when older than one hour. All status values are explicitly UTC and never use an invented local time zone, DST state, or "current date" supplied by the browser.

**Security boundary:** Plain NTP is not cryptographically authenticated. This model is intended only for an independently configured isolated LAN source with verified source ACL, nonce generation and monotonic clock. The production architecture still requires NTS or another authenticated network time mechanism before using untrusted network time for privileged or safety-relevant scheduling. The model never writes an RTC or applies time to the firmware scheduler.

## Native API and UI

`GET /api/v1/time/capabilities` declares the NTP parser, response timing/age policy, and false qualifications for source configuration, NTS, entropy, ACL, hardware RTC, local time zone and DST rules. `GET /api/v1/time/status` provides UTC Unix seconds/milliseconds and bounded RTT/uncertainty from an accepted, unexpired source only. Otherwise the native and simulator APIs return **503 TIME_NOT_SYNCHRONIZED**. Both endpoints reject HTTP mutations.

The Time page displays synchronization health and allows a local, validated **NTP server, IANA-style time zone and daylight-saving policy review export**. The export is explicitly non-installable and does not set the card clock, configure a real upstream server or change alarm execution. The entire existing Time management and manual-set workflow remains in scope; controls are blocked until the real authenticated backend is connected.

## Automated verification

Tests include source qualification, secure nonce gating, nonce replay mismatch, source address refusal, NTP mode/leap/stratum checks, high root dispersion, non-monotonic server timestamps, stale/late responses, wrap-safe 32-bit monotonic deltas, RFC 1900-to-Unix epoch conversion, explicit era-1 handling for 2037, fractional timestamps, bounded interpolation, one-hour stale withdrawal and native API truthfulness. Host, ARM7 big-endian linking, browser behavior, and AddressSanitizer/UBSan gates remain mandatory.

**Outstanding:** qualified H910P network interface with NTP request/response transport, production secure entropy and monotonic timer bindings, authentication/NTS, reliable RTC set/read, authenticated time configuration, real IANA timezone/DST database and schedule integration. None of the tests imply the live management card has been synchronized.
