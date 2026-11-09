"""Offline regression suite for H910P firmware format and UI resource safety.
Run: python3 -m unittest -v test_validation.py
Requires the two original Dell ZIP archives in the parent directory.
"""
from __future__ import annotations

from dataclasses import replace
import hashlib
from pathlib import Path
import random
import struct
import tempfile
import unittest
import zipfile

from h910p_rebuild import (
    HEADER_LEN, Image, ValidationError, crc32_netos, load_zip, pack_lzss2,
    read_image, rebuild_same_application, unpack_lzss2
)
from rompager_map import discover, export_static_candidates
from ui_patch_proof import test_css_patch

HERE=Path(__file__).resolve().parent
ROOT=HERE/'originals' if (HERE/'originals').is_dir() else HERE.parent/'originals' if (HERE.parent/'originals').is_dir() else HERE.parent
A16=ROOT/'NMC_Firmware_01_16_0001_YJ6JM.zip'
A18=ROOT/'NMC_Firmware_01_18_0005_X2JYN.zip'


class TestFirmwareInputs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.input=[]
        for p in (A16,A18):
            _,raw=load_zip(p)
            cls.input.append((p,read_image(raw)))

    def test_expected_zip_hash(self):
        hashes=['46f9693350294cd4e9204353b4224002241ccd08c1cac47830d60cfbaf854697',
                '7190e0b41be5af5f0e26819ccc73a69e13748273ec5504685c1522b293183866']
        for (p,_),expected in zip(self.input,hashes):
            with self.subTest(archive=p.name):
                self.assertEqual(hashlib.sha256(p.read_bytes()).hexdigest(),expected)

    def test_both_original_checksums(self):
        for p,img in self.input:
            with self.subTest(archive=p.name):
                self.assertEqual(crc32_netos(img.original),0)
                self.assertEqual(len(img.unpacked),2533980 if '01_16' in p.name else 2757180)

    def test_byte_exact_application_round_trip_and_flash_header(self):
        for p,img in self.input:
            with self.subTest(archive=p.name):
                rebuilt = rebuild_same_application(img)
                got = read_image(rebuilt)
                self.assertEqual(got.unpacked,img.unpacked)
                self.assertEqual(got.header[:32],img.header[:32])
                self.assertEqual(crc32_netos(rebuilt),0)
                self.assertLessEqual(len(got.compressed),len(img.compressed))

    def test_corrupt_crc_rejected(self):
        raw=bytearray(self.input[1][1].original)
        raw[1024]^=1
        with self.assertRaisesRegex(ValidationError, 'CRC'):
            read_image(raw)

    def test_corrupt_header_magic_rejected(self):
        raw=bytearray(self.input[1][1].original)
        raw[8:15]=b'wrongHd'
        with self.assertRaisesRegex(ValidationError, 'header'):
            read_image(raw)

    def test_corrupt_image_length_rejected(self):
        raw=bytearray(self.input[1][1].original)
        struct.pack_into('>I',raw,32,9)
        with self.assertRaises(ValidationError):
            read_image(raw)

    def test_oversize_input_rejected(self):
        with self.assertRaises(ValidationError):
            read_image(b'a'*(16*1024*1024+1))

    def test_truncated_image_rejected(self):
        with self.assertRaises(ValidationError):
            read_image(self.input[1][1].original[:-8])

    def test_bad_zip_member_layout_rejected(self):
        with tempfile.TemporaryDirectory() as root:
            path=Path(root)/'bad.zip'
            with zipfile.ZipFile(path,'w') as zf:
                zf.writestr('one.bin',b'1234')
                zf.writestr('two.bin',b'5678')
            with self.assertRaises(ValidationError):
                load_zip(path)


class TestEncoderBehavior(unittest.TestCase):
    def test_synthetic_bodies(self):
        cases=[b'',b'a',b'ab',b'abc',b'aaaa',b'a'*5000,b' '*5000,
            bytes(range(256))*37,b'abcabc'*1000,b'\x00\xff\x80'*1000,
            b'1234567890'*500+b'0987654321'*500]
        for src in cases:
            with self.subTest(length=len(src),prefix=src[:8]):
                self.assertEqual(unpack_lzss2(pack_lzss2(src)),src)

    def test_seeded_random_data_round_trips(self):
        rng=random.Random(0x9100)
        for length in (0,1,2,3,7,8,9,17,18,19,32,255,256,257,4095,4096,4097,8193):
            src=rng.randbytes(length)
            with self.subTest(length=length):
                self.assertEqual(unpack_lzss2(pack_lzss2(src)),src)

    def test_overlap_backreferences(self):
        a=b'AB'*20000
        self.assertEqual(unpack_lzss2(pack_lzss2(a)),a)

    def test_truncated_reference_rejected(self):
        with self.assertRaisesRegex(ValidationError,'Truncated'):
            unpack_lzss2(b'\x00\x01')

    def test_invalid_encoder_candidates_rejected(self):
        with self.assertRaises(ValidationError):
            pack_lzss2(b'x',max_candidates=0)


class TestRomPagerMapping(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.images=[]
        for path in (A16,A18):
            _,raw=load_zip(path)
            cls.images.append(read_image(raw))

    def test_mapping_both_versions(self):
        expected=[(0x204d6c,156),(0x23352c,159)]
        for img,(base,count) in zip(self.images,expected):
            with self.subTest(count=count):
                got,assets=discover(img.unpacked)
                self.assertEqual(got,base)
                self.assertEqual(len(assets),count)
                self.assertTrue(all(obj['terminated'] for obj in assets))

    def test_key_ui_resources_are_real_rompager_objects(self):
        _,objs=discover(self.images[1].unpacked)
        obj={o['url']:o for o in objs}
        for name in ('/index.htm','/menu.htm','/html/mgeweb.css',
                     '/html/utils.js','/ups_cntr.htm','/ups_propAbout.htm','/app_upload.htm'):
            with self.subTest(url=name):
                self.assertIn(name,obj)
                self.assertGreater(obj[name]['items_count'],0)

    def test_static_resource_export_bytes(self):
        with tempfile.TemporaryDirectory() as root:
            _,objs=discover(self.images[1].unpacked)
            records=export_static_candidates(self.images[1].unpacked,objs,Path(root))
            paths={r['url'] for r in records}
            self.assertIn('/html/utils.js',paths)
            self.assertNotIn('/html/mgeweb.css',paths) # Contains compressed/dictionary elements
            self.assertNotIn('/index.htm',paths) # Not a standalone static asset

    def test_css_patch_changes_only_one_decompressed_byte(self):
        with tempfile.TemporaryDirectory() as root:
            report=test_css_patch(A18,Path(root)/'result.json')
            self.assertEqual(report['changed_application_bytes'],1)
            self.assertTrue(report['rompager_descriptors_unchanged'])
            self.assertTrue(report['recomputed_crc_valid'])
            self.assertLessEqual(report['image_growth_bytes'],0)


if __name__=='__main__':
    unittest.main(verbosity=2)
