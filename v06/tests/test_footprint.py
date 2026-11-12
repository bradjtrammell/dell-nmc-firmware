"""Purely offline ARM footprint budget tests; no UPS, network, or flash access."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / 'tools' / 'audit_footprint.py'
spec = importlib.util.spec_from_file_location('audit_footprint', MODULE)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def minimal_elf():
    """Generate independent ELF32 big-endian ARM fixture with a NOLOAD stack."""
    base = module.EXPECTED_ENTRY
    strings = b'\0.shstrtab\0.text\0.bss\0.stack\0'
    # 52 ELF header bytes; section string table at 64, .text payload at 112.
    raw = bytearray(512)
    raw[:7] = b'\x7fELF\x01\x02\x01'
    struct.pack_into('>HHI', raw, 16, 2, 40, 1)
    struct.pack_into('>I', raw, 24, base)
    raw[64:64 + len(strings)] = strings
    raw[112:128] = b'\x00' * 16
    shoff = 256
    struct.pack_into('>I', raw, 32, shoff)
    struct.pack_into('>HHH', raw, 46, 40, 5, 1)
    def section(i, name, typ, flags, addr, off, n):
        struct.pack_into('>IIIIIIIIII', raw, shoff + 40 * i,
                         strings.index(name), typ, flags, addr, off, n, 0, 0, 4, 0)
    section(1, b'.shstrtab', 3, 0, 0, 64, len(strings))
    section(2, b'.text', 1, 6, base, 112, 16)
    section(3, b'.bss', 8, 3, base + 16, 0, 32)
    section(4, b'.stack', 8, 3, base + 48, 0, 8192)
    return bytes(raw)


def policy():
    return json.loads((ROOT / 'resource-policy.json').read_text())


class FootprintTests(unittest.TestCase):
    def test_realistic_freestanding_sections(self):
        info = module.inspect_elf(minimal_elf())
        self.assertEqual(info['ramSpanBytes'], 16 + 32 + 8192)
        self.assertEqual(info['zeroFillBytes'], 32 + 8192)
        self.assertEqual(info['reservedStackBytes'], 8192)
        self.assertEqual(info['initializedBytes'], 16)
        result = module.check_limits(info, 16, policy())
        self.assertTrue(result['pass'])
        self.assertFalse(result['flashable'])
        self.assertFalse(result['hardwareVerified'])

    def test_provisional_hardware_not_misrepresented(self):
        cfg = policy()
        self.assertEqual(cfg['reference']['dellImageBytes'], 1517029)
        self.assertIsNone(cfg['hardware']['installedFlashBytes'])
        self.assertIsNone(cfg['hardware']['installedRAMBytes'])
        cfg['hardware']['verified'] = True
        with self.assertRaises(module.FootprintError):
            module.check_limits(module.inspect_elf(minimal_elf()), 16, cfg)

    def test_reject_overweight_flash_reference(self):
        info = module.inspect_elf(minimal_elf())
        result = module.check_limits(info, policy()['researchLimits']['rawImageBytes'] + 1, policy())
        self.assertFalse(result['pass'])
        self.assertLess(result['headroomBytes']['rawResearchImage'], 0)

    def test_reject_ram_over_budget(self):
        info = module.inspect_elf(minimal_elf())
        info['ramSpanBytes'] = policy()['researchLimits']['ramSpanBytes'] + 1
        result = module.check_limits(info, 16, policy())
        self.assertFalse(result['pass'])

    def test_reject_missing_stack(self):
        info = module.inspect_elf(minimal_elf())
        info['reservedStackBytes'] = 0
        result = module.check_limits(info, 16, policy())
        self.assertFalse(result['pass'])

    def test_reject_wrong_isa_and_endianness(self):
        elf = bytearray(minimal_elf())
        elf[5] = 1
        with self.assertRaises(module.FootprintError):
            module.inspect_elf(bytes(elf))
        elf = bytearray(minimal_elf())
        struct.pack_into('>H', elf, 18, 62)
        with self.assertRaises(module.FootprintError):
            module.inspect_elf(bytes(elf))

    def test_reject_duplicate_section_overlay(self):
        elf = bytearray(minimal_elf())
        # Move .bss over .text
        struct.pack_into('>I', elf, 256 + 40 * 3 + 12, module.EXPECTED_ENTRY + 4)
        with self.assertRaises(module.FootprintError):
            module.inspect_elf(bytes(elf))

    def test_reject_bad_section_table(self):
        elf = bytearray(minimal_elf())
        struct.pack_into('>I', elf, 32, 0xfffffff0)
        with self.assertRaises(module.FootprintError):
            module.inspect_elf(bytes(elf))

    def test_cli_production_proof_always_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            elf = root / 'test.elf'
            data = root / 'test.dat'
            elf.write_bytes(minimal_elf())
            data.write_bytes(b'x' * 16)
            command = [sys.executable, str(MODULE), str(elf), str(data),
                       '--require-production-ready']
            result = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 3)
            self.assertIn('NOT_FLASHABLE', result.stdout)
            self.assertIn('REFUSED', result.stderr)


if __name__ == '__main__':
    unittest.main()
