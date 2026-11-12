# NS7520 native polling clock for TCP and hardware runtime

The hardware timer has a 27-bit programmable count register and is mapped at
0xFFB00010/14 (Timer 1) and 0xFFB00018/1C (Timer 2).
The code implements start, monotonic millisecond polling, and stop using
those exact registers. TCLK selects the verified system clock rather than a
guessed oscillator frequency, and timer interrupts are deliberately disabled.

The boot port must provide the actual verified timer channel ownership and
ticks-per-millisecond calibration. A running timer is never seized. An
unverified clock rate, timer read/write failure, a changed timer control
register or an ambiguous wrap fails closed so TCP retransmission deadlines
are never fabricated. First-period TIP is explicitly cleared after enabling
the timer, per the NS7520 hardware reference.

This hardware-clock driver can be passed as the `clock_ms` callback to
`ou_board_runtime_start`. It is not yet provisioned automatically, since
the H910P installed clock frequency and ownership must be verified.

Host gcc, ASAN/UBSAN and big-endian ARM7 compiler tests cover startup,
ticks, wrap handling, denied ownership and stop.

Reference: Digi NS7520 Hardware Reference, GEN module Timer Control and
Timer Status register tables.
https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/

## Bounded tick-to-millisecond conversion

Timer polling no longer subtracts one ms interval repeatedly from the
elapsed tick delta (up to 134,217,727 ticks). A constant 32-iteration
binary long divider converts the maximum count without a libgcc EABI
division helper or startup hangs. Both 1 and 251 ticks-per-ms edge cases
are exercised by host GCC and ASAN/UBSAN, and the driver compiles for
big-endian ARM7.
