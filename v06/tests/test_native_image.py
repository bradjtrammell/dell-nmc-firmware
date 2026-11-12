"""Offline ARM image, privilege transition and tamper-detection tests."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

FILE = Path(__file__).resolve().parents[1] / 'tools' / 'verify_native_image.py'
spec = importlib.util.spec_from_file_location('image_verify', FILE)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def synthetic():
    elf = bytearray(52)
    elf[:7] = b'\x7fELF\x01\x02\x01'
    struct.pack_into('>HHII', elf, 16, 2, 40, 1, 0x08004000)
    raw = bytearray(256)
    raw[0:4] = bytes.fromhex('e59ff038')
    raw[4:0x40] = bytes.fromhex('e1a00000') * 15
    struct.pack_into('>I', raw, 0x40, 0x08004060)
    struct.pack_into('>IIIIIII', raw, 0x60,
                     0xE10F0000, 0xE200101F, 0xE3510010, 0x0A00000B,
                     0xE3C0003F, 0xE38000D3, 0xE121F000)
    struct.pack_into('>I', raw, 0xA0, 0xEAFFFFFE)
    return elf, raw


class ResearchImageTests(unittest.TestCase):
    def check(self, elf, raw):
        with tempfile.TemporaryDirectory() as directory:
            a, b = Path(directory) / 'research.elf', Path(directory) / 'research.dat'
            a.write_bytes(elf)
            b.write_bytes(raw)
            return mod.inspect(a, b)

    def test_expected_research_vector(self):
        elf, raw = synthetic()
        info = self.check(elf, raw)
        self.assertFalse(info['flashable'])
        self.assertTrue(info['startupPrivilegeGuard'])
        self.assertTrue(info['interruptsMaskedOnEntry'])
        self.assertEqual(info['resetLiteral'], '0x8004060')

    def test_reject_wrong_endian(self):
        elf, raw = synthetic()
        elf[5] = 1
        with self.assertRaises(ValueError): self.check(elf, raw)

    def test_reject_wrong_entry(self):
        elf, raw = synthetic()
        struct.pack_into('>I', elf, 24, 0x08000000)
        with self.assertRaises(ValueError): self.check(elf, raw)

    def test_reject_corrupt_reset_literal(self):
        elf, raw = synthetic()
        struct.pack_into('>I', raw, 0x40, 0xFFB00000)
        with self.assertRaises(ValueError): self.check(elf, raw)

    def test_reject_corrupt_reset_vector(self):
        elf, raw = synthetic()
        raw[0] = 0
        with self.assertRaises(ValueError): self.check(elf, raw)

    def test_reject_missing_user_guard(self):
        elf, raw = synthetic()
        raw[0x60:0x64] = b'\0\0\0\0'
        with self.assertRaisesRegex(ValueError, 'guard'): self.check(elf, raw)

    def test_reject_missing_irq_mask(self):
        elf, raw = synthetic()
        raw[0x74:0x78] = b'\0\0\0\0'
        with self.assertRaisesRegex(ValueError, 'SVC'): self.check(elf, raw)

    def test_reject_user_mode_branch_removed(self):
        elf, raw = synthetic()
        raw[0x6C:0x70] = bytes.fromhex('e1a00000')
        with self.assertRaisesRegex(ValueError, 'refusal'): self.check(elf, raw)

    def test_reject_user_mode_branch_to_invalid_location(self):
        elf, raw = synthetic()
        raw[0x6C:0x70] = bytes.fromhex('0affffff')
        with self.assertRaisesRegex(ValueError, 'escapes known'): self.check(elf, raw)

    def test_reject_missing_halt(self):
        elf, raw = synthetic()
        raw[0xA0:0xA4] = b'\0\0\0\0'
        with self.assertRaisesRegex(ValueError, 'self-halt'): self.check(elf, raw)


if __name__ == '__main__':
    unittest.main()
