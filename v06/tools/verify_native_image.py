#!/usr/bin/env python3
"""Validate research-only big-endian ARM7 ELF and vector, NOT a flash image."""
from pathlib import Path
import argparse
import struct

def inspect(elf_path: Path, raw_path: Path) -> dict:
    elf = elf_path.read_bytes()
    raw = raw_path.read_bytes()
    if len(elf) < 52 or elf[:7] != b'\x7fELF\x01\x02\x01':
        raise ValueError('not ELF32 big-endian')
    kind, machine, version = struct.unpack_from('>HHI', elf, 16)
    entry, = struct.unpack_from('>I', elf, 24)
    if (kind, machine, version, entry) != (2, 40, 1, 0x08004000):
        raise ValueError('unexpected ELF identity, architecture or entry')
    if len(raw) < 0x64 or raw[:4] != bytes.fromhex('e59ff038'):
        raise ValueError('incorrect reset vector')
    if raw[4:0x40] != bytes.fromhex('e1a00000') * 15:
        raise ValueError('unknown vector padding')
    reset, = struct.unpack_from('>I', raw, 0x40)
    if reset < 0x08004060 or reset >= 0x08005000 or reset % 4:
        raise ValueError('reset literal outside expected research layout')
    if len(raw) >= 1_048_576:
        raise ValueError('offline image outside bounded research envelope')
    return {'bigEndian': True, 'arm7tdmi': True, 'entry': hex(entry),
            'resetLiteral': hex(reset), 'rawBytes': len(raw),
            'flashable': False, 'boardSupport': False, 'upsDriver': False}

def main():
    a = argparse.ArgumentParser()
    a.add_argument('elf', type=Path)
    a.add_argument('raw', type=Path)
    args = a.parse_args()
    print(inspect(args.elf, args.raw))

if __name__ == '__main__':
    main()
