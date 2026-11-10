#!/usr/bin/env python3
"""Validate the real firmware's parser-ordered script loading contract.

The v0.5.3-v0.5.5 dynamic resource loader was never verified on hardware;
this version uses the same static script ordering as the working v0.5.2 UI.
"""
from html.parser import HTMLParser
from pathlib import Path
import json
import re
import sys
import unittest

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
        cls.original=(cls.site/'default.htm').read_text('ascii')
        cls.parser=ScriptParser();cls.parser.feed(cls.original)
        image=read_image((cls.build/'H910P_01.18.0005_INSTRUMENT_PANEL_EXPERIMENTAL.bin').read_bytes())
        _,rr=discover(image.unpacked)
        cls.table={d['url']:d for d in rr}

    def test_index_fits_native_static_slots(self):
        self.assertLessEqual(len(self.original),672)
        self.assertEqual(self.table['/default.htm']['mime_enum'],self.table['/index.htm']['mime_enum'])
        self.assertEqual(self.result['native_index_script_count'],len(self.result['repurposed_original_help_pages']))
        self.assertGreaterEqual(len(self.parser.sources),14)

    def test_all_asset_chunks_precede_bootstrap(self):
        names=self.result['repurposed_original_help_pages']
        self.assertEqual(self.parser.sources,[x.lstrip('/') for x in names[1:]+names[:1]])
        self.assertEqual(self.parser.frames,[])
        self.assertIn('<main id=a>',self.original)
        self.assertIn('<main id=a>',self.original)

    def test_all_scripts_have_javascript_mime(self):
        for path in self.result['repurposed_original_help_pages']:
            with self.subTest(path=path):
                self.assertEqual(self.table[path]['mime_enum'],21)
                self.assertTrue((self.site/path.lstrip('/')).is_file())

    def test_no_network_dependent_loader(self):
        bootstrap=self.site/self.result['repurposed_original_help_pages'][0].lstrip('/')
        source=bootstrap.read_text('ascii')
        self.assertIn('document.querySelector("main").id="app";',source)
        self.assertNotIn('el.onload',source)
        self.assertNotIn('el.src=',source)
        self.assertNotIn('await new Promise',source)
        self.assertIn('Incomplete assets (',source)
        self.assertIn('new DecompressionStream("gzip")',source)
        joined='\n'.join((self.site/path.lstrip('/')).read_text('ascii') for path in self.result['repurposed_original_help_pages'])
        self.assertIn('window.H910P_PACK="";',joined)
        self.assertGreater(len(re.findall(r'window.H910P_PACK\+=',joined)),50)

if __name__=='__main__':unittest.main(verbosity=2)
