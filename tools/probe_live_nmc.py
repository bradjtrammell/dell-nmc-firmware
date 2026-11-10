#!/usr/bin/env python3
"""Read-only comparison of embedded NMC HTTP responses to a firmware BIN.

All NMC requests use GET. No firmware upload, configuration write, CGI POST,
or power-control requests are made. Credentials never appear in output.
"""
from __future__ import annotations
import argparse
import base64
from getpass import getpass
import hashlib
import json
from pathlib import Path
import sys
from urllib import error, parse, request

HERE=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(HERE/'tools'))
from h910p_rebuild import read_image
from rompager_map import discover


def static_bytes(image, descriptor):
    if not descriptor['terminated'] or any(item['type']!=1 for item in descriptor['items']):
        return None
    chunks=[]
    for item in descriptor['items']:
        pos=item['offset']
        end=image.unpacked.find(b'\0',pos,min(pos+8192,len(image.unpacked)))
        if end<0:raise ValueError(f'Unterminated resource {descriptor["url"]}')
        chunks.append(image.unpacked[pos:end])
    return b''.join(chunks)


def verify_response(actual, expected):
    if actual==expected:return {'match':True}
    pos=next((i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b),min(len(actual),len(expected)))
    return {'match':False,'first_differing_byte':pos,'expected_length':len(expected),'received_length':len(actual)}


def probe(card, username, password, names, expected, timeout):
    parsed=parse.urlsplit(card)
    if parsed.scheme not in ('http','https') or not parsed.hostname or parsed.username or parsed.password or parsed.path not in ('','/') or parsed.query or parsed.fragment:
        raise ValueError('Specify only the NMC base URL (http://hostname or https://hostname)')
    # Ignore environment proxies: never leak NMC LAN requests into a proxy.
    client=request.build_opener(request.ProxyHandler({}))
    encoded=base64.b64encode((username+':'+password).encode('utf-8')).decode('ascii')
    report={'card':parsed.scheme+'://'+parsed.netloc,'method':'GET only','resources':[]}
    try:
        for path in names:
            url=parse.urljoin(card.rstrip('/')+'/',path.lstrip('/'))
            req=request.Request(url,headers={
                'Authorization':'Basic '+encoded,
                'Cache-Control':'no-cache','Pragma':'no-cache',
                'Accept-Encoding':'identity','Connection':'close',
                'User-Agent':'H910P-ReadOnly-Resource-Verify/1.0'
            })
            info={'path':path}
            try:
                with client.open(req,timeout=timeout) as resp:
                    # Read at most 128 KiB; this is not a general HTTP scanner.
                    content=resp.read(131073)
                    info['http_status']=resp.status
                    info['content_type']=resp.headers.get('Content-Type','')
                    info['actual_bytes']=len(content)
                    info['expected_bytes']=len(expected[path])
                    info['sha256']=hashlib.sha256(content).hexdigest()
                    info.update(verify_response(content,expected[path]))
                    if parse.urlsplit(resp.url).path != path:
                        info['redirected_to_path']=parse.urlsplit(resp.url).path
                        info['match']=False
            except error.HTTPError as exc:
                info.update({'http_status':exc.code,'match':False,'error':'HTTP rejection'})
            except (error.URLError,TimeoutError,OSError) as exc:
                info.update({'match':False,'error':type(exc).__name__})
            report['resources'].append(info)
    finally:
        encoded=''
        password=''
    report['all_exact']=all(item.get('match') is True for item in report['resources'])
    return report


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--nmc',required=True,help='NMC base URL such as http://192.168.1.50')
    p.add_argument('--firmware',required=True,type=Path,help='Exact BIN expected to be running on the card')
    p.add_argument('--manifest',default=HERE/'build'/'VALIDATION.json',type=Path)
    p.add_argument('--user',default='admin',help='NMC user; password prompted securely')
    p.add_argument('--timeout',type=float,default=10)
    p.add_argument('--output',type=Path,help='Optional sanitized JSON result destination')
    args=p.parse_args()
    if args.timeout<1 or args.timeout>60:p.error('--timeout must be 1-60 seconds')
    firmware=args.firmware.read_bytes()
    img=read_image(firmware)
    _,descriptors=discover(img.unpacked)
    table={e['url']:e for e in descriptors}
    manifest=json.loads(args.manifest.read_text())
    expected_firmware=hashlib.sha256(firmware).hexdigest()
    if expected_firmware!=manifest['custom_sha256']:
        p.error('Firmware BIN SHA-256 does not match the manifest; cannot compare')
    names=['/index.htm',*manifest['repurposed_original_help_pages']]
    expected={name:static_bytes(img,table[name]) for name in names}
    if any(v is None for v in expected.values()):
        p.error('Firmware manifest contains nonstatic resource; cannot compare')
    password=getpass('NMC password (never saved): ')
    result=probe(args.nmc,args.user,password,names,expected,args.timeout)
    result['firmware_sha256']=expected_firmware
    output=json.dumps(result,indent=2)+'\n'
    print(output,end='')
    if args.output:args.output.write_text(output)
    return 0 if result['all_exact'] else 2

if __name__=='__main__':raise SystemExit(main())
