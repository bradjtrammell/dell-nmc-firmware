"""Synthetic fixture tests; must never require or write NMC credentials."""
from __future__ import annotations
import importlib.util
from pathlib import Path
import tempfile
import unittest

MODULE = Path(__file__).parent / "tools" / "extract_live_contract.py"
spec = importlib.util.spec_from_file_location("live_contract", MODULE)
mod = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(mod)


class ContractTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "product.xml").write_text('<PRODUCT_INFO name="SN-PRIVATE"><GET_OBJECT mode="POST" url="/secret-path"/></PRODUCT_INFO>')
        (self.root / "descriptor.xml").write_text('''<SYSTEM_CONFIG><GROUP id="Network"><ITEM object="System.Network.Dhcp" access="RW" type="Boolean"/><ITEM object="System.Password" access="RO" type="String:20"/></GROUP></SYSTEM_CONFIG>''')
        (self.root / "ups.xml").write_text('''<GET_OBJECT><OBJECT name="UPS.PowerSummary.iModel" value="PRIVATE_DEVICE_SERIAL"/><OBJECT name="UPS.PowerSummary.RemainingCapacity"><VALUE>12345</VALUE></OBJECT></GET_OBJECT>''')

    def test_metadata_and_no_values(self):
        import json
        result = mod.build_contract(self.root)
        self.assertEqual(result["counts"]["groups"], 1)
        self.assertEqual(result["counts"]["configurationItems"], 2)
        self.assertEqual(result["counts"]["telemetryObjects"], 2)
        self.assertEqual(result["counts"]["access"], {"RO": 1, "RW": 1})
        dumped = json.dumps(result)
        self.assertNotIn("PRIVATE", dumped)
        self.assertNotIn("12345", dumped)
        self.assertNotIn("secret-path", dumped)
        self.assertIn("System.Password", dumped)

    def test_preserves_oem_enum_and_conditional_range_expressions(self):
        (self.root / "descriptor.xml").write_text('''<SYSTEM_CONFIG><GROUP id="Environment.Temperature"><ITEM object="Environment.Temperature.Unit" access="RW" type="List:C,F:$TUnit"/><ITEM object="Environment.Temperature.HighThreshold" access="RW" type="Integer:$TLow..70:$THigh"/><ITEM object="System.Schedule[0].On" access="RW" type="Time:-1..86399/1800"/></GROUP></SYSTEM_CONFIG>''')
        schema = mod.build_contract(self.root)
        types = [field["type"] for field in schema["configurationGroups"][0]["items"]]
        self.assertEqual(types, ["List:C,F:$TUnit", "Integer:$TLow..70:$THigh", "Time:-1..86399/1800"])

    def test_rejects_untrusted_type_expression(self):
        (self.root / "descriptor.xml").write_text('''<SYSTEM_CONFIG><GROUP id="Network"><ITEM object="System.A" access="RW" type="String:<script>alert(1)</script>"/></GROUP></SYSTEM_CONFIG>''')
        with self.assertRaises(mod.ContractError):
            mod.build_contract(self.root)

    def test_reports_coverage_and_hides_legacy_groups_without_values(self):
        (self.root / "descriptor.xml").write_text('''<SYSTEM_CONFIG><GROUP id="Control.Outlet[1]" hidden="TRUE"><ITEM object="System.A" type="Integer:1..5" unit="s" access="WO" restart="TRUE" /></GROUP></SYSTEM_CONFIG>''')
        (self.root / "ups.xml").write_text('''<GET_OBJECT><OBJECT name="System.A">PRIVATE</OBJECT><OBJECT name="UPS.PowerSummary.iModel">SERIAL_SECRET</OBJECT></GET_OBJECT>''')
        result = mod.build_contract(self.root)
        self.assertTrue(result["configurationGroups"][0]["hiddenInOEM"])
        self.assertEqual(result["configurationGroups"][0]["items"][0]["unit"], "s")
        self.assertEqual(result["counts"]["coverage"], {
            "descriptorItemsObserved": 1, "descriptorItemsMissing": 0,
            "additionalObservedObjects": 1,
            "observedNamePrefixes": {"System": 1, "UPS": 1},
        })
        self.assertNotIn("PRIVATE", str(result))
        self.assertNotIn("SERIAL_SECRET", str(result))

    def test_duplicate_fields_rejected(self):
        (self.root / "descriptor.xml").write_text('''<SYSTEM_CONFIG><GROUP id="Network"><ITEM object="System.A" access="RW"/><ITEM object="System.A" access="RW"/></GROUP></SYSTEM_CONFIG>''')
        with self.assertRaisesRegex(mod.ContractError, "Duplicate"):
            mod.build_contract(self.root)

    def test_bad_root_rejected(self):
        (self.root / "ups.xml").write_text('<html/>')
        with self.assertRaisesRegex(mod.ContractError, "Incorrect XML root"):
            mod.build_contract(self.root)

    def test_oversized_file_rejected(self):
        (self.root / "ups.xml").write_bytes(b"x" * (mod.MAX_XML + 1))
        with self.assertRaisesRegex(mod.ContractError, "exceeds"):
            mod.build_contract(self.root)

    def test_untrusted_field_rejected(self):
        (self.root / "ups.xml").write_text('<GET_OBJECT><OBJECT name="UPS.Value|SECRET" value="42"/></GET_OBJECT>')
        with self.assertRaisesRegex(mod.ContractError, "Unexpected telemetry"):
            mod.build_contract(self.root)


if __name__ == "__main__":
    unittest.main()