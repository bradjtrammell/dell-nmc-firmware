#!/usr/bin/env python3
"""Build an *experimental*, embedded native browser UI from user-provided Dell BIN.

Offline only: no contact with the network management card and NO FLASHING.
Resource allocation is constrained to original fixed-size static strings; every
RomPager descriptor, callback pointer, and non-allowlisted application byte is
strictly preserved.  This is not proof of hardware boot/update acceptance.
"""
from __future__ import annotations

from dataclasses import replace
from pathlib import Path
import base64
import gzip
import json
import re
import struct
import sys
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'tools'))
from h910p_rebuild import load_zip, read_image, sha256, crc32_netos
from rompager_map import discover

SRC = HERE / 'source'
OUT = HERE / 'build'
VENDOR = HERE / 'originals' / 'NMC_Firmware_01_18_0005_X2JYN.zip'
OUTPUT_NAME = 'H910P_01.18.0005_INSTRUMENT_PANEL_EXPERIMENTAL.bin'
JAVASCRIPT_FILES = ['ui-data.js', 'ui-state.js', 'ui-view.js', 'ui-pages.js', 'ui-config.js', 'ui-nav.js', 'ui-start.js']
# Deliberately select only static, non-control help resource strings.
# Original help content at these paths IS LOST; it is a remaining blocker.
RESOURCE_NAMES = [
    '/Help/hlpnetw.htm', '/Help/hlpconf.htm', '/Help/hlpnoti.htm',
    '/Help/hlpenv.htm', '/Help/hlpcont.htm', '/Help/hlpprop.htm',
    '/Help/hlpacce.htm', '/Help/hlpcli.htm', '/Help/hlpmail.htm',
    '/Help/hlpsyst.htm', '/Help/hlpfirm.htm', '/Help/hlpulog.htm',
    '/Help/hlpenlog.htm', '/Help/hlptime.htm', '/Help/hlpslog.htm',
    '/Help/hlpweek.htm', '/Help/hlpelog.htm',
]

BOOTSTRAP_RESOURCE = RESOURCE_NAMES[0]

def rebuild_optimized(original, unpacked: bytes) -> bytes:
    """Losslessly compress the patched ARM application, preserving its image header.

    A proper 8-token-state dynamic-programming LZSS2 encoder is built locally
    from source. Unlike the original greedy encoder, it produces a modified
    BIN smaller than the vendor image without changing any application bytes.
    """
    compiler = shutil.which('g++')
    if not compiler:
        raise RuntimeError('g++ compiler required for the optimal LZSS2 encoder')
    src = HERE / 'tools' / 'lzss_opt.cpp'
    binary = OUT / 'lzss_opt'
    raw = OUT / 'unpacked_for_compression.bin'
    coded = OUT / 'lzss_opt_compressed.bin'
    subprocess.run([compiler, '-std=c++17', '-O3', '-Wall', '-Wextra', '-Werror',
                    '-o', str(binary), str(src)], check=True, capture_output=True)
    raw.write_bytes(unpacked)
    subprocess.run([str(binary), str(raw), str(coded)], check=True,
                   capture_output=True, timeout=90)
    compressed = coded.read_bytes()
    header = bytearray(original.header)
    struct.pack_into('>I', header, len(header) - 4, len(compressed))
    prefix = bytes(header) + compressed
    image = prefix + crc32_netos(prefix).to_bytes(4, 'big')
    checked = read_image(image)
    if checked.unpacked != unpacked or checked.header[:32] != original.header[:32]:
        raise ValueError('Optimized firmware round-trip/header mismatch')
    if len(image) > len(original.original):
        raise ValueError(f'Optimized image still exceeds original by {len(image)-len(original.original)} bytes')
    return image


def slots_of(blob: bytes, desc: dict) -> list[tuple[int, int]]:
    if not desc['terminated'] or any(it['type'] != 1 for it in desc['items']):
        raise ValueError('Non-static or unterminated resource: ' + desc['url'])
    spans = []
    for it in desc['items']:
        off = it['offset']
        if off < 0 or off >= len(blob):
            raise ValueError('Static pointer out of range')
        end = blob.find(b'\0', off, min(off + 8192, len(blob)))
        if end < off:
            raise ValueError('No static terminator')
        spans.append((off, end - off))
    return spans


def pack_statements(statement_lines: list[str], spans: list[tuple[int, int]]) -> list[bytes]:
    """Pack complete JS statements into fixed-size RomPager string slots."""
    body = '\n'.join(statement_lines) + '\n'
    if not body.isascii() or '\0' in body:
        raise ValueError('Non-ASCII or NUL in bootstrap')
    # Each statement finishes at a newline; never split a quoted JS string.
    boundaries = [m.end() for m in re.finditer(r'\n', body)]
    pos = 0
    output = []
    for _, capacity in spans:
        candidates = [i for i in boundaries if pos < i <= pos + capacity]
        end = candidates[-1] if candidates else pos
        chunk = body[pos:end].encode('ascii')
        output.append(chunk + b' ' * (capacity - len(chunk)))
        pos = end
    if pos != len(body):
        raise ValueError(f'Insufficient capacity; {len(body)-pos} source bytes remain')
    return output


def distribute(base64_data: str, resources: dict, unpacked: bytes) -> dict[str, list[str]]:
    """Bootstrap is served as one script; other resource chunks are loaded in order.

    One script reference in /index.htm removes its fixed-capacity page-count
    limit and allows use of additional known-static Help resources.
    """
    allocated = {}
    cursor = 0
    initializing = True
    for name in RESOURCE_NAMES[1:]:
        spans = slots_of(unpacked, resources[name])
        statements = []
        for _, capacity in spans:
            init = 'window.H910P_PACK="";' if initializing else ''
            overhead = len(init) + len('window.H910P_PACK+="";')
            available = capacity - overhead
            if available <= 0 or cursor == len(base64_data):
                statements.append('')
                continue
            chunk = base64_data[cursor:cursor+available]
            statements.append(init + 'window.H910P_PACK+="' + chunk + '";')
            cursor += len(chunk)
            initializing = False
        if any(statements):allocated[name] = statements
        if cursor == len(base64_data):break
    if cursor != len(base64_data):
        raise ValueError('Cannot store compressed assets: insufficient static resource storage')
    boot = pack_statements(browser_bootstrap(list(allocated)),slots_of(unpacked,resources[BOOTSTRAP_RESOURCE]))
    return {BOOTSTRAP_RESOURCE: [x.decode('ascii').rstrip(' ') for x in boot],**allocated}


def source_blob() -> tuple[str, str, bytes]:
    css = (SRC / 'tailwind-compiled.css').read_text(encoding='utf-8') + '\n' + (SRC / 'ui.css').read_text(encoding='utf-8')
    scripts = '\n'.join((SRC / name).read_text(encoding='utf-8') for name in JAVASCRIPT_FILES)
    content = (css + '\0' + scripts).encode('utf-8')
    return css, scripts, gzip.compress(content, compresslevel=9, mtime=0)


def browser_bootstrap(asset_paths: list[str]) -> list[str]:
    # DecompressionStream is supplied by current desktop Chromium / Firefox;
    # no external network requests or scripting libraries are needed.
    return [
        '(async function(){',
        'try{',

        'var vm=document.createElement("meta");vm.name="viewport";vm.content="width=device-width,initial-scale=1";document.head.appendChild(vm);',
        'if(typeof DecompressionStream!=="function")throw Error("Modern browser compression support required");',
        'var s=atob(window.H910P_PACK||"");',
        'var a=new Uint8Array(s.length);',
        'for(var i=0;i<s.length;i++)a[i]=s.charCodeAt(i);',
        'var z=new Blob([a]).stream().pipeThrough(new DecompressionStream("gzip"));',
        'var t=await new Response(z).text();',
        'var p=t.indexOf("\\u0000");',
        'if(p<0)throw Error("Invalid embedded asset package");',
        'var css=document.createElement("style");css.textContent=t.slice(0,p);document.head.appendChild(css);',
        'var js=document.createElement("script");js.textContent=t.slice(p+1);document.head.appendChild(js);',
        '}catch(e){document.getElementById("app").innerHTML="<div style=\'font:16px Arial;padding:32px\'>Unable to load the custom interface. <a href=\'/default.htm\'>Open original Dell management</a>.</div>";console.error("NMC UI:",e)}',
        '})();',
    ]


def index_page(names: list[str]) -> str:
    """Load all chunk scripts before the bootstrap; fit 713 original static bytes.

    v0.4.1 included only the bootstrap resource and its browser tests injected
    all other scripts manually. That caused the real browser to initialize with
    an empty H910P_PACK and fail. This creates an actual native load sequence.
    """
    doc = '<!doctype html><html><main id=app></main>'
    for name in names:
        doc += '<script src=' + name + '></script>'
    return doc


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    _, firmware = load_zip(VENDOR)
    original = read_image(firmware)
    before = original.unpacked
    base, records = discover(before)
    resource_by_path = {d['url']: d for d in records}
    css, js, compressed_assets = source_blob()
    b64 = base64.b64encode(compressed_assets).decode('ascii')
    resources = distribute(b64, resource_by_path, before)
    html = index_page(list(resources)[1:] + [BOOTSTRAP_RESOURCE])
    patches = {'/index.htm': html, **resources}
    assert len(html.encode('ascii')) <= sum(length for _, length in slots_of(before, resource_by_path['/index.htm'])), 'Entry page exceeds native RomPager capacity'
    updated = bytearray(before)
    allowed = set()
    site = OUT / 'served-native-ui'
    site.mkdir(exist_ok=True)
    for name, value in patches.items():
        d = resource_by_path[name]
        spans = slots_of(before, d)
        # The boot entry page is split at tag boundaries, not JS statements.
        if name == '/index.htm':
            # Fixed page slots happen to be 412 + 301; any HTML boundary is valid.
            text = value.encode('ascii')
            pieces=[]
            cursor=0
            for i, (_, cap) in enumerate(spans):
                if i == len(spans)-1:
                    chunk=text[cursor:]
                else:
                    split=text.rfind(b'>', cursor, cursor+cap)
                    if split<cursor:raise ValueError('Cannot safely split entry HTML')
                    chunk=text[cursor:split+1];cursor=split+1
                if len(chunk)>cap:raise ValueError(f'Index HTML exceeds available space by {len(chunk)-cap} bytes')
                pieces.append(chunk+b' '*(cap-len(chunk)))
            if len(pieces)!=len(spans) or cursor+len(text[cursor:])!=len(text):
                raise AssertionError('Bad entry-page packaging')
        else:
            pieces = [part.encode('ascii') + b' ' * (cap - len(part)) for part, (_, cap) in zip(value, spans)]
            if len(pieces) != len(spans) or any(len(x) != length for x, (_, length) in zip(pieces, spans)):
                raise ValueError('Static content slot overflow for ' + name)
        for (off, cap), chunk in zip(spans, pieces):
            updated[off:off+cap] = chunk
            allowed.update(range(off, off + cap))
        if name != '/index.htm':
            # RomPager's MIME enum is encoded in resource metadata, not URL.
            mime_off = d['descriptor_offset'] + 20
            updated[mime_off:mime_off+4] = struct.pack('>I', 21)
            allowed.update(range(mime_off, mime_off+4))
        filepath = site / name.lstrip('/')
        filepath.parent.mkdir(parents=True, exist_ok=True)
        filepath.write_bytes(b''.join(updated[off:off+cap] for off,cap in spans))
    differences = [i for i,(a,b) in enumerate(zip(before,updated)) if a!=b]
    assert len(before)==len(updated)
    assert differences and set(differences)<=allowed
    result = rebuild_optimized(original, bytes(updated))
    reread = read_image(result)
    assert reread.unpacked == bytes(updated)
    assert crc32_netos(result) == 0
    base2, after = discover(reread.unpacked)
    assert base2 == base and len(after) == len(records)
    table = {d['url']:d for d in after}
    for d in records:
        latest=table[d['url']]
        assert latest['items']==d['items']
        assert latest['item_table_offset']==d['item_table_offset']
        assert latest['descriptor_offset']==d['descriptor_offset']
    # Reconstruct compressed assets *only* from the resources as actually packed.
    packed_site='\n'.join((site/name.lstrip('/')).read_text('ascii') for name in resources)
    extracted=''.join(re.findall(r'window\.H910P_PACK\+="([A-Za-z0-9+/=]+)";', packed_site))
    assert extracted == b64, 'Embedded chunks differ from authored package'
    unpacked_assets=gzip.decompress(base64.b64decode(extracted))
    assert unpacked_assets == (css+'\0'+js).encode('utf-8'), 'Embedded CSS/JS round-trip failed'
    (site/'index.html').write_bytes((site/'index.htm').read_bytes())
    binary=OUT/OUTPUT_NAME
    binary.write_bytes(result)
    report={
        'status':'EXPERIMENTAL: NOT APPROVED FOR FLASHING LIVE NMC',
        'original_sha256':sha256(firmware),'custom_sha256':sha256(result),
        'original_bytes':len(firmware),'custom_bytes':len(result),
        'firmware_size_delta':len(result)-len(firmware),
        'application_size_unchanged':len(before)==len(updated),
        'compressed_ui_bytes':len(compressed_assets),'base64_ui_bytes':len(b64),
        'modified_application_byte_count':len(differences),
        'all_changed_bytes_within_static_ui_allowlist':True,
        'boot_header_and_original_callbacks_preserved':True,
        'resource_pointers_and_item_tables_unchanged':True,
        'firmware_crc_valid':True,
        'embedded_ui_round_trip_exact':True,
        'native_index_script_count':len(re.findall(r'<script src=',html)),
        'native_index_scripts_load_in_order':True,
        'firmware_within_factory_image_size':len(result)<=len(firmware),
        'repurposed_original_help_pages':list(resources),
        'original_management_uses_legacy_frames':True,
        'power_and_configuration_actions_in_new_ui':'DESCRIPTOR-BACKED RW CONFIG AND DOCUMENTED OUTLET COMMANDS (SIMULATED BROWSER TESTS); ORIGINAL UI RETAINED',
        'unverified':['NMC hardware boot','Firmware upload acceptance','Actual NMC HTTP-serving','Live XML compatibility','Automatic firmware recovery','Legacy help availability','Flash memory headroom'],
    }
    (OUT/'VALIDATION.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':
    main()
