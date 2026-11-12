# NS7520 HAL timer binding

The physical MMIO HAL now grants one of the two GEN hardware timer
register pairs (control/status) only through `ou_ns7520_hal_timer_bind`,
which requires an initialized qualified board HAL, exclusive timer
ownership and an independently verified timer-clock rate.

Timer writes cannot touch the other timer, watchdog, serial interface,
GEN system clocks, memory controller, firmware flash, or UPS control.
The HAL refuses unbinding while the selected timer remains enabled,
and the existing `ou_timer_start/now/stop` functions work through
the HAL's checked read/write callbacks.

Host tests exercise denial before binding; invalid ownership/clock
checks; blocked writes to the opposite channel; timer start/elapsed
milliseconds; stop; and lease release. ASAN/UBSAN and big-endian ARM7
object compilation passed locally. A physical H910P timer clock still
needs independent qualification before the boot port may use it.
