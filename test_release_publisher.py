#!/usr/bin/env python3
"""Safety checks for the GitHub release publisher; no GitHub calls."""
from __future__ import annotations
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parent/'tools'))
import publish_release

ROOT=Path(__file__).resolve().parent
IMAGE=ROOT/'H910P_Custom_UI_v0.5.10.bin'
SHA='e3adea74aad966c15e87f9fdb7407bb9d9d810e20158da54bad639466f70b8bb'

@unittest.skipUnless(IMAGE.exists(),'Reference v0.5.10 BIN required from engineering package')
class PublicationTests(unittest.TestCase):
    def test_main_requires_hardware_gate_and_plans_exact_asset(self):
        with tempfile.TemporaryDirectory() as d:
            notes=Path(d)/'notes.md';notes.write_text('Validated firmware release\n')
            with self.assertRaisesRegex(ValueError,'hardware approval'):
                publish_release.plan(IMAGE,SHA,'main',notes)
            plan=publish_release.plan(IMAGE,SHA,'main',notes,True)
            self.assertEqual(plan['tag'],'v0.5.10')
            self.assertFalse(plan['prerelease'])
            self.assertEqual(plan['size'],1517026)
            with self.assertRaisesRegex(ValueError,'Beta version'):
                publish_release.plan(IMAGE,SHA,'dev',notes,True)
    def test_rejects_empty_notes(self):
        with tempfile.TemporaryDirectory() as d:
            notes=Path(d)/'notes.md';notes.write_text('')
            with self.assertRaisesRegex(ValueError,'nonempty'):
                publish_release.plan(IMAGE,SHA,'main',notes,True)

if __name__=='__main__':unittest.main(verbosity=2)
