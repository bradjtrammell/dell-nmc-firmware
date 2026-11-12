"""Validation of purely offline OEM ARM7 xref research. Synthetic data only."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

FILE = Path(__file__).resolve().parents[1] / 'tools' / 'analyze_oem_arm.py'
spec = importlib.util.spec_from_file_location('analyze_oem_arm', FILE)
assert spec is not None and spec.loader is not None
research = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = research
spec.loader.exec_module(research)
BASE = 0x08004000


def fixture() -> bytes:
    """An ARM-state LDR literal references only allowlisted synthetic text."""
    raw = bytearray(256)
    struct.pack_into('>I', raw, 0, 0xE59F0000)  # LDR r0, [PC]; slot +8
    struct.pack_into('>I', raw, 8, BASE + 64)
    raw[64:64 + len(b'shutHostGetReport')] = b'shutHostGetReport'
    struct.pack_into('>I', raw, 16, 0xE59F1000)  # LDR r1, [PC]; slot +24
    struct.pack_into('>I', raw, 24, 0xFFC00010)
    struct.pack_into('>I', raw, 32, 0xEB000001)  # BL +4, source 32 -> 44
    return bytes(raw)


class OEMArmForensicTests(unittest.TestCase):
    def test_arm_ldr_pc_positive_and_negative(self):
        self.assertEqual(research.arm_literal(0xE59F0000, BASE), (BASE + 8, 0))
        self.assertEqual(research.arm_literal(0xE51F200C, BASE + 16), (BASE + 12, 2))
        self.assertIsNone(research.arm_literal(0xE1A00000, BASE))
        self.assertIsNone(research.arm_literal(0xE5DF0000, BASE))  # byte
        self.assertIsNone(research.arm_literal(0xE79F0000, BASE))  # register offset
        self.assertIsNone(research.arm_literal(0xF59F0000, BASE))  # NV
        self.assertIsNone(research.arm_literal(0xE58F0000, BASE))  # store
        self.assertIsNone(research.arm_literal(0xE59F0000, 0xFFFFFFFF))

    def test_arm_branch_and_sign_extension(self):
        self.assertEqual(research.arm_branch(0xEB000001, BASE + 32), ('BL', BASE + 44))
        self.assertEqual(research.arm_branch(0xEAFFFFFE, BASE + 32), ('B', BASE + 32))
        self.assertIsNone(research.arm_branch(0xFB000001, BASE))  # BLX
        self.assertIsNone(research.arm_branch(0xE1A00000, BASE))

    def test_non_utf8_data_never_emitted(self):
        raw = bytearray(fixture())
        raw[200:224] = b'not-a-real-password-here'
        result = research.analyze_application(bytes(raw))
        rendered = json.dumps(result)
        self.assertNotIn('password', rendered)
        self.assertNotIn('not-a-real', rendered)
        self.assertIn('shutHostGetReport', rendered)
        self.assertFalse(result['assessment']['flashableFirmwareGenerated'])

    def test_exact_string_literal_references(self):
        result = research.analyze_application(fixture())
        refs = {entry['label']: entry for entry in result['labelReferences']}
        ref = refs['shutHostGetReport']
        self.assertEqual(ref['stringHits'], 1)
        self.assertEqual(ref['firstStringOffset'], '0x00000040')
        self.assertEqual(ref['literalReferenceCandidates'], 1)
        self.assertEqual(ref['references'][0]['instructionAddress'], '0x08004000')
        self.assertEqual(ref['references'][0]['targetAddress'], '0x08004040')
        self.assertEqual(ref['references'][0]['destinationRegister'], 0)

    def test_mmio_candidates_are_not_proven_hardware(self):
        result = research.analyze_application(fixture())
        regs = result['possibleMMIOReferences']
        self.assertEqual(len(regs['NS7520_CS_REGISTER_CANDIDATE']), 1)
        self.assertEqual(regs['NS7520_CS_REGISTER_CANDIDATE'][0]['candidateRegisterValue'], '0xffc00010')
        self.assertEqual(regs['HIGH_IO_ADDRESS_CANDIDATE'], [])
        self.assertFalse(result['assessment']['h910pHardwareMappingProven'])
        self.assertFalse(result['assessment']['upsPhysicalBusIdentified'])

    def test_reject_bad_load_or_length(self):
        for raw in (b'', b'x' * 65, b'x' * 60):
            with self.subTest(raw_length=len(raw)), self.assertRaises(ValueError):
                research.analyze_application(raw)
        with self.assertRaises(ValueError):
            research.analyze_application(fixture(), 0x08004002)
        with self.assertRaises(ValueError):
            research.analyze_application(fixture(), 0xffffff80)

    def test_detect_header_with_strong_validation(self):
        self.assertEqual(research.find_recovery_images(fixture()), [])
        raw = bytearray(256)
        raw[32:40] = b'bootHdr\0'
        self.assertEqual(research.find_recovery_images(bytes(raw)), [])

    def test_embedded_recovery_crc_and_lzss_roundtrip(self):
        from h910p_rebuild import HEADER_LEN, HEADER_STRUCT, crc32_netos
        raw_reset = bytes.fromhex('e59ff038e1a00000')
        compressed = b'\xff' + raw_reset  # one 8-byte literal block
        header = HEADER_STRUCT.pack(HEADER_LEN, HEADER_LEN, b'bootHdr\0',
                                    3, 9, 0, 0x00404000, len(compressed))
        prefix = header + compressed
        image = prefix + crc32_netos(prefix).to_bytes(4, 'big')
        app = bytearray(256)
        app[64:64 + len(image)] = image
        report = research.find_recovery_images(bytes(app))
        self.assertEqual(len(report), 1)
        self.assertTrue(report[0]['crcValid'])
        self.assertTrue(report[0]['decompressionVerified'])
        self.assertTrue(report[0]['armResetVectorMatch'])
        self.assertEqual(report[0]['decompressedBytes'], len(raw_reset))
        self.assertFalse(report[0]['matchesPreviouslyObservedRecoveryExecutable'])
        # Corruption must not be reported as a validated embedded image.
        app[64 + HEADER_LEN] ^= 1
        self.assertEqual(research.find_recovery_images(bytes(app)), [])

    def test_reference_report_bound_and_base_offset(self):
        raw = bytearray(fixture())
        struct.pack_into('>I', raw, 8, BASE + 63)
        result = research.analyze_application(bytes(raw))
        refs = {entry['label']: entry for entry in result['labelReferences']}
        self.assertEqual(refs['shutHostGetReport']['literalReferenceCandidates'], 1)

    def test_compare_cross_release(self):
        a = research.analyze_application(fixture())
        a['vendorFirmwareRelease'] = '01.16.0001'
        b = research.analyze_application(fixture())
        b['vendorFirmwareRelease'] = '01.18.0005'
        comparison = research.compare_releases([a, b])
        self.assertEqual(comparison['markerOffsetDeltasBytes']['shutHostGetReport'], 0)
        self.assertEqual(comparison['matchingEmbeddedRecoveryImageSHA256'], [])
        self.assertIsNone(research.compare_releases([a]))

    def test_strict_sha_allowlist(self):
        with patch.object(research, 'load_zip', return_value=('fake.bin', b'invalid-image')):
            with self.assertRaisesRegex(ValueError, 'Unknown OEM BIN SHA-256'):
                research.inspect_archive(Path('untrusted.zip'))

    def test_output_rejects_existing_file_without_overwrite(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'report.json'
            output.write_text('protected')
            self.assertEqual(research.main(['unused.zip', '--output', str(output)]), 2)
            self.assertEqual(output.read_text(), 'protected')

    def test_cli_rejects_duplicate_archive(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'report.json'
            self.assertEqual(research.main(['same.zip', 'same.zip', '--output', str(output)]), 2)
            self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
