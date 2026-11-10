"""Recover exact UI CSS and JS from the final native RomPager image exports."""
from __future__ import annotations
import base64
import gzip
import json
import re
from pathlib import Path


def unpack(root: Path) -> tuple[str, str]:
    site = root / 'build' / 'served-native-ui'
    meta = json.loads((root / 'build' / 'VALIDATION.json').read_text())
    files = [site / name.lstrip('/') for name in meta['repurposed_original_help_pages']]
    full = '\n'.join(f.read_text('ascii') for f in files)
    codes = ''.join(re.findall(r'window\.H910P_PACK\+="([^"]+)";', full))
    if not codes or len(codes) != meta['encoded_ui_bytes']:
        raise ValueError('Missing or incomplete native ROM assets')
    if meta['embedded_encoding'] != 'base85':
        raise ValueError('Unsupported native ROM encoding')
    raw = base64.b85decode(codes)[:meta['compressed_ui_bytes']]
    css, js = gzip.decompress(raw).decode('utf-8').split('\0', 1)
    if '</script' in js.lower():
        raise ValueError('Embedded JS closes script element')
    if '</style' in css.lower():
        raise ValueError('Embedded CSS closes style element')
    return css, js


def inline_html(root: Path) -> str:
    css, js = unpack(root)
    return '<!doctype html><html><head><style>'+css+'</style></head><body><main id="app"></main><script>'+js+'</script></body></html>'
