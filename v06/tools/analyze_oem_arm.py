#!/usr/bin/env python3
"""Audit independently supplied H910P OEM images for ARM7 hardware-link evidence.

Offline-only, SHA-allowlisted input. The tool exports addresses, references and
counts, never proprietary binary bytes or firmware images. Static ARM scanning
is heuristic: it does NOT establish an electrical protocol, flash layout or
working bootloader. It cannot access hardware, change power or flash a card.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from h910p_rebuild import (  # noqa: E402
    HEADER_LEN, HEADER_STRUCT, ValidationError, crc32_netos, load_zip,
    read_image, unpack_lzss2,
)

KNOWN = {
    'df6471c61126fb95b393f6c91006d136d0e63612f19ae2f5d43f33fbaef932ba': '01.16.0001',
    'ed5a8eb47ae5bdf72817286d22fccd06b8a68381194f461f7fb271325b1a6a6a': '01.18.0005',
}
EXPECTED_RECOVERY_SHA256 = '2083936d3217efd8fd7bb71b8612fbd5a07ef513564b175b01e699b603f3214f'
MAX_APPLICATION = 32_000_000
MAX_FINDINGS = 120
MAX_CANDIDATES = 128

# We only inspect known, already-observed labels; never serialize random OEM
# strings, passwords, device configuration or proprietary instruction bytes.
LABELS = (
    b'threadBinding', b'shutHostGetReport', b'shutHostSetReport',
    b'COPI_READ', b'NO_REPLY_EVT', b'NO_DATA_EVT',
    b'SendStringBufferToUPS', b'SendNMCFirmwareToUPS',
    b'UPS File Transfer Protocol', b'GetReport', b'SetReport',
    b'HID', b'bootHdr',
)


def hx(value: int) -> str:
    return f'0x{value:08x}'


def u32(blob: bytes, offset: int) -> int:
    return struct.unpack_from('>I', blob, offset)[0]


def sign_extend24(value: int) -> int:
    return value - (1 << 24) if value & (1 << 23) else value


def arm_literal(word: int, address: int) -> tuple[int, int] | None:
    """Decode ARM-state immediate, pre-indexed LDR (not register-offset/byte)."""
    if word >> 28 == 0xF or (word & 0x0E500000) != 0x04100000:
        return None
    # P=1, I=0, B=0, W=0, L=1, Rn=PC; reject conditional NV.
    if not (word & (1 << 24)) or word & (1 << 22) or word & (1 << 21):
        return None
    if ((word >> 16) & 0xF) != 15:
        return None
    offset = word & 0xFFF
    literal = address + 8 + (offset if word & (1 << 23) else -offset)
    if literal < 0 or literal > 0xFFFFFFFF:
        return None
    return literal, (word >> 12) & 0xF


def arm_branch(word: int, address: int) -> tuple[str, int] | None:
    """Decode ARM-state branch/BL, not BLX. No control-flow claims."""
    if word >> 28 == 0xF or (word & 0x0E000000) != 0x0A000000:
        return None
    delta = sign_extend24(word & 0xFFFFFF) << 2
    target = (address + 8 + delta) & 0xFFFFFFFF
    return ('BL' if word & 0x01000000 else 'B'), target


def marker_locations(blob: bytes) -> dict[str, list[int]]:
    locations: dict[str, list[int]] = {}
    for label in LABELS:
        places = []
        start = 0
        while len(places) < MAX_CANDIDATES:
            at = blob.find(label, start)
            if at == -1:
                break
            places.append(at)
            start = at + len(label)
        locations[label.decode('ascii')] = places
    return locations


def classify_mmio(value: int) -> str | None:
    if value == 0xFFC00000:
        return 'NS7520_MMCR_CANDIDATE'
    if 0xFFC00010 <= value <= 0xFFC00058 and (value - 0xFFC00010) % 4 == 0:
        if ((value - 0xFFC00010) % 16) in (0, 4, 8):
            return 'NS7520_CS_REGISTER_CANDIDATE'
    if 0xFF000000 <= value <= 0xFFFFFFFF:
        return 'HIGH_IO_ADDRESS_CANDIDATE'
    return None


def find_recovery_images(blob: bytes) -> list[dict[str, Any]]:
    """Inspect bounded, embedded NET+OS bootHdr images (no file extraction)."""
    findings: list[dict[str, Any]] = []
    seek = 0
    while True:
        marker = blob.find(b'bootHdr\0', seek)
        if marker == -1:
            break
        seek = marker + 8
        start = marker - 8
        if start < 0 or start + HEADER_LEN + 4 > len(blob):
            continue
        a, b, magic, version, flags, flash, ram, length = HEADER_STRUCT.unpack_from(blob, start)
        if a != HEADER_LEN or b != HEADER_LEN or magic != b'bootHdr\0':
            continue
        if version not in (2, 3) or flags != 9 or length > 4_000_000:
            continue
        total = HEADER_LEN + length + 4
        if start + total > len(blob):
            continue
        raw = blob[start:start + total]
        if crc32_netos(raw) != 0:
            continue
        report: dict[str, Any] = {
            'applicationOffset': hx(start),
            'imageLength': total,
            'imageSHA256': hashlib.sha256(raw).hexdigest(),
            'headerVersion': version,
            'flags': flags,
            'flashOffsetField': hx(flash),
            'ramAddressField': hx(ram),
            'crcValid': True,
            'matchesPreviouslyObservedRecoveryExecutable': hashlib.sha256(raw).hexdigest() == EXPECTED_RECOVERY_SHA256,
        }
        try:
            uncompressed = unpack_lzss2(raw[HEADER_LEN:-4])
            report['decompressedBytes'] = len(uncompressed)
            report['armResetVectorMatch'] = uncompressed.startswith(bytes.fromhex('e59ff038e1a00000'))
        except ValidationError:
            report['decompressionVerified'] = False
        else:
            report['decompressionVerified'] = True
        findings.append(report)
        if len(findings) >= 16:
            break
    return findings


def analyze_application(blob: bytes, load_address: int = 0x08004000) -> dict[str, Any]:
    """Scan ARM-state words, literal pools, and string references.

    Generic ARM instruction patterns embedded in strings/data can be false
    positives; results are intentionally labelled as candidate evidence only.
    """
    if len(blob) < 64 or len(blob) > MAX_APPLICATION or len(blob) % 4:
        raise ValueError('Expected bounded, word-aligned ARM application')
    if load_address & 3 or load_address > 0xFFFFFFFF - len(blob):
        raise ValueError('Invalid ARM application load address')
    labels = marker_locations(blob)
    candidates: dict[str, list[dict[str, Any]]] = {name: [] for name in labels}
    mmio: dict[str, list[dict[str, Any]]] = {
        'NS7520_MMCR_CANDIDATE': [],
        'NS7520_CS_REGISTER_CANDIDATE': [],
        'HIGH_IO_ADDRESS_CANDIDATE': [],
    }
    counts = {'ldrLiteralCandidates': 0, 'branches': 0, 'literalPointersIntoImage': 0, 'mmioLiteralCandidates': 0}
    # Index word-aligned strings by their exact offset and by interior +1..16
    # to accommodate references to static messages with leading prefixes.
    string_address_to_name: dict[int, list[tuple[str, int]]] = {}
    for name, positions in labels.items():
        for pos in positions:
            for start in range(max(0, pos - 16), pos + 1):
                key = load_address + start
                string_address_to_name.setdefault(key, []).append((name, pos))

    # Scan all words; no assumption that all bytes are executable ARM code.
    # Candidate reports cap size; full per-feature counters are retained.
    for offset in range(0, len(blob) - 3, 4):
        pc = load_address + offset
        word = u32(blob, offset)
        branch = arm_branch(word, pc)
        if branch:
            counts['branches'] += 1
        decoded = arm_literal(word, pc)
        if not decoded:
            continue
        literal_addr, rd = decoded
        literal_offset = literal_addr - load_address
        if literal_offset < 0 or literal_offset + 4 > len(blob) or literal_offset & 3:
            continue
        counts['ldrLiteralCandidates'] += 1
        value = u32(blob, literal_offset)
        if load_address <= value < load_address + len(blob):
            counts['literalPointersIntoImage'] += 1
            for name, string_offset in string_address_to_name.get(value, []):
                if len(candidates[name]) < MAX_FINDINGS:
                    candidates[name].append({
                        'instructionAddress': hx(pc),
                        'literalAddress': hx(literal_addr),
                        'targetLabelOffset': hx(string_offset),
                        'targetAddress': hx(value),
                        'destinationRegister': rd,
                        'confidence': 'candidate_string_reference_only',
                    })
        mmio_kind = classify_mmio(value)
        if mmio_kind:
            counts['mmioLiteralCandidates'] += 1
            if len(mmio[mmio_kind]) < MAX_FINDINGS:
                mmio[mmio_kind].append({
                    'instructionAddress': hx(pc),
                    'literalAddress': hx(literal_addr),
                    'candidateRegisterValue': hx(value),
                    'destinationRegister': rd,
                    'confidence': 'candidate_register_reference_only',
                })
    # Summarize references to exact strings and observed offsets, not raw
    # instruction words or OEM function bytes. A BL target is only a hint.
    label_reports = []
    for label, positions in labels.items():
        hits = candidates[label]
        label_reports.append({
            'label': label,
            'stringHits': len(positions),
            'firstStringOffset': hx(positions[0]) if positions else None,
            'literalReferenceCandidates': len(hits),
            'references': hits,
        })
    return {
        'schemaVersion': 1,
        'mode': 'OFFLINE_FORENSIC_RESEARCH_ONLY',
        'loadAddress': hx(load_address),
        'applicationLength': len(blob),
        'candidateInstructions': counts,
        'labelReferences': label_reports,
        'possibleMMIOReferences': mmio,
        'embeddedRecoveryImages': find_recovery_images(blob),
        'assessment': {
            'h910pHardwareMappingProven': False,
            'upsPhysicalBusIdentified': False,
            'usableProtocolDecoderAvailable': False,
            'onDeviceBootloaderRecoveryVerified': False,
            'flashableFirmwareGenerated': False,
            'warning': 'ARM patterns in data may be false positives. Literal/string xrefs are hints, NOT proven call graphs, register reads, UPS protocol, or physical flash geometry.',
        },
    }


def inspect_archive(path: Path) -> dict[str, Any]:
    _, original = load_zip(path)
    digest = hashlib.sha256(original).hexdigest()
    if digest not in KNOWN:
        raise ValueError('Unknown OEM BIN SHA-256; refusing forensic scan')
    parsed = read_image(original)
    report = analyze_application(parsed.unpacked, parsed.ram_address)
    report['vendorFirmwareRelease'] = KNOWN[digest]
    report['vendorFirmwareSHA256'] = digest
    report['flashOffsetHeaderField'] = hx(parsed.flash_offset)
    report['sourceArchiveName'] = path.name
    return report


def compare_releases(reports: list[dict[str, Any]]) -> dict[str, Any] | None:
    if len(reports) != 2:
        return None
    a, b = reports
    am = {x['label']: x for x in a['labelReferences']}
    bm = {x['label']: x for x in b['labelReferences']}
    deltas = {}
    for label in sorted(am.keys() & bm.keys()):
        aa, bb = am[label]['firstStringOffset'], bm[label]['firstStringOffset']
        if aa is not None and bb is not None:
            deltas[label] = int(bb, 16) - int(aa, 16)
    ar = {x['imageSHA256'] for x in a['embeddedRecoveryImages']}
    br = {x['imageSHA256'] for x in b['embeddedRecoveryImages']}
    return {
        'markerOffsetDeltasBytes': deltas,
        'matchingEmbeddedRecoveryImageSHA256': sorted(ar & br),
        'note': 'Identical embedded image bytes do not prove resident bootloader identity.',
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archives', type=Path, nargs='+')
    parser.add_argument('--output', type=Path, required=True, help='New JSON report path, must not exist')
    args = parser.parse_args(argv)
    try:
        if not 1 <= len(args.archives) <= 2:
            raise ValueError('Exactly one or two known OEM archives required')
        if len(set(args.archives)) != len(args.archives):
            raise ValueError('Duplicate OEM archives are not independent evidence')
        if args.output.exists():
            raise ValueError('Refusing to overwrite an existing report')
        reports = [inspect_archive(path) for path in args.archives]
        report = {
            'schemaVersion': 1,
            'operation': 'OFFLINE_RESEARCH_ONLY',
            'outputsContainVendorBytes': False,
            'releases': reports,
            'crossReleaseComparison': compare_releases(reports),
        }
        args.output.write_text(json.dumps(report, sort_keys=True, indent=2) + '\n', encoding='utf-8')
        print(f'SANITIZED: {args.output} ({len(reports)} verified OEM release(s))')
        return 0
    except (OSError, ValueError, ValidationError) as exc:
        print(f'REFUSED: {exc}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
