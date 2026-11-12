"""Offline-only original ROM vocabulary audit tests; no NMC access."""
from __future__ import annotations
import importlib.util
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'tools' / 'probe_oem_transport.py'
spec = importlib.util.spec_from_file_location('probe_oem_transport', SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class TransportVocabularyTests(unittest.TestCase):
    def test_distinct_marker_offsets(self):
        report = module.analyze_application(b'foo COPI_READ foo shutHostGetReport COPI_READ')
        markers = {m['marker']: m for m in report['markers']}
        self.assertEqual(markers['COPI_READ']['hits'], 2)
        self.assertEqual(markers['COPI_READ']['firstOffset'], '0x4')
        self.assertEqual(markers['shutHostGetReport']['hits'], 1)
        self.assertEqual(markers['HID']['hits'], 0)
        self.assertNotIn('foo COPI_READ', str(report))

    def test_unverified_image_rejected(self):
        with tempfile.TemporaryDirectory() as path:
            from zipfile import ZipFile
            archive = Path(path) / 'fake.zip'
            with ZipFile(archive, 'w') as z:
                z.writestr('fake.bin', b'not signed vendor firmware')
            with self.assertRaises(ValueError):
                module.inspect_archive(archive)

    def test_reject_unbounded_application(self):
        with self.assertRaises(ValueError):
            module.analyze_application(b'')
        with self.assertRaises(ValueError):
            module.analyze_application(b'a' * 32_000_001)


if __name__ == '__main__':
    unittest.main()
