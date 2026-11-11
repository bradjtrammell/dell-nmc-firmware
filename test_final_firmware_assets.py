#!/usr/bin/env python3
"""Verify final BIN resources, asset encoding, and recovered OEM fallback pages."""
import base64
import gzip
import json
import re
import unittest
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'tools'))
from h910p_rebuild import read_image,load_zip
from rompager_map import discover

class FirmwareEmbeddedAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.meta=json.loads((ROOT/'build/VALIDATION.json').read_text())
        cls.image=read_image((ROOT/'build/H910P_01.18.0005_INSTRUMENT_PANEL_EXPERIMENTAL.bin').read_bytes())
        _,entries=discover(cls.image.unpacked)
        cls.table={it['url']:it for it in entries}
        _,factory=load_zip(ROOT/'originals/NMC_Firmware_01_18_0005_X2JYN.zip')
        cls.oem=read_image(factory)
        _,factory_entries=discover(cls.oem.unpacked)
        cls.oem_table={it['url']:it for it in factory_entries}

    def contents(self,resource,data=None,descriptors=None):
        data = self.image.unpacked if data is None else data
        descriptors = self.table if descriptors is None else descriptors
        return b''.join(
            data[item['offset']:data.index(b'\0',item['offset'])]
            for item in descriptors[resource]['items'] if item['type']==1
        )

    def test_active_resources_never_blank(self):
        for name in self.meta['repurposed_original_help_pages']:
            with self.subTest(path=name):
                self.assertTrue(self.contents(name).strip())
                self.assertEqual(self.contents(name),(ROOT/'build/served-native-ui'/name.lstrip('/')).read_bytes())

    def test_final_bin_contains_exact_decompressible_app(self):
        encoded=''.join(re.findall(
            r'window\.H910P_PACK\+="([^"]+)";',
            '\n'.join(self.contents(n).decode('ascii') for n in self.meta['repurposed_original_help_pages'])
        ))
        self.assertEqual(len(encoded),self.meta['encoded_ui_bytes'])
        self.assertEqual(self.meta['embedded_encoding'],'base85')
        app=gzip.decompress(base64.b85decode(encoded)[:self.meta['compressed_ui_bytes']])
        self.assertIn(b"U.uiVersion='0.5.11-beta.3'",app)
        self.assertIn(b'U.startApp',app)
        self.assertIn(b'</svg>',app)
        self.assertTrue(app.lstrip().startswith(b'.sr-only{'))

    def test_no_retired_resource_reused_for_asset(self):
        retired=set(self.meta['retired_legacy_synoptic_resources'])
        assets=set(self.meta['repurposed_original_help_pages'])
        self.assertFalse(assets&retired,assets&retired)

    def test_original_static_entry_pages_exactly_preserved(self):
        for path in ('/menu.htm','/toolbar_main.htm','/mgeups/default.htm'):
            with self.subTest(path=path):
                old=self.contents(path,self.oem.unpacked,self.oem_table)
                new=self.contents(path)
                self.assertEqual(new,old)
                self.assertTrue(new.strip())

    def test_default_serves_embedded_console(self):
        self.assertEqual(self.contents('/default.htm').rstrip(),self.contents('/index.htm').rstrip())
        self.assertEqual(self.contents('/default.htm').count(b'<script src='),self.meta['native_default_script_count'])
        self.assertEqual(self.table['/default.htm']['mime_enum'],self.table['/index.htm']['mime_enum'])

    def test_original_stylesheet_still_has_content(self):
        path='/html/mgeweb.css'
        original=self.oem_table[path]
        latest=self.table[path]
        self.assertEqual(latest['items'],original['items'])
        self.assertEqual(self.contents(path),self.contents(path,self.oem.unpacked,self.oem_table))

if __name__=='__main__':unittest.main(verbosity=2)
