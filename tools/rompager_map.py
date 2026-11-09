#!/usr/bin/env python3
"""Read-only RomPager resource map for the two supplied Dell H910P images.

A *reference* is not proof that an object can be safely replaced. Produces
static segment exports only for provably contiguous zero-terminated item data.
Never modifies firmware or sends HTTP requests.
"""
from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import re
import struct

from h910p_rebuild import read_image, load_zip, sha256, ValidationError

RAM_BASE = 0x08004000
DATA_BASE = 0x00400000
URL_RX = re.compile(rb'/(?:(?:[a-zA-Z0-9_.-]+/){0,8})[a-zA-Z0-9_.-]{1,96}\.(?:htm|html|css|js|gif|jpg|png|ico|xml|cgi)\x00', re.I)


def u32(blob: bytes, where: int) -> int:
    return struct.unpack_from('>I', blob, where)[0]


def _find_descriptor(blob: bytes, url_offset: int) -> tuple[int, list[int]] | None:
    pointer = struct.pack('>I', RAM_BASE + url_offset)
    seek = 0
    while True:
        where = blob.find(pointer, seek)
        if where < 0:
            return None
        seek = where + 1
        if where % 4 or where < 0x100000 or where + 28 > len(blob):
            continue
        fields = list(struct.unpack_from('>7I', blob, where))
        # fItemsArrayPtr in dynamic data section, MIME type and cache control reasonable
        if DATA_BASE <= fields[1] < 0x00600000 and fields[5] <= 64 and fields[6] <= 2:
            return where, fields


def discover(blob: bytes) -> tuple[int, list[dict]]:
    known = {}
    for match in URL_RX.finditer(blob):
        path = match.group()[:-1].decode('ascii')
        possible = _find_descriptor(blob, match.start())
        if possible:
            known[path] = possible
    if '/index.htm' not in known or '/html/mgeweb.css' not in known:
        raise ValidationError('Cannot establish main RomPager resource table')
    # Common descriptor layout: 7 x u32, then item list for static pages.
    bases = []
    for name in ['/index.htm','/menu.htm','/html/mgeweb.css','/html/utils.js']:
        if name in known:
            off, fields = known[name]
            bases.append(off+28-(fields[1]-DATA_BASE))
    if len(set(bases)) != 1:
        raise ValidationError(f'RomPager data mapping inconsistent: {bases}')
    ram_file_offset = bases[0]
    def resolve(ptr: int) -> int | None:
        if DATA_BASE <= ptr < DATA_BASE + len(blob):
            offset = ram_file_offset + ptr - DATA_BASE
        elif RAM_BASE <= ptr < RAM_BASE + len(blob):
            offset = ptr - RAM_BASE
        else:
            return None
        return offset if 0 <= offset < len(blob) else None

    output = []
    for name, (location, fields) in sorted(known.items()):
        items_at = resolve(fields[1])
        if items_at is None or items_at % 4:
            continue
        items = []
        termination = False
        for j in range(1024):
            where = items_at+j*8
            if where+8 > len(blob):
                break
            kind, pointer = struct.unpack_from('>II',blob,where)
            if kind == 0 and pointer == 0:
                termination=True
                break
            if kind > 64:
                break
            at = resolve(pointer)
            if at is None:
                break
            items.append({'type': kind,'pointer':f'0x{pointer:08x}', 'offset':at})
        obj = {
            'url':name,
            'descriptor_offset':location,
            'item_table_offset': items_at,
            'extension_offset': resolve(fields[2]) if fields[2] else None,
            'mime_enum':fields[5],
            'cache_enum':fields[6],
            'items_count':len(items),
            'item_type_counts':dict(Counter(str(item['type']) for item in items)),
            'terminated':termination,
            'items': items,
        }
        output.append(obj)
    return ram_file_offset, output


def export_static_candidates(blob: bytes, objects: list[dict], folder: Path) -> list[dict]:
    folder.mkdir(parents=True, exist_ok=True)
    exported = []
    for obj in objects:
        if not obj['terminated'] or not obj['items']:
            continue
        if not obj['url'].lower().endswith(('.css','.js')):
            continue
        if any(item['type'] != 1 for item in obj['items']):
            continue
        pieces=[]
        for item in obj['items']:
            pos=item['offset']
            end=blob.find(b'\0',pos,min(pos+8192,len(blob)))
            if end < 0:
                pieces=[]
                break
            pieces.append(blob[pos:end])
        if not pieces:
            continue
        assembled=b''.join(pieces)
        # Don't present template bytecode as executable static assets.
        if any(c >= 128 or (c < 32 and c not in (9, 10, 13)) for c in assembled):
            continue
        rel=obj['url'].lstrip('/')
        if '..' in Path(rel).parts:
            continue
        dest=folder/rel
        dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(assembled)
        exported.append({'url':obj['url'],'path':str(dest.relative_to(folder)),
                         'bytes':len(assembled),'sha256':sha256(assembled)})
    return exported


def main() -> int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('archives', type=Path, nargs='+')
    ap.add_argument('--output',type=Path,default=Path('rompager-results'))
    args=ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for archive in args.archives:
        _, image = load_zip(archive)
        blob = read_image(image).unpacked
        base, resources=discover(blob)
        out=args.output/archive.stem
        out.mkdir(parents=True, exist_ok=True)
        exported=export_static_candidates(blob,resources,out/'static-assets')
        metadata={
            'archive':archive.name,
            'data_mapping_file_offset':f'0x{base:08x}',
            'resource_descriptors_identified':len(resources),
            'terminated_resource_lists':sum(o['terminated'] for o in resources),
            'static_assets_exported':exported,
            'warning':'Static exports are preliminary only; embedded dynamic templates/handlers are NOT reconstructed',
            'resources':resources,
        }
        (out/'resource-map.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
        (out/'resource-summary.txt').write_text('\n'.join(f"{r['url']:<68} items={r['items_count']:>4} types={r['item_type_counts']}" for r in resources)+'\n',encoding='utf-8')
        print(f'{archive.name}: mapping=0x{base:x}, descriptors={len(resources)}, terminated={metadata["terminated_resource_lists"]}, static exports={len(exported)}')
        print('  static: '+', '.join(e['url'] for e in exported[:12]))
    return 0


if __name__=='__main__':
    raise SystemExit(main())
