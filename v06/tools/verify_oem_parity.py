#!/usr/bin/env python3
"""Offline-only reference mapping: legacy XML capture -> typed JSON test values.

This is a development validation tool, NOT an embedded XML driver. Raw source
files might contain secrets; only selected numeric UPS measurements are read.
The result intentionally excludes all configured System values and identifiers.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import xml.etree.ElementTree as ET

MAX_INPUT = 128 * 1024
FIELDS = {
    'UPS.PowerSummary.RemainingCapacity': ('batteryPercent', 0, 100),
    'UPS.PowerSummary.PercentLoad': ('loadPercent', 0, 200),
    'UPS.PowerSummary.RunTimeToEmpty': ('runtimeSeconds', 0, 604800),
    'UPS.PowerConverter.Input[1].Voltage': ('inputVolts', 0, 300),
    'UPS.PowerConverter.Output.Voltage': ('outputVolts', 0, 300),
    'UPS.PowerSummary.PresentStatus.ACPresent': ('acPresent', 0, 1),
    'UPS.BatterySystem.Battery.PresentStatus.Present': ('batteryPresent', 0, 1),
    'UPS.OutletSystem.Outlet[2].PresentStatus.SwitchOnOff': ('segment1On', 0, 1),
    'UPS.OutletSystem.Outlet[3].PresentStatus.SwitchOnOff': ('segment2On', 0, 1),
}

class ParityError(ValueError):
    pass


def decode(xml_bytes: bytes) -> dict[str, int]:
    if len(xml_bytes) > MAX_INPUT or not xml_bytes:
        raise ParityError('invalid capture size')
    if b'<!DOCTYPE' in xml_bytes.upper() or b'<!ENTITY' in xml_bytes.upper():
        raise ParityError('XML entities / DOCTYPE forbidden')
    try:
        root = ET.fromstring(xml_bytes)
    except ET.ParseError as exc:
        raise ParityError('malformed capture') from exc
    if root.tag != 'GET_OBJECT':
        raise ParityError('unexpected OEM response type')
    result: dict[str, int] = {}
    requested = set(FIELDS)
    for element in root:
        if element.tag != 'OBJECT':
            continue
        name = element.get('name', '')
        if name not in requested:
            continue
        key, minimum, maximum = FIELDS[name]
        if key in result:
            raise ParityError(f'duplicate required field {key}')
        raw = (element.text or '').strip()
        if not raw or not raw.isascii() or raw.lstrip('-').isdecimal() is False:
            raise ParityError(f'noninteger value in {key}')
        value = int(raw)
        if value < minimum or value > maximum:
            raise ParityError(f'out-of-range value in {key}')
        result[key] = value
    missing = [name for name, (key, _, _) in FIELDS.items() if key not in result]
    if missing:
        raise ParityError('missing required numeric UPS fields: ' + ', '.join(missing))
    result['inputDecivolts'] = result.pop('inputVolts') * 10
    result['outputDecivolts'] = result.pop('outputVolts') * 10
    return result


def main() -> int:
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('capture', type=Path)
    args = a.parse_args()
    try:
        if not args.capture.is_file() or args.capture.stat().st_size > MAX_INPUT:
            raise ParityError('missing or oversized input')
        result = decode(args.capture.read_bytes())
        print(json.dumps({'schemaVersion': 1, 'offlineOnly': True,
                          'hardwareProtocolDiscovered': False,
                          'readings': result}, indent=2, sort_keys=True))
    except (OSError, ParityError) as exc:
        print('OFFLINE REFERENCE REJECTED:', exc)
        return 1
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
