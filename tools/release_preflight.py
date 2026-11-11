#!/usr/bin/env python3
"""Offline H910P release gate. Never connects to or flashes an NMC."""
from __future__ import annotations
import argparse
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import sys
from h910p_rebuild import read_image, ValidationError, crc32_netos
from rompager_map import discover

FACTORY_SIZE = 1_517_029
PATTERN = re.compile(r'H910P_Custom_UI_v(\d+\.\d+\.\d+(?:-beta\.\d+)?)\.bin\Z')

class Scripts(HTMLParser):
    def __init__(self):
        super().__init__();self.paths=[];self.frames=0
    def handle_starttag(self, tag, attrs):
        values=dict(attrs)
        if tag=='script' and values.get('src'):self.paths.append(values['src'])
        if tag in ('frameset','frame','iframe'):self.frames+=1

def static_text(app:bytes, record:dict) -> str:
    if not record['terminated'] or not record['items'] or any(x['type']!=1 for x in record['items']):
        raise ValidationError('Landing page is not static RomPager content')
    data=[]
    for item in record['items']:
        pos=item['offset']
        end=app.find(b'\x00',pos,min(len(app),pos+8192))
        if end<0:raise ValidationError('Unterminated RomPager static string')
        data.append(app[pos:end])
    return b''.join(data).decode('ascii').rstrip()

def verify(path:Path,expected_sha256:str) -> dict:
    match=PATTERN.fullmatch(path.name)
    if not match:raise ValidationError('BIN filename must contain an exact H910P semantic release version')
    if not re.fullmatch('[0-9a-f]{64}',expected_sha256):
        raise ValidationError('Expected SHA-256 must be 64 lowercase hexadecimal characters')
    raw=path.read_bytes()
    if not 100_000<=len(raw)<=FACTORY_SIZE:raise ValidationError('Image is outside factory size constraints')
    digest=hashlib.sha256(raw).hexdigest()
    if digest!=expected_sha256:raise ValidationError('Firmware SHA-256 does not match release manifest')
    img=read_image(raw)
    _,resources=discover(img.unpacked)
    table={x['url']:x for x in resources}
    for name in ('/default.htm','/index.htm'):
        if name not in table:raise ValidationError('Missing native landing page '+name)
    default=static_text(img.unpacked,table['/default.htm'])
    index=static_text(img.unpacked,table['/index.htm'])
    if default!=index:raise ValidationError('Default and index entry pages disagree')
    parser=Scripts();parser.feed(default)
    if parser.frames:raise ValidationError('Old frame-based UI entry remains')
    if len(parser.paths)<14:raise ValidationError('Incomplete statically ordered UI loader')
    if len(parser.paths)!=len(set(parser.paths)) or any(('/'+p) not in table for p in parser.paths):
        raise ValidationError('Missing or repeated bundled script resources')
    if any(table['/'+p]['mime_enum']!=21 for p in parser.paths):
        raise ValidationError('Incorrect MIME type for a bundled script')
    return {'status':'offline-verified','file':path.name,'version':match.group(1),'size':len(raw),
            'sha256':digest,'factory_size_margin':FACTORY_SIZE-len(raw),
            'crc_valid':crc32_netos(raw)==0,'startup_resources':len(parser.paths),
            'entry_routes':['/default.htm','/index.htm'],
            'hardware_boot':'NOT_TESTED','flash_write':'NOT_TESTED','rollback':'NOT_AVAILABLE'}

def main(argv=None) -> int:
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument('firmware',type=Path)
    cli.add_argument('--sha256',required=True,help='Known expected firmware SHA-256')
    cli.add_argument('--json-output',type=Path,help='Write machine-readable preflight report')
    args=cli.parse_args(argv)
    try:
        result=verify(args.firmware,args.sha256)
    except (ValueError,OSError,UnicodeDecodeError) as exc:
        cli.exit(1,'Release preflight FAILED: '+str(exc)+'\n')
    data=json.dumps(result,indent=2)+'\n'
    print(data,end='')
    if args.json_output:
        args.json_output.parent.mkdir(parents=True,exist_ok=True)
        args.json_output.write_text(data,encoding='utf-8')
    return 0
if __name__=='__main__':sys.exit(main())
