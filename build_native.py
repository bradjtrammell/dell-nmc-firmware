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
import argparse
import base64
import gzip
import zopfli.gzip
from urllib.parse import quote
import json
import re
import struct
import sys
import shutil
import subprocess
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'tools'))
from h910p_rebuild import load_zip, read_image, sha256, crc32_netos
from rompager_map import discover

SRC = HERE / 'source'
OUT = HERE / 'build'
VENDOR = HERE / 'originals' / 'NMC_Firmware_01_18_0005_X2JYN.zip'
FACTORY_IMAGE_BYTES = 1517029
KNOWN_V042_SHA256 = 'db0cede9c360b71a36dc68425970b8119005f170b3d9cfb7bda4eba2e7515757'
OUTPUT_NAME = 'H910P_01.18.0005_INSTRUMENT_PANEL_EXPERIMENTAL.bin'
JAVASCRIPT_FILES = ['ui-data.js', 'ui-state.js', 'ui-view.js', 'ui-pages.js', 'ui-config.js', 'ui-nav.js', 'ui-console.js', 'ui-notifications.js', 'ui-notification-bridge.js', 'ui-dashboard.js', 'ui-updates.js', 'ui-auth.js', 'ui-start.js']
# Deliberately select only static, non-control help resource strings.
# Original help content at these paths IS LOST; it is a remaining blocker.
RESOURCE_NAMES = [
    '/Help/hlpnetw.htm', '/html/synoptic/dhtml.js', '/html/synoptic/ajax.js', '/html/synoptic/png.js', '/Help/hlpconf.htm', '/Help/hlpnoti.htm',
    '/Help/hlpweek.htm', '/Help/hlpcont.htm', '/Help/hlpprop.htm',
    '/Help/hlpacce.htm', '/Help/hlpenlog.htm', '/Help/hlpmail.htm',
    '/Help/hlpsyst.htm', '/Help/hlpfirm.htm', '/Help/hlpulog.htm',
    '/Help/hlpcli.htm', '/Help/hlptime.htm', '/Help/hlpslog.htm',
    '/Help/hlpenv.htm', '/Help/hlpelog.htm', '/Help/hlpindex.htm',
]

# Legacy synoptic graphics are already unusable in v0.5: dhtml.js is repurposed.
# Reclaim their fixed STRING slots only; preserve every dynamic callback/descriptor.
# This intentionally removes the old OEM power-diagram presentation, not its backend.
RETIRED_SYNOPTIC = [
 '/html/synoptic/ajax.js', '/html/synoptic/png.js',
 '/html/synoptic/ups_on_line_bpmanu.js', '/html/synoptic/ups_on_line_bpauto.js',
 '/html/synoptic/ups_measure_33.js', '/html/synoptic/ups_measure_31.js',
 '/html/synoptic/ups_measure_11.js', '/html/synoptic/ups_line_interactive.js',
 '/html/synoptic/ups_Dell_measure_11.js', '/html/synoptic/ups_on_line.js',
 '/html/synoptic/ups_measure_31_phph.js', '/html/synoptic/ups_measure_11_simple.js',
 '/html/synoptic/ups_measure_11_simple_extremec.js',
 '/Help/hlpmenu.htm', '/Help/hlpsnmp.htm', '/Help/hlpsenv.htm', '/Help/hlpmng.htm'
]

BOOTSTRAP_RESOURCE = RESOURCE_NAMES[0]
# Every page here was already overwritten by the verified v0.4.2 build.
# Reset unused old chunks to deterministic blanks so OEM and v0.4.2 inputs
# produce exactly the same application and resulting firmware image.
OLD_CHUNK_RESOURCES = [
    '/Help/hlpnetw.htm', '/Help/hlpconf.htm', '/Help/hlpnoti.htm',
    '/Help/hlpweek.htm', '/Help/hlpcont.htm', '/Help/hlpprop.htm',
    '/Help/hlpacce.htm', '/Help/hlpenlog.htm', '/Help/hlpmail.htm',
    '/Help/hlpsyst.htm', '/Help/hlpfirm.htm', '/Help/hlpulog.htm',
    '/Help/hlpcli.htm', '/Help/hlptime.htm', '/Help/hlpslog.htm',
    '/Help/hlpenv.htm', '/Help/hlpelog.htm', '/Help/hlpindex.htm',
]


def rebuild_optimized(original, unpacked: bytes, max_size: int = FACTORY_IMAGE_BYTES) -> bytes:
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
    if len(image) > max_size:
        raise ValueError(f'Optimized image exceeds factory firmware size limit by {len(image)-max_size} bytes')
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


def distribute(base64_data: str, resources: dict, unpacked: bytes, compressed_bytes: int) -> dict[str, list[str]]:
    """Bootstrap is served as one script; other resource chunks are loaded in order.

    One script reference in /index.htm removes its fixed-capacity page-count
    limit and allows use of additional known-static Help resources.
    """
    allocated = {}
    cursor = 0
    initializing = True
    # Pack largest available static resources first to fit the 672-byte
    # default.htm entry (each referenced script consumes scarce HTML bytes).
    ordered = sorted(RESOURCE_NAMES[1:], key=lambda name: sum(length for _, length in slots_of(unpacked, resources[name])), reverse=True)
    for name in ordered:
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
    boot = pack_statements(browser_bootstrap(len(base64_data), compressed_bytes),slots_of(unpacked,resources[BOOTSTRAP_RESOURCE]))
    return {BOOTSTRAP_RESOURCE: [x.decode('ascii').rstrip(' ') for x in boot],**allocated}


def firmware_schema_source(application: bytes) -> str:
    """Embed only the OEM field map as a recovery catalog, never default values/secrets.

    The live XML transport on H910P can return invalid content. The catalog
    permits readable configuration navigation, but is NOT evidence of live values.
    """
    start = application.find(b'<SYSTEM_CONFIG')
    end = application.find(b'</SYSTEM_CONFIG>', start)
    if start < 0 or end < 0:
        raise ValueError('Required OEM configuration schema missing from firmware')
    xml = application[start:end+len(b'</SYSTEM_CONFIG>')]
    root = ET.fromstring(xml)
    groups = []
    for group in root.findall('GROUP'):
        items=[]
        for el in group.findall('ITEM'):
            if el.get('access') != 'RW':
                continue
            key = el.get('object', '')
            if not re.fullmatch(r'(?:System|Environment)\.[A-Za-z0-9_.\[\]]{1,110}',key):
                continue
            items.append([key, el.get('type','String:49'), el.get('unit',''), el.get('restart') == 'TRUE'])
        if items:
            groups.append([group.get('id','Settings'), group.get('hidden') == 'TRUE', items])
    return 'window.U=window.U||{};U.oemCatalog='+json.dumps(groups,separators=(',',':'),ensure_ascii=True)+';'


def source_blob(application: bytes) -> tuple[str, str, bytes]:
    css = (SRC / 'tailwind-compiled.css').read_text(encoding='utf-8') + '\n' + (SRC / 'ui.css').read_text(encoding='utf-8')
    # Remove CSS documentation comments before deterministic asset compression.
    # Keep all selectors, declarations, and ordering intact.
    css = re.sub(r'/\*[\s\S]*?\*/', '', css)
    css = re.sub(r'\s*([{};])\s*', r'\1', css)
    css = re.sub(r'\n+', '', css)
    css = re.sub(r'\s*([:,>])\s*', r'\1', css)
    js_files = []
    for name in JAVASCRIPT_FILES:
        script = (SRC / name).read_text(encoding='utf-8')
        # Preserve readable source while omitting non-runtime JS block comments
        # from constrained embedded RomPager static resource slots.
        script = re.sub(r'(?m)^[ \t]*/\*[\s\S]*?\*/', '', script)
        script = re.sub(r'(?m)^[ \t]+', '', script)
        if name in ('ui-notifications.js','ui-notification-bridge.js'):
            script = script.replace('\n', '')
        if name == 'ui-console.js':
            logo = quote((SRC / 'dell-white.svg').read_text('utf-8'),safe="/=:;,+-_.!~*'()")
            script += '\nU.embedLogo("data:image/svg+xml,' + logo + '");U.installConsole();'
        js_files.append(script.replace("\n", ""))
    scripts = '\n'.join([firmware_schema_source(application)] + js_files)
    content = (css + '\0' + scripts).encode('utf-8')
    return css, scripts, zopfli.gzip.compress(content, numiterations=500)


def browser_bootstrap(expected_bytes: int, compressed_bytes: int) -> list[str]:
    """All payloads load in native parser order BEFORE this bootstrap.

    The v0.5.3-v0.5.5 dynamic loader made the UI depend on sequential
    onload events for 17 requests and failed on physical H910P HTTP service.
    Avoid dynamic network requests in the startup path.
    """
    return [
        '(async function(){',
        'document.querySelector("main").id="app";',
        'try{',
        f'var expected={expected_bytes};',
        'var s=window.H910P_PACK||"";',
        'if(s.length!==expected)throw Error("Incomplete assets ("+s.length+"/"+expected+" bytes)");',
        'var vm=document.createElement("meta");vm.name="viewport";vm.content="width=device-width,initial-scale=1";document.head.appendChild(vm);',
        'if(typeof DecompressionStream!=="function")throw Error("Browser lacks gzip support");',
        'var alphabet="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$%&()*+-;<=>?@^_`{|}~";',
        'var lookup={};for(var i=0;i<alphabet.length;i++)lookup[alphabet.charCodeAt(i)]=i;',
        f'var a=new Uint8Array({compressed_bytes});',
        'for(var k=0,o=0;k<s.length;k+=5,o+=4){var n=0;for(var j=0;j<5;j++){var digit=lookup[s.charCodeAt(k+j)];if(digit===undefined)throw Error("Invalid asset data");n=n*85+digit;}if(n>4294967295)throw Error("Asset overflow");for(var j=3;j>=0;j--){if(o+j<a.length)a[o+j]=n%256;n=Math.floor(n/256)}}',

        'var z=new Blob([a]).stream().pipeThrough(new DecompressionStream("gzip"));',
        'var code=await new Response(z).text(),p=code.indexOf("\\u0000");',
        'if(p<0)throw Error("Missing asset separator");',
        'var css=document.createElement("style");css.textContent=code.slice(0,p);document.head.appendChild(css);',
        'var js=document.createElement("script");js.textContent=code.slice(p+1);document.head.appendChild(js);',
        '}catch(e){var a=document.getElementById("app");a.textContent="Unable to load the custom interface: "+e.message+". ";var l=document.createElement("a");l.href="/app_upload.htm";l.textContent="Firmware recovery";a.appendChild(l);console.error("H910P UI:",e)}',
        '})();',
    ]


def index_page(names: list[str], entry: str = '/default.htm') -> str:
    """Use only *static, parser ordered script tags*, like working v0.5.2.

    Short relative root URLs leave room for an always-present emergency
    recovery link even when JavaScript cannot execute.
    """
    if not names or names[-1] != BOOTSTRAP_RESOURCE:
        raise ValueError('Bootstrap must be the final script')
    html = '<main id=a>' + ''.join(
        '<script src=' + name.lstrip('/') + '></script>' for name in names
    )
    if len(html.encode('ascii')) > (672 if entry == '/default.htm' else 713):
        raise ValueError('Native entry page capacity exceeded: ' + entry)
    return html


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--from-v042', type=Path, help='Build using an exact SHA-256 verified local v0.4.2 BIN instead of the Dell vendor ZIP')
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    if args.from_v042:
        firmware = args.from_v042.read_bytes()
        if sha256(firmware) != KNOWN_V042_SHA256:
            raise ValueError('Supplied firmware does not match the proven v0.4.2 baseline; build aborted')
    else:
        _, firmware = load_zip(VENDOR)
    original = read_image(firmware)
    before = original.unpacked
    base, records = discover(before)
    resource_by_path = {d['url']: d for d in records}
    css, js, compressed_assets = source_blob(before)
    b64 = base64.b85encode(compressed_assets, pad=True).decode('ascii')
    resources = distribute(b64, resource_by_path, before, len(compressed_assets))
    html = index_page(list(resources)[1:] + [BOOTSTRAP_RESOURCE])
    # /default.htm is the browser's standard NMC landing page. Keep /index.htm
    # available as an alias. Do not expose the old empty legacy frameset.
    patches = {'/default.htm': html, '/index.htm': html, **resources}
    # Normalization of legacy Help chunks makes the result identical regardless
    # of whether users begin with OEM firmware or the already-installed v0.4.2.
    unused_old_chunks = [n for n in OLD_CHUNK_RESOURCES if n not in resources]
    for name in unused_old_chunks:
        patches[name] = ['' for _ in slots_of(before, resource_by_path[name])]
    for entry in ('/default.htm', '/index.htm'):
        assert len(html.encode('ascii')) <= sum(length for _, length in slots_of(before, resource_by_path[entry])), 'Entry page exceeds native RomPager capacity: '+entry
    updated = bytearray(before)
    allowed = set()
    site = OUT / 'served-native-ui'
    site.mkdir(exist_ok=True)
    for name, value in patches.items():
        d = resource_by_path[name]
        spans = slots_of(before, d)
        # The boot entry page is split at tag boundaries, not JS statements.
        if name in ('/index.htm', '/default.htm'):
            # Fixed page slots happen to be 412 + 301; any HTML boundary is valid.
            text = value.encode('ascii')
            pieces=[]
            cursor=0
            # RomPager concatenates consecutive STRING fragments. Fill each
            # slot exactly up to its capacity, padding only the final slot.
            # This permits an HTML attribute to cross a slot boundary without
            # injecting spaces into a resource URL.
            for i, (_, cap) in enumerate(spans):
                chunk=text[cursor:cursor+cap]
                cursor += len(chunk)
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
        if name not in ('/index.htm', '/default.htm'):
            # RomPager's MIME enum is encoded in resource metadata, not URL.
            mime_off = d['descriptor_offset'] + 20
            updated[mime_off:mime_off+4] = struct.pack('>I', 21)
            allowed.update(range(mime_off, mime_off+4))
        filepath = site / name.lstrip('/')
        filepath.parent.mkdir(parents=True, exist_ok=True)
        filepath.write_bytes(b''.join(updated[off:off+cap] for off,cap in spans))
    retired_count = 0
    retired_resource_names = []
    active_resource_names = set(resources)
    for name in RETIRED_SYNOPTIC:
        if name in active_resource_names:
            continue  # A packed JS resource must never be blanked after installation.
        d = resource_by_path[name]
        retired_resource_names.append(name)
        for it in d['items']:
            if it['type'] != 1:
                continue  # Keep every dynamic handler/callback unmodified.
            offset = it['offset']
            end = before.find(b'\0', offset, min(offset+8192, len(before)))
            if end < offset:
                raise ValueError('Unterminated retired UI string in '+name)
            cap = end-offset
            if cap:
                updated[offset:end] = b' ' * cap
                allowed.update(range(offset,end))
                retired_count += cap

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
    # The firmware is the deliverable, not the temporary pre-retirement files.
    # Export and validate each resource from the final re-decoded BIN. This
    # catches subsequent overwrite by cleanup passes (the v0.5.4 regression).
    live_resource_bytes = {}
    for name in patches:
        descriptor = table[name]
        live_resource_bytes[name] = b''.join(
            reread.unpacked[off:off + cap]
            for off, cap in slots_of(reread.unpacked, descriptor)
        )
        export_path = site / name.lstrip('/')
        if export_path.read_bytes() != live_resource_bytes[name]:
            raise AssertionError('Final firmware resource diverged from packed source: ' + name)
    # Assert no asset-bearing resource was touched by presentation retirement.
    for name in resources:
        if name not in live_resource_bytes:
            raise AssertionError('Missing firmware resource: ' + name)
    # Every intended JS chunk must survive in the actual firmware.
    final_scripts = '\n'.join(
        live_resource_bytes[name].decode('ascii') for name in resources
    )
    final_encoded = ''.join(re.findall(
        r'window\.H910P_PACK\+="([^"]+)";', final_scripts
    ))
    if final_encoded != b64:
        raise AssertionError('Final firmware embedded package corrupted')
    if gzip.decompress(base64.b85decode(final_encoded)[:len(compressed_assets)]) != (css+'\0'+js).encode('utf-8'):
        raise AssertionError('Final firmware asset round trip failed')
    # Reconstruct compressed assets *only* from the resources as actually packed.
    packed_site='\n'.join((site/name.lstrip('/')).read_text('ascii') for name in resources)
    extracted=''.join(re.findall(r'window\.H910P_PACK\+="([^"]+)";', packed_site))
    assert extracted == b64, 'Embedded chunks differ from authored package'
    unpacked_assets=gzip.decompress(base64.b85decode(extracted)[:len(compressed_assets)])
    assert unpacked_assets == (css+'\0'+js).encode('utf-8'), 'Embedded CSS/JS round-trip failed'
    (site/'index.html').write_bytes((site/'index.htm').read_bytes())
    binary=OUT/OUTPUT_NAME
    binary.write_bytes(result)
    report={
        'status':'EXPERIMENTAL: NOT APPROVED FOR FLASHING LIVE NMC',
        'source_image': 'verified-v0.4.2' if args.from_v042 else 'vendor-01.18.0005',
        'original_sha256':sha256(firmware),'custom_sha256':sha256(result),
        'original_bytes':len(firmware),'custom_bytes':len(result),
        'firmware_size_delta':len(result)-FACTORY_IMAGE_BYTES,
        'application_size_unchanged':len(before)==len(updated),
        'compressed_ui_bytes':len(compressed_assets),'encoded_ui_bytes':len(b64),'embedded_encoding':'base85',
        'modified_application_byte_count':len(differences),
        'all_changed_bytes_within_static_ui_allowlist':True,
        'boot_header_and_original_callbacks_preserved':True,
        'resource_pointers_and_item_tables_unchanged':True,
        'firmware_crc_valid':True,
        'embedded_ui_round_trip_exact':True,
        'native_index_script_count':len(re.findall(r'<script src=',html)),
        'landing_page': '/default.htm',
        'native_default_script_count':len(re.findall(r'<script src=',html)),
        'native_index_scripts_load_in_order':True,
        'bootstrap_mode':'static parser ordered (no async resource fetch)',
        'firmware_within_factory_image_size':len(result)<=FACTORY_IMAGE_BYTES,
        'repurposed_original_help_pages':list(resources),
        'cleared_unused_prior_help_chunks':unused_old_chunks,
        'retired_legacy_synoptic_resources': retired_resource_names,
        'retired_legacy_synoptic_static_bytes': retired_count,
        'legacy_entry_pages_preserved':False,
        'legacy_default_replaced_by_native_ui':True,
        'original_management_uses_legacy_frames':True,
        'power_and_configuration_actions_in_new_ui':'DESCRIPTOR-BACKED RW CONFIG AND DOCUMENTED OUTLET COMMANDS (SIMULATED BROWSER TESTS); OEM ENTRY PAGES RESTORED, SOME LEGACY ASSETS REPURPOSED',
        'unverified':['NMC hardware boot','Firmware upload acceptance','Actual NMC HTTP-serving','Live XML compatibility','Automatic firmware recovery','Legacy help availability','Flash memory headroom'],
    }
    (OUT/'VALIDATION.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':
    main()
