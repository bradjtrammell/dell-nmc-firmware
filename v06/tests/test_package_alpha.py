"""Independent OEM-envelope regression tests; no hardware or network access."""
import importlib.util
import hashlib
from pathlib import Path
import struct
import tempfile
import unittest

FILE = Path(__file__).resolve().parents[1] / 'tools' / 'package_alpha.py'
spec = importlib.util.spec_from_file_location('package_alpha', FILE)
packer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packer)


def synthetic_raw():
    buf = bytearray(1024)
    buf[:4] = bytes.fromhex('e59ff038')
    buf[4:0x40] = bytes.fromhex('e1a00000') * 15
    struct.pack_into('>I', buf, 0x40, 0x08004060)
    struct.pack_into('>IIIIIII', buf, 0x60,
                     0xE10F0000, 0xE200101F, 0xE3510010, 0x0A00000B,
                     0xE3C0003F, 0xE38000D3, 0xE121F000)
    struct.pack_into('>I', buf, 0xA0, 0xEAFFFFFE)
    return bytes(buf)


class PackageTests(unittest.TestCase):
    def test_structural_oem_envelope(self):
        original = synthetic_raw()
        image, info = packer.build(original)
        self.assertEqual(packer.crc_netos(image), 0)
        self.assertEqual(image[8:16], b'bootHdr\0')
        self.assertEqual(info['unpackedSHA256'], hashlib.sha256(original).hexdigest())
        self.assertEqual(packer.unpack_lzss(image[36:-4]), original)
        self.assertFalse(info['safeToInstall'])
        self.assertEqual(info['release'], '0.6.0-alpha.1')

    def test_different_payload_roundtrip(self):
        raw = bytearray(synthetic_raw())
        raw[-1] = 0xa3
        image, _ = packer.build(bytes(raw))
        self.assertEqual(packer.unpack_lzss(image[36:-4]), bytes(raw))

    def test_empty_image_rejected(self):
        with self.assertRaises(ValueError): packer.build(b'')

    def test_vector_rejected(self):
        raw = bytearray(synthetic_raw())
        raw[0] ^= 0xff
        with self.assertRaises(ValueError): packer.build(raw)

    def test_privilege_guard_rejected(self):
        raw = bytearray(synthetic_raw())
        raw[0x60] = 0
        with self.assertRaises(ValueError): packer.build(raw)

    def test_halt_rejected(self):
        raw = bytearray(synthetic_raw())
        raw[0xa0:0xa4] = b'xxxx'
        with self.assertRaises(ValueError): packer.build(raw)

    def test_max_payload_rejected(self):
        raw = synthetic_raw() + b'\x00' * (packer.MAX_APPLICATION + 1)
        with self.assertRaises(ValueError): packer.build(raw)

    def test_malformed_compressed_stream_rejected(self):
        with self.assertRaises(ValueError): packer.unpack_lzss(b'\x00\x01')

    def test_cli_no_overwrite(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            source = root/'app.dat'; source.write_bytes(synthetic_raw())
            output = root/'app.bin'; output.write_bytes(b'important')
            manifest = root/'manifest.json'
            self.assertEqual(packer.main([str(source),'--output',str(output),
                '--manifest',str(manifest),'--acknowledge-research-only']),2)
            self.assertEqual(output.read_bytes(), b'important')

    def test_cli_success_only_research(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            source=root/'app.dat'; source.write_bytes(synthetic_raw())
            output=root/'app.bin'; manifest=root/'manifest.json'
            self.assertEqual(packer.main([str(source),'--output',str(output),
                '--manifest',str(manifest),'--acknowledge-research-only']),0)
            self.assertIn('"safeToInstall": false',manifest.read_text())
            self.assertEqual(packer.crc_netos(output.read_bytes()),0)


if __name__=='__main__':
    unittest.main()
