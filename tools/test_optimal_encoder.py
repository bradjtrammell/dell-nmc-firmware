#!/usr/bin/env python3
"""Regression tests for the exact-optimal LZSS2 encoder used by the custom UI.

Runs locally with the C++ executable built by build_native.py. No network or UPS.
"""
import os
from pathlib import Path
import random
import subprocess
import tempfile
import unittest

from h910p_rebuild import unpack_lzss2

EXE = Path(__file__).resolve().parent.parent / 'build' / 'lzss_opt'

class TestOptimizedEncoder(unittest.TestCase):
    def encode(self, data):
        with tempfile.TemporaryDirectory() as folder:
            src = Path(folder) / 'src.bin'
            dst = Path(folder) / 'lzss.bin'
            src.write_bytes(data)
            subprocess.run([str(EXE), str(src), str(dst)], check=True, capture_output=True, timeout=30)
            compressed = dst.read_bytes()
            self.assertEqual(unpack_lzss2(compressed), data)
            return compressed

    def test_short_literals(self):
        for data in [b'a', b'hi', b'abc', b'abcdefg', b'abcdefgh', b'abcdefghi']:
            with self.subTest(data=data): self.encode(data)

    def test_ring_wrap_and_overlapping_matches(self):
        self.encode(b'AB' * 4098 + b'ZX' * 6000 + b'AB' * 2100)

    def test_repetitive_data_and_random_data(self):
        rng = random.Random(90210)
        self.encode(b'1'*10000 + b'0'*20000)
        self.encode(rng.randbytes(14000))

    def test_deterministic_encoding(self):
        data=(b'Dell H910P 01.18.0005'*2000)
        self.assertEqual(self.encode(data), self.encode(data))

if __name__ == '__main__': unittest.main(verbosity=2)
