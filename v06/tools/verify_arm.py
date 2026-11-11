#!/usr/bin/env python3
"""Verify big-endian ARM ELF object produced for the NS7520 ARM7 target.

Never packages an H910P flash image, even if a valid object is supplied.
"""
from pathlib import Path
import struct
import sys


def verify(data: bytes) -> None:
    if len(data) < 52 or data[:6] != b"\x7fELF\x01\x02":
        raise ValueError("Expected big-endian ELF32")
    if data[6] != 1:
        raise ValueError("Invalid ELF version")
    kind, machine, version = struct.unpack_from(">HHI", data, 16)
    if kind != 1 or machine != 40 or version != 1:
        raise ValueError("Expected relocatable ARM ELF32 object")
    if len(data) > 256 * 1024:
        raise ValueError("Unexpected ARM core object size")


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: verify_arm.py object.o", file=sys.stderr)
        return 2
    data = Path(sys.argv[1]).read_bytes()
    verify(data)
    print(f"ARM7 big-endian ELF32 object verified: {len(data)} bytes; no firmware BIN generated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
