# H910P Settings port — serial investigation

The H910P has an Ethernet jack and a separate RJ45 **Settings** jack. Dell's Network Management Card setup guide describes the latter as an RS-232-style configuration interface supplied with a serial cable, usable even without Ethernet connectivity. It is **not** an Ethernet interface.

## Verified Dell settings

- Serial speed: **9600 baud**
- Data bits: **8**
- Parity: **none**
- Stop bits: **1**
- Flow control: **none**
- Local echo: disabled
- Factory settings-port password: `admin` (documented by Dell as not user-modifiable)

The factory menu offers **Reset**, **Network configuration**, **Set Login Password to Default**, **Return to Default Configuration**, and **Exit**. These are service options; several are destructive or change connectivity. Do not select them during a passive investigation. Within Network configuration, Dell documents an option to **Read Network settings** (a non-destructive way to verify serial communications).

## Confirmed Settings cable wiring

Dell's H910P Network Management Card guide identifies the supplied DB9-to-RJ45 cable connections:

| NMC RJ45 pin | Computer DB9 pin | Signal |
| --- | --- | --- |
| 3 | 2 | NMC transmit → computer receive |
| 6 | 3 | NMC receive ← computer transmit |
| 4 | 5 | Ground |
| Shield | Shield | Cable shield |

Those three connections agree with the standard Cisco rolled-console wiring to a PC-side terminal adapter. This makes a Cisco USB-to-RJ45 lead plausible, but the exact adapter model and voltage levels must still be verified before transmitting. A silent terminal is not evidence that the Settings port is disabled.

The documented serial menu includes reading network settings and recovering access; it does not establish an interactive OS shell, bootloader console, or serial firmware flashing interface.

References: [Dell NMC guide, pp. 79–81](https://usermanual.wiki/Dell/1508079828delllineinteractiverackups1920rusersguide14enus.2134937042.pdf) and [Cisco RJ45 console pinouts](https://www.cisco.com/c/en/us/support/docs/routers/7000-series-routers/12223-14.html).

## Cisco USB-to-RJ45 console cable compatibility

A Cisco-compatible USB-to-RJ45 cable presents a USB serial device on Linux, but the **physical RJ45 pinout and signal levels must still be compatible with the Dell card**. The common Cisco wiring agrees on TX, RX and ground, but each USB cable model must still be checked before use. Do not assume that an apparently silent serial connection means there is no Settings console.

Before transmitting data, establish the USB adapter manufacturer/model, its UART/driver, and the Cisco versus Dell cable wiring. A USB serial device under `/dev/ttyUSB*` or `/dev/serial/by-id/*` proves only that Linux sees the USB adapter, not that the far end's wiring matches.

## Safe, read-only checks on `dockeradm`

Run locally on the host `dockeradm` (not inside an unprivileged container):

```bash
ls -l /dev/serial/by-id/ 2>/dev/null || true
lsusb
journalctl -k -n 80 --no-pager | grep -Ei 'ttyUSB|ttyACM|ftdi|ch34|cp210|usb serial' || true
```

These commands query the Linux host only. They do not transmit to the NMC or change settings. Record the device symlink and USB chipset before selecting a terminal application.

**After wiring is verified**, connect to the identified serial device using a terminal emulator (for example, `picocom -b 9600 --databits 8 --parity n --stopbits 1 --flow n /dev/serial/by-id/DEVICE`). `DEVICE` is deliberately not guessed; substitute the actual verified Linux symlink. If the menu requests a password, Dell's manual documents the factory Settings password as `admin`. Limit exploration to viewing the menu or reading network settings; do not reset, restore defaults, or alter network parameters.

## Research opportunities

The Settings port may provide out-of-band access to the NMC configuration menu and to network recovery when the web interface fails. There is no published evidence from Dell that the normal menu provides bootloader access, arbitrary flash memory writes, interactive shell access, or UART boot logs. Those are research hypotheses, not features. Any investigation beyond the documented menu requires a controlled spare card and a verified rollback procedure.

## References

- [Dell 1920R NMC setup guide (PDF)](https://dl.dell.com/manuals/all-products/esuprt_ser_stor_net/esuprt_rack_infrastructure/dell-line-interactive-rack-ups-1920r_user%27s%20guide16_en-us.pdf)
- [Dell Network Management Card user's guide (PDF)](https://usermanual.wiki/Dell/1508079828delllineinteractiverackups1920rusersguide14enus.2134937042.pdf)
