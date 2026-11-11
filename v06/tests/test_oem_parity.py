"""Tests isolated OEM reference mapper; no external devices or writes."""
import importlib.util
from pathlib import Path
import unittest

PATH = Path(__file__).resolve().parents[1] / 'tools' / 'verify_oem_parity.py'
spec = importlib.util.spec_from_file_location('oem_parity', PATH)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def capture(updates=None, omit=None):
    entries = {
        'UPS.PowerSummary.RemainingCapacity': '99',
        'UPS.PowerSummary.PercentLoad': '56',
        'UPS.PowerSummary.RunTimeToEmpty': '980',
        'UPS.PowerConverter.Input[1].Voltage': '119',
        'UPS.PowerConverter.Output.Voltage': '120',
        'UPS.PowerSummary.PresentStatus.ACPresent': '1',
        'UPS.BatterySystem.Battery.PresentStatus.Present': '1',
        'UPS.OutletSystem.Outlet[2].PresentStatus.SwitchOnOff': '1',
        'UPS.OutletSystem.Outlet[3].PresentStatus.SwitchOnOff': '0',
    }
    entries.update(updates or {})
    if omit:
        entries.pop(omit)
    payload = '<GET_OBJECT>' + ''.join(
        f'<OBJECT name="{name}">{value}</OBJECT>' for name, value in entries.items()
    ) + '<OBJECT name="System.Network.SmtpServer.Password">SECRET_TEST</OBJECT></GET_OBJECT>'
    return payload.encode('utf-8')


class ReferenceMappingTests(unittest.TestCase):
    def test_typed_units_and_no_secrets(self):
        mapped = module.decode(capture())
        self.assertEqual(mapped['inputDecivolts'], 1190)
        self.assertEqual(mapped['outputDecivolts'], 1200)
        self.assertEqual(mapped['batteryPercent'], 99)
        self.assertEqual(mapped['runtimeSeconds'], 980)
        self.assertNotIn('SECRET_TEST', repr(mapped))

    def test_missing_required_field_fails_closed(self):
        with self.assertRaises(module.ParityError):
            module.decode(capture(omit='UPS.PowerSummary.RemainingCapacity'))

    def test_out_of_range_rejected(self):
        with self.assertRaises(module.ParityError):
            module.decode(capture({'UPS.PowerSummary.PercentLoad': '9999'}))

    def test_duplicate_rejected(self):
        data = capture().replace(b'</GET_OBJECT>', b'<OBJECT name="UPS.PowerSummary.PercentLoad">55</OBJECT></GET_OBJECT>')
        with self.assertRaises(module.ParityError):
            module.decode(data)

    def test_doctype_rejected(self):
        with self.assertRaises(module.ParityError):
            module.decode(b'<!DOCTYPE bad [<!ENTITY e SYSTEM "file:///etc/passwd">]>' + capture())

    def test_malformed_root_rejected(self):
        with self.assertRaises(module.ParityError):
            module.decode(capture().replace(b'GET_OBJECT', b'BAD_OBJECT'))


if __name__ == '__main__':
    unittest.main()
