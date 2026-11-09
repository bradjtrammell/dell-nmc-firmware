#!/usr/bin/env python3
"""Validate the unaltered, rendered /index.htm resource wiring in custom BIN.

Unlike earlier browser tests, these checks do not pre-inject any scripts. They
validate the page actually emitted by RomPager and every referenced resource.
No network, credentials, or live hardware are accessed.
"""
from html.parser import HTMLParser
from pathlib import Path
import json
import re
import unittest
import sys

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'tools'))
from h910p_rebuild import read_image
from rompager_map import discover

class ScriptParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.sources=[]
        self.frames=[]
    def handle_starttag(self,tag,attrs):
        if tag=='script':
            source=dict(attrs).get('src')
            if source:self.sources.append(source)
        if tag in ('frame','frameset','iframe'):self.frames.append(tag)

class NativeEntryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build=ROOT/'build'
        cls.site=cls.build/'served-native-ui'
        cls.result=json.loads((cls.build/'VALIDATION.json').read_text())
        cls.original=(cls.site/'index.htm').read_text('ascii')
        cls.parser=ScriptParser();cls.parser.feed(cls.original)
        image=read_image((cls.build/'H910P_01.18.0005_INSTRUMENT_PANEL_EXPERIMENTAL.bin').read_bytes())
        _,rr=discover(image.unpacked)
        cls.table={d['url']:d for d in rr}

    def test_index_fits_native_static_slots(self):
        # Original /index.htm consists of two zero-terminated string slots.
        self.assertEqual(len(self.original),713)
        self.assertEqual(self.result['native_index_script_count'], 1)
        self.assertGreaterEqual(len(self.result['repurposed_original_help_pages']), 18)

    def test_all_asset_chunks_precede_bootstrap(self):
        names=self.result['repurposed_original_help_pages']
        self.assertEqual(self.parser.sources,names[:1])
        self.assertEqual(self.parser.frames,[])
        self.assertIn('<main id=app>',self.original)

    def test_all_scripts_have_javascript_mime(self):
        for path in self.result['repurposed_original_help_pages']:
            with self.subTest(path=path):
                self.assertEqual(self.table[path]['mime_enum'],21)
                self.assertTrue((self.site/path.lstrip('/')).is_file())

    def test_no_missing_pack_initialization(self):
        joined='\n'.join((self.site/path.lstrip('/')).read_text('ascii') for path in self.result['repurposed_original_help_pages'])
        for path in self.result['repurposed_original_help_pages'][1:]:
            self.assertIn(path,joined, 'Bootstrap must dynamically load '+path)
        self.assertIn('window.H910P_PACK="";',joined)
        self.assertIn('new DecompressionStream("gzip")',joined)
        self.assertGreater(len(re.findall(r'window.H910P_PACK\+=',joined)),50)

if __name__=='__main__':unittest.main(verbosity=2)
