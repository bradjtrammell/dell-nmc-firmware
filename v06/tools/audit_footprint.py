#!/usr/bin/env python3
"""Offline ELF32 big-endian ARM size gates; never claims physical flash capacity.

Reports allocatable memory footprint including NOLOAD stack/BSS, a raw-image
transport estimate, and explicitly provisional development budgets. Does not
produce or endorse flashable firmware, and never contacts a device.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys

ELF32_HEADER_SIZE = 52
ELF32_SECTION_SIZE = 40
SHF_ALLOC = 0x2
SHT_NOBITS = 8
EXPECTED_ENTRY = 0x08004000


class FootprintError(ValueError):
    pass


def inspect_elf(data: bytes) -> dict:
    if len(data) < ELF32_HEADER_SIZE or data[:7] != b'\x7fELF\x01\x02\x01':
        raise FootprintError('expected ELF32 big-endian image')
    kind, machine, version = struct.unpack_from('>HHI', data, 16)
    entry, = struct.unpack_from('>I', data, 24)
    section_offset, = struct.unpack_from('>I', data, 32)
    section_size, section_count, shstr_index = struct.unpack_from('>HHH', data, 46)
    if (kind, machine, version, entry) != (2, 40, 1, EXPECTED_ENTRY):
        raise FootprintError('not the verified ARM7 research entry format')
    if section_count < 2 or section_count > 4096 or section_size < ELF32_SECTION_SIZE:
        raise FootprintError('invalid ELF section table')
    if section_offset < ELF32_HEADER_SIZE or section_offset + section_size * section_count > len(data):
        raise FootprintError('section table outside ELF')
    if shstr_index >= section_count:
        raise FootprintError('invalid section name table index')
    sections = []
    for i in range(section_count):
        at = section_offset + i * section_size
        name_off, typ, flags, addr, file_off, size, _link, _info, _align, _entry_size = struct.unpack_from('>IIIIIIIIII', data, at)
        sections.append({'nameOffset': name_off, 'type': typ, 'flags': flags, 'address': addr,
                         'fileOffset': file_off, 'bytes': size})
    names = sections[shstr_index]
    if names['type'] == SHT_NOBITS or names['fileOffset'] + names['bytes'] > len(data):
        raise FootprintError('invalid section name table')
    strings = data[names['fileOffset']:names['fileOffset'] + names['bytes']]
    allocated = []
    for section in sections:
        off = section['nameOffset']
        if off >= len(strings):
            raise FootprintError('section name outside string table')
        end = strings.find(b'\0', off)
        if end < 0:
            raise FootprintError('unterminated section name')
        name = strings[off:end].decode('ascii', errors='replace')
        if not (section['flags'] & SHF_ALLOC) or not section['bytes']:
            continue
        if section['address'] < EXPECTED_ENTRY or section['address'] + section['bytes'] > 0xFFFFFFFF:
            raise FootprintError('allocated section outside research RAM image')
        if section['type'] != SHT_NOBITS and section['fileOffset'] + section['bytes'] > len(data):
            raise FootprintError('loaded section outside ELF file')
        allocated.append({'name': name, 'address': section['address'],
                          'bytes': section['bytes'], 'zeroFill': section['type'] == SHT_NOBITS})
    allocated.sort(key=lambda s: (s['address'], s['name']))
    if not allocated or allocated[0]['address'] != EXPECTED_ENTRY:
        raise FootprintError('no allocated reset section at expected RAM load address')
    end = EXPECTED_ENTRY
    for s in allocated:
        if s['address'] < end:
            raise FootprintError('overlapping allocated sections')
        end = s['address'] + s['bytes']
    if not any(s['name'] == '.text' for s in allocated):
        raise FootprintError('missing executable .text section')
    stack = sum(s['bytes'] for s in allocated if s['name'] == '.stack')
    return {
        'architecture': 'ARM7TDMI ELF32 big-endian',
        'loadAddress': f'0x{EXPECTED_ENTRY:08x}',
        'ramSpanBytes': end - EXPECTED_ENTRY,
        'zeroFillBytes': sum(s['bytes'] for s in allocated if s['zeroFill']),
        'initializedBytes': sum(s['bytes'] for s in allocated if not s['zeroFill']),
        'reservedStackBytes': stack,
        'sections': allocated,
    }


def check_limits(elf_info: dict, raw_size: int, policy: dict) -> dict:
    if policy.get('schemaVersion') != 1:
        raise FootprintError('unknown footprint policy version')
    if policy.get('hardware', {}).get('verified') is not False:
        raise FootprintError('do not claim verified hardware from provisional budget')
    limits = policy.get('researchLimits', {})
    for key in ('rawImageBytes', 'ramSpanBytes', 'minimumStackBytes'):
        value = limits.get(key)
        if type(value) is not int or value <= 0:
            raise FootprintError(f'invalid research limit {key}')
    raw = limits['rawImageBytes']
    ram = limits['ramSpanBytes']
    stack = limits['minimumStackBytes']
    checks = {
        'rawResearchImage': {'measured': raw_size, 'limit': raw, 'pass': raw_size <= raw},
        'ramSpan': {'measured': elf_info['ramSpanBytes'], 'limit': ram,
                    'pass': elf_info['ramSpanBytes'] <= ram},
        'reservedStack': {'measured': elf_info['reservedStackBytes'], 'minimum': stack,
                          'pass': elf_info['reservedStackBytes'] >= stack},
    }
    if raw_size <= 0:
        raise FootprintError('empty research image')
    if not elf_info['sections'] or elf_info['initializedBytes'] > raw_size:
        raise FootprintError('raw research image is inconsistent with ELF sections')
    return {
        'pass': all(c['pass'] for c in checks.values()),
        'flashable': False,
        'hardwareVerified': False,
        'disposition': 'RESEARCH_ONLY_NOT_FLASHABLE',
        'limitations': 'No installed flash/RAM geometry, protected bootloader regions, board drivers or recovery qualification',
        'referenceDellImageBytes': policy['reference']['dellImageBytes'],
        'measurements': {'rawImageBytes': raw_size, **elf_info},
        'checks': checks,
        'headroomBytes': {'rawResearchImage': raw - raw_size, 'ramSpan': ram - elf_info['ramSpanBytes']},
    }


def audit(elf: Path, raw: Path, policy_path: Path) -> dict:
    policy = json.loads(policy_path.read_text(encoding='utf-8'))
    elf_info = inspect_elf(elf.read_bytes())
    raw_size = raw.stat().st_size
    return check_limits(elf_info, raw_size, policy)


def main() -> int:
    parser = argparse.ArgumentParser(description='Fail-closed resource gate for NON-FLASHABLE v0.6 ARM research ELF')
    parser.add_argument('elf', type=Path)
    parser.add_argument('raw', type=Path)
    parser.add_argument('--policy', type=Path, default=Path(__file__).resolve().parents[1] / 'resource-policy.json')
    parser.add_argument('--report', type=Path)
    parser.add_argument('--require-production-ready', action='store_true', help='Fail: physical firmware qualification is not implemented')
    args = parser.parse_args()
    try:
        report = audit(args.elf, args.raw, args.policy)
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n', encoding='utf-8')
        print(json.dumps(report, indent=2, sort_keys=True))
        if args.require_production_ready:
            print('REFUSED: resource budgets cannot certify a flashable H910P firmware', file=sys.stderr)
            return 3
        return 0 if report['pass'] else 2
    except (FootprintError, OSError, json.JSONDecodeError, KeyError, struct.error) as exc:
        print(f'RESOURCE AUDIT FAILED: {exc}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
