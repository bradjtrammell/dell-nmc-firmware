#!/usr/bin/env python3
"""Independent preflight checks against the known v0.5.10 BIN."""
from __future__ import annotations
import importlib.util
from pathlib import Path
import tempfile
import unittest

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('h910p_release_preflight', HERE/'tools'/'release_preflight.py')
import sys
sys.path.insert(0,str(HERE/'tools'))
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
KNOWN='e3adea74aad966c15e87f9fdb7407bb9d9d810e20158da54bad639466f70b8bb'
BIN=HERE/'H910P_Custom_UI_v0.5.10.bin'

@unittest.skipUnless(BIN.exists(),"Reference v0.5.10 BIN required from engineering package")
class ReleasePreflightTests(unittest.TestCase):
    def test_known_baseline(self):
        report=audit.verify(BIN,KNOWN)
        self.assertEqual(report['size'],1517026)
        self.assertEqual(report['version'],'0.5.10')
        self.assertTrue(report['crc_valid'])
        self.assertEqual(report['hardware_boot'],'NOT_TESTED')
        self.assertGreaterEqual(report['startup_resources'],14)
    def test_wrong_checksum_fails(self):
        with self.assertRaisesRegex(ValueError,'SHA-256'):
            audit.verify(BIN,'0'*64)
    def test_corrupt_firmware_fails(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/BIN.name
            image=bytearray(BIN.read_bytes());image[2000]^=1;p.write_bytes(image)
            import hashlib
            with self.assertRaisesRegex(ValueError,'CRC'):
                audit.verify(p,hashlib.sha256(image).hexdigest())
    def test_rejects_non_release_name(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'H910P_Custom_UI_latest.bin';p.write_bytes(BIN.read_bytes())
            with self.assertRaisesRegex(ValueError,'filename'):
                audit.verify(p,KNOWN)

if __name__=='__main__':unittest.main(verbosity=2)
