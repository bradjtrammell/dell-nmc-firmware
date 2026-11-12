#!/usr/bin/env python3
"""Validate research-only big-endian ARM7 ELF, vectors and guarded entry.

This is not a boot qualification or a flashable image.
"""
from pathlib import Path
import argparse
import struct

ENTRY = 0x08004000
RESET_GUARD = (0xE10F0000, 0xE200101F, 0xE3510010)
SUPERVISOR_SWITCH = (0xE3C0003F, 0xE38000D3, 0xE121F000)


def inspect(elf_path: Path, raw_path: Path) -> dict:
    elf = elf_path.read_bytes()
    raw = raw_path.read_bytes()
    if len(elf) < 52 or elf[:7] != b'\x7fELF\x01\x02\x01':
        raise ValueError('not ELF32 big-endian')
    kind, machine, version = struct.unpack_from('>HHI', elf, 16)
    entry, = struct.unpack_from('>I', elf, 24)
    if (kind, machine, version, entry) != (2, 40, 1, ENTRY):
        raise ValueError('unexpected ELF identity, architecture or entry')
    if len(raw) < 0xA4 or raw[:4] != bytes.fromhex('e59ff038'):
        raise ValueError('incorrect reset vector')
    if raw[4:0x40] != bytes.fromhex('e1a00000') * 15:
        raise ValueError('unknown vector padding')
    reset, = struct.unpack_from('>I', raw, 0x40)
    if reset < ENTRY + 0x60 or reset >= ENTRY + 0x1000 or reset % 4:
        raise ValueError('reset literal outside expected research layout')
    reset_off = reset - ENTRY
    if reset_off + 0x44 > len(raw):
        raise ValueError('truncated native startup')
    first = struct.unpack_from('>III', raw, reset_off)
    transition = struct.unpack_from('>III', raw, reset_off + 0x10)
    branch, = struct.unpack_from('>I', raw, reset_off + 0x0c)
    if first != RESET_GUARD or transition != SUPERVISOR_SWITCH:
        raise ValueError('missing privileged mode guard or SVC/IRQ masks')
    if (branch & 0xff000000) != 0x0a000000:
        raise ValueError('missing User-mode refusal branch')
    displacement = branch & 0x00ffffff
    if displacement & 0x800000:
        displacement -= 1 << 24
    target = reset_off + 0x0c + 8 + (displacement << 2)
    if target < reset_off + 0x1c or target + 4 > len(raw):
        raise ValueError('User-mode failure branch escapes known startup code')
    if raw[target:target+4] != bytes.fromhex('eafffffe'):
        raise ValueError('User-mode failure target is not a self-halt')
    if len(raw) >= 1_048_576:
        raise ValueError('offline image outside bounded research envelope')
    return {'bigEndian': True, 'arm7tdmi': True, 'entry': hex(entry),
            'resetLiteral': hex(reset), 'rawBytes': len(raw),
            'startupPrivilegeGuard': True, 'interruptsMaskedOnEntry': True,
            'flashable': False, 'boardSupport': False, 'upsDriver': False}


def main():
    a = argparse.ArgumentParser()
    a.add_argument('elf', type=Path)
    a.add_argument('raw', type=Path)
    args = a.parse_args()
    print(inspect(args.elf, args.raw))


if __name__ == '__main__':
    main()
