#!/usr/bin/env python3
"""Package the exact built UI scripts as a self-contained OFFLINE demo.

Demo authentication and data are simulated. Never use the demo as firmware,
and never connect its fake adapters to a live management card.
"""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SITE = ROOT / 'build' / 'served-native-ui'
MOCK = Path(__file__).with_name('demo-mock.js')
STYLE = '''<style>
body::after{content:'OFFLINE DEMO — simulated UPS data';position:fixed;bottom:12px;right:12px;z-index:9999;background:#164b67;color:white;padding:9px 12px;font:600 12px Arial,sans-serif;border-radius:3px;box-shadow:0 2px 10px #0002;pointer-events:none}
body:has(.signin-page)::before{content:'Preview only: Enter any username and password. No NMC connection is made.';position:fixed;top:16px;left:16px;right:16px;z-index:9998;text-align:center;padding:12px;background:#fff5dc;color:#683d04;border:1px solid #dec27c;font:600 13px Arial,sans-serif;pointer-events:none}
@media(max-width:520px){body:has(.signin-page)::before{position:static;display:block;margin:12px 8px}body::after{font-size:10px;right:6px;bottom:6px}}
</style>'''


def build(destination: Path) -> None:
    from embedded_assets import unpack
    css, js = unpack(ROOT)
    original = '<!doctype html><html><head><style>'+css+'</style></head><body><main id="app"></main></body></html>'
    html = original
    mock_js = MOCK.read_text(encoding='utf-8')
    if '</script' in mock_js.lower():
        raise ValueError('Mock includes an HTML script terminator')
    if html.count('<html>') != 1:
        raise ValueError('Unexpected embedded entry page')
    html = html.replace('<html>', '<html>' + STYLE + '<script>' + mock_js + '</script>', 1).replace('</body>', '<script>'+js+'</script></body>')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text('<!-- OFFLINE DEMONSTRATION: Simulated device values and credentials. -->\n' + html, encoding='utf-8')
    print(f'Offline demo: {destination} ({destination.stat().st_size} bytes)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build' / 'H910P_offline_demo.html')
    args = parser.parse_args()
    build(args.output)
