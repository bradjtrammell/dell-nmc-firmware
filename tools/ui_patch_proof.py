#!/usr/bin/env python3
"""ONE-BYTE, OFFLINE-ONLY proof that a web resource can be changed while all
other decompressed application bytes remain identical. Not flash approved."""
from __future__ import annotations

from dataclasses import replace
import json
from pathlib import Path
import sys

from h910p_rebuild import load_zip, read_image, rebuild_same_application, compare, sha256
from rompager_map import discover


def test_css_patch(archive: Path, report_path: Path) -> dict:
    _, raw = load_zip(archive)
    original = read_image(raw)
    blob = original.unpacked
    map_base, objects = discover(blob)
    css = next(o for o in objects if o['url']=='/html/mgeweb.css')
    assert css['terminated']
    # Only look inside actual CSS resource's first type-1 data item.
    assert css['items'][0]['type'] == 1
    start = css['items'][0]['offset']
    end = blob.index(0, start, start + 4096)
    needle = b'background-color: #FFFFDD;'
    assert blob[start:end].count(needle) == 1, 'Expected original CSS not found exactly once'
    offset = blob.index(needle, start, end) + len(b'background-color: #FFF')
    assert blob[offset:offset+1] == b'F'
    altered = bytearray(blob)
    altered[offset] = ord('E')
    modified = replace(original, unpacked=bytes(altered))
    rebuilt = rebuild_same_application(modified)
    reread = read_image(rebuilt)
    base2, objects2 = discover(reread.unpacked)
    assert map_base == base2
    assert [(o['url'],o['descriptor_offset'],o['items_count'],o['item_type_counts']) for o in objects] == [(o['url'],o['descriptor_offset'],o['items_count'],o['item_type_counts']) for o in objects2]
    differing = [i for i,(a,b) in enumerate(zip(blob,reread.unpacked)) if a!=b]
    assert differing == [offset], f'Unexpected application changes: {differing[:12]}'
    assert reread.unpacked[offset] == ord('E')
    report={
        'input_archive':archive.name,
        'test':'ONE-BYTE WEB RESOURCE PATCH - OFFLINE ONLY',
        'resource':'/html/mgeweb.css',
        'patch_application_offset_hex':f'0x{offset:08X}',
        'original_byte':chr(blob[offset]),
        'new_byte':chr(reread.unpacked[offset]),
        'changed_application_bytes':len(differing),
        'all_non_ui_application_bytes_identical':len(differing)==1,
        'rompager_descriptors_unchanged':True,
        'recompressed_app_byte_exact_to_intended_change':reread.unpacked==bytes(altered),
        'recomputed_crc_valid':True,
        'original_compressed_bytes':len(original.compressed),
        'repacked_compressed_bytes':len(reread.compressed),
        'image_growth_bytes':len(rebuilt)-len(original.original),
        'original_sha256':sha256(original.original),
        'proof_image_sha256':sha256(rebuilt),
        'status':'OFFLINE PASS ONLY. NOT INSTALLABLE/FLASH APPROVED. No runtime or device validation.'
    }
    report_path.parent.mkdir(parents=True,exist_ok=True)
    report_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    return report


if __name__=='__main__':
    a=Path(sys.argv[1]) if len(sys.argv)>1 else Path('/mnt/data/NMC_Firmware_01_18_0005_X2JYN.zip')
    o=Path(sys.argv[2]) if len(sys.argv)>2 else Path('reports/css-patch-proof.json')
    print(json.dumps(test_css_patch(a,o),indent=2))
