"""Offline ARM image identifier and tamper-detection regression tests."""
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
        self.assertEqual(info['resetLiteral'], '0x8004060')

    def test_reject_wrong_endian(self):
        elf, raw = synthetic()
        elf[5] = 1
        with self.assertRaises(ValueError):
            self.check(elf, raw)

    def test_reject_wrong_entry(self):
        elf, raw = synthetic()
        struct.pack_into('>I', elf, 24, 0x08000000)
        with self.assertRaises(ValueError):
            self.check(elf, raw)

    def test_reject_corrupt_reset_literal(self):
        elf, raw = synthetic()
        struct.pack_into('>I', raw, 0x40, 0xFFB00000)
        with self.assertRaises(ValueError):
            self.check(elf, raw)

    def test_reject_corrupt_reset_vector(self):
        elf, raw = synthetic()
        raw[0] = 0
        with self.assertRaises(ValueError):
            self.check(elf, raw)


if __name__ == '__main__':
    unittest.main()
