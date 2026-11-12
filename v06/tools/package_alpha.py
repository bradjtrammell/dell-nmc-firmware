#!/usr/bin/env python3
"""Package native H910P ARM7 code in the verified NET+OS envelope.

Laboratory artifact only: neither boot-qualified nor safe to flash.
No network access or hardware write actions.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

HEADER = struct.Struct('>II8sIIIII')
HEADER_SIZE = HEADER.size
MAGIC = b'bootHdr\0'
MAX_VENDOR_IMAGE = 1517029
MAX_APPLICATION = 1_200_000
POLY = 0x04C11DB7


def crc_netos(blob: bytes) -> int:
    crc = 0
    for value in blob:
        crc ^= value << 24
        for _ in range(8):
            crc = ((crc << 1) ^ (POLY if crc & 0x80000000 else 0)) & 0xffffffff
    return crc


def literal_lzss(data: bytes) -> bytes:
    """LZSS2-compatible literal-only output; no proprietary encoder required."""
    out = bytearray()
    for start in range(0, len(data), 8):
        part = data[start:start + 8]
        out.append((1 << len(part)) - 1)
        out.extend(part)
    return bytes(out)


def unpack_lzss(data: bytes, max_output: int = MAX_APPLICATION) -> bytes:
    ring = bytearray(b' ' * 4096)
    write = 0xfee
    result = bytearray()
    i = 0
    while i < len(data):
        flags = data[i]
        i += 1
        for bit in range(8):
            if i == len(data):
                return bytes(result)
            if flags & (1 << bit):
                c = data[i]
                i += 1
                result.append(c)
                ring[write] = c
                write = (write + 1) & 4095
            else:
                if i + 2 > len(data):
                    raise ValueError('Truncated LZSS back-reference')
                low, high = data[i], data[i + 1]
                i += 2
                offset = low | ((high & 0xf0) << 4)
                length = (high & 15) + 3
                for j in range(length):
                    c = ring[(offset + j) & 4095]
                    result.append(c)
                    ring[write] = c
                    write = (write + 1) & 4095
            if len(result) > max_output:
                raise ValueError('Unpacked image exceeds research bound')
    return bytes(result)


def check_arm_raw(data: bytes) -> None:
    if not 0x100 <= len(data) <= MAX_APPLICATION:
        raise ValueError('Unsupported native application length')
    if data[:4] != bytes.fromhex('e59ff038'):
        raise ValueError('Not the expected big-endian ARM7 reset vector')
    if data[4:0x40] != bytes.fromhex('e1a00000') * 15:
        raise ValueError('Reset-vector padding does not match native target')
    reset_addr, = struct.unpack_from('>I', data, 0x40)
    if not 0x08004060 <= reset_addr < 0x08005000 or reset_addr & 3:
        raise ValueError('Reset target outside native entry region')
    off = reset_addr - 0x08004000
    if off + 0x44 > len(data):
        raise ValueError('Truncated native ARM startup')
    if data[off:off + 12] != bytes.fromhex('e10f0000e200101fe3510010'):
        raise ValueError('Native startup privilege guard missing')
    if data[off + 16:off + 28] != bytes.fromhex('e3c0003fe38000d3e121f000'):
        raise ValueError('Native SVC/IRQ mask instructions missing')
    branch, = struct.unpack_from('>I', data, off + 12)
    if branch >> 24 != 0x0a:
        raise ValueError('Missing unprivileged handoff refusal branch')
    delta = branch & 0xffffff
    if delta & 0x800000:
        delta -= 1 << 24
    halt_at = off + 20 + (delta << 2)
    if halt_at < off + 28 or halt_at + 4 > len(data):
        raise ValueError('Refusal branch target outside guarded startup')
    if data[halt_at:halt_at + 4] != bytes.fromhex('eafffffe'):
        raise ValueError('No fail-closed boot-halt at refusal target')


def build(raw: bytes) -> tuple[bytes, dict]:
    check_arm_raw(raw)
    compressed = literal_lzss(raw)
    header = HEADER.pack(HEADER_SIZE, HEADER_SIZE, MAGIC, 2, 9,
                         0x00010000, 0x08004000, len(compressed))
    image_prefix = header + compressed
    crc = crc_netos(image_prefix)
    image = image_prefix + crc.to_bytes(4, 'big')
    if len(image) > MAX_VENDOR_IMAGE:
        raise ValueError('Image exceeds observed OEM update-image size limit')
    if crc_netos(image) != 0:
        raise ValueError('NET+OS CRC verification failed')
    if unpack_lzss(image[HEADER_SIZE:-4]) != raw:
        raise ValueError('Firmware compressed payload did not round-trip')
    a, b, magic, ver, flags, flash, ram, length = HEADER.unpack_from(image)
    if (a, b, magic, ver, flags, flash, ram, length) != (
        HEADER_SIZE, HEADER_SIZE, MAGIC, 2, 9, 0x10000, 0x08004000, len(compressed)
    ):
        raise ValueError('Firmware header did not round-trip')
    report = {
        'release': '0.6.0-alpha.1',
        'classification': 'RESEARCH_IMAGE_NOT_OPERATIONAL_NOT_FLASH_QUALIFIED',
        'firmwareHeader': 'NET+OS bootHdr v2 flags 9',
        'flashOffsetHeaderField': '0x00010000',
        'ramLoadHeaderField': '0x08004000',
        'imageBytes': len(image), 'imageSHA256': hashlib.sha256(image).hexdigest(),
        'unpackedBytes': len(raw), 'unpackedSHA256': hashlib.sha256(raw).hexdigest(),
        'compression': 'LZSS2-compatible literal stream', 'netosCRCValid': True,
        'exactNativePayloadRoundTrip': True,
        'provenUploaderEnvelope': True,
        'independentBootVerified': False,
        'ethernetDriverImplemented': False,
        'upsDriverImplemented': False,
        'hardwareRecoveryVerified': False,
        'safeToInstall': False,
    }
    return image, report


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('native_raw', type=Path)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--acknowledge-research-only', action='store_true', required=True)
    args = p.parse_args(argv)
    try:
        if args.output.suffix != '.bin' or args.output == args.manifest:
            raise ValueError('Expected distinct .bin artifact and JSON manifest')
        if args.output.exists() or args.manifest.exists():
            raise ValueError('Refusing to overwrite existing research artifacts')
        if not args.output.parent.is_dir() or not args.manifest.parent.is_dir():
            raise ValueError('Artifact parent directory does not exist')
        image, report = build(args.native_raw.read_bytes())
        args.output.write_bytes(image)
        args.manifest.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n', encoding='utf-8')
        print(f'RESEARCH BIN: {args.output} ({len(image)} bytes, CRC valid)')
        print('NOT SAFE TO FLASH: no native H910P drivers/recovery qualification')
        return 0
    except (OSError, ValueError) as exc:
        print(f'REFUSED: {exc}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
