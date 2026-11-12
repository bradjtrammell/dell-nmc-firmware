#!/usr/bin/env python3
"""Offline-only original OEM application vocabulary evidence for UPS link.

Usage: python3 v06/tools/probe_oem_transport.py originals/NMC_*.zip
Never performs hardware I/O, never produces firmware, never exports OEM code.
Only fingerprints expected textual markers in integrity-checked OEM images.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from h910p_rebuild import read_image, ValidationError  # noqa: E402

KNOWN = {
    'df6471c61126fb95b393f6c91006d136d0e63612f19ae2f5d43f33fbaef932ba': '01.16.0001',
    'ed5a8eb47ae5bdf72817286d22fccd06b8a68381194f461f7fb271325b1a6a6a': '01.18.0005',
}

# These are symbol/string *clues*, NOT proof of physical bus type.
MARKERS = (
    b'threadBinding',
    b'shutHostGetReport',
    b'shutHostSetReport',
    b'COPI_READ',
    b'BDD',
    b'SendStringBufferToUPS',
    b'SendNMCFirmwareToUPS',
    b'UPS File Transfer Protocol',
    b'NO_REPLY_EVT',
    b'NO_DATA_EVT',
    b'GetReport',
    b'SetReport',
    b'HID',
)


def analyze_application(app: bytes) -> dict:
    if not app or len(app) > 32_000_000:
        raise ValueError('invalid unpacked application length')
    markers = []
    for token in MARKERS:
        offsets = []
        cursor = 0
        while True:
            offset = app.find(token, cursor)
            if offset < 0:
                break
            if len(offsets) >= 10000:
                raise ValueError('unreasonable marker repetition')
            offsets.append(offset)
            cursor = offset + len(token)
        markers.append({'marker': token.decode('ascii'), 'hits': len(offsets),
                        'firstOffset': f'0x{offsets[0]:x}' if offsets else None})
    return {'decompressedBytes': len(app), 'markers': markers,
            'interpretation': 'OEM application string references only; transport, packet layout and physical wiring unconfirmed'}


def inspect_archive(path: Path) -> dict:
    with zipfile.ZipFile(path) as archive:
        members = [x for x in archive.infolist() if x.filename.lower().endswith('.bin')]
        if len(members) != 1 or members[0].file_size > 4_000_000:
            raise ValueError('expected exactly one bounded vendor BIN')
        name = members[0].filename
        if name.startswith('/') or '..' in Path(name).parts:
            raise ValueError('unsafe archive path')
        image_bytes = archive.read(name)
    digest = hashlib.sha256(image_bytes).hexdigest()
    if digest not in KNOWN:
        raise ValueError('unknown OEM BIN SHA-256; refusing analysis')
    image = read_image(image_bytes)  # validates NET+OS header, CRC and LZSS2
    report = analyze_application(image.unpacked)
    report.update({'release': KNOWN[digest], 'originalImageSHA256': digest,
                   'originalImageBytes': len(image_bytes),
                   'mainFlashOffsetField': f'0x{image.flash_offset:08x}',
                   'mainRamLoadField': f'0x{image.ram_address:08x}'})
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archives', type=Path, nargs='+')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        report = {'schemaVersion': 1, 'operation': 'OFFLINE-ONLY',
                  'disclaimer': 'No UPS protocol is decoded or implemented by this tool',
                  'releases': [inspect_archive(path) for path in args.archives]}
        text = json.dumps(report, indent=2, sort_keys=True) + '\n'
        if args.output:
            if args.output.exists():
                raise ValueError('refusing to overwrite existing report')
            args.output.write_text(text, encoding='utf-8')
        print(text, end='')
        return 0
    except (OSError, zipfile.BadZipFile, ValidationError, ValueError) as exc:
        print(f'REFUSED: {exc}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
