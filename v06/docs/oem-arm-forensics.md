# H910P ARM7 firmware forensics — executable OEM evidence capture

This is a **read-only, offline** investigation of the original Dell H910P
images 01.16.0001 and 01.18.0005. It is a concrete step beyond the earlier
string-frequency inventory, but it is **not** a native H910P driver, a device
probe, a proven recovery path or a flashable firmware build.

## What it establishes

- Uses the existing fully CRC-checked NET+OS / LZSS2 unpacker.
- Refuses any vendor BIN whose SHA-256 does not match an already audited image.
- Scans big-endian ARM-state instructions for PC-relative literal loads and
  branches, then identifies candidate references to existing UPS transport
  strings and known NS7520 MMCR/BAR/OR addresses.
- Independently checks embedded NET+OS bootHdr candidates for valid header,
  CRC, compressed executable, reset vector, and the SHA-256 previously
  observed in Dell's recovery-related image.
- Compares report-label positions and matching embedded recovery-image hashes
  between both OEM releases.
- Writes **sanitized JSON only**: known label names, binary offsets, numeric
  addresses, reference counts and hashes. It never exports vendor
  instructions, embedded executables, raw device data, configuration values,
  user passwords, or a BIN.

**Critical limitation:** A match in a static ARM word scan may be a false
positive from a data region. An instruction referencing a string or MMIO
address does not demonstrate a working driver, physical UPS link, flash layout
or installed bootloader. Treat the output as leads for a more focused reverse
engineering pass, never as deployment evidence.

## Use with Dell originals already available offline

From the repository root, with **original proprietary Dell archives** in
the ignored originals/ directory:

\`\`\`bash
python3 v06/tools/analyze_oem_arm.py \
  originals/NMC_Firmware_01_16_0001_YJ6JM.zip \
  originals/NMC_Firmware_01_18_0005_X2JYN.zip \
  --output v06/build/oem-arm-evidence.json
\`\`\`

The output file must not exist. The directory v06/build/ is created by
\`make all\`. A failed SHA check exits without writing the report.

Run synthetic regression tests without the proprietary archives:

\`\`\`bash
cd v06
python3 -m unittest tests/test_oem_arm_forensics.py -v
\`\`\`

## What follows from the evidence

1. Review literal candidates linked to GetReport/SetReport/COPI/HID strings,
   then confirm genuine executable callsites using ARM control-flow
   disassembly and compare both releases.
2. Distinguish embedded recovery code from the actual resident bootloader by
   **separate, authorized, physical verification**. Generic NET+OS documentation
   confirms a bootloader/application split, not this board's installed map.
3. Capture verified NMC↔UPS report framing and electrical transport without
   transmitting power commands. Only after that can the native provider be
   attached to actual driver data.
4. Identify real flash geometry/erasures and boot rescue before any test
   installer is allowed. Keep stable v0.5.10 unchanged.

No proprietary OEM archives or executable bytes are stored in the repository.
