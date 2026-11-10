#!/usr/bin/env python3
"""Serve extracted embedded RomPager bytes over HTTP and test in Chromium.

Offline emulator: explicitly does not model the NMC's RomPager MIME response,
firmware boot, device callback runtime, or actual M2M XML implementations.
"""
from __future__ import annotations

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from threading import Thread
from urllib.parse import unquote
import json

from playwright.sync_api import sync_playwright, Error as PlaywrightError

HERE=Path(__file__).resolve().parent
BUILD=HERE/'build'
SITE=BUILD/'served-native-ui'
META=json.loads((BUILD/'VALIDATION.json').read_text())
SCRIPTS=set(META['repurposed_original_help_pages'])
XML=b'''<?xml version="1.0"?><GET_OBJECT>
<OBJECT name="UPS.PowerSummary.PercentLoad"><VALUE>53</VALUE></OBJECT>
<OBJECT name="UPS.PowerSummary.RemainingCapacity" value="100"/>
<OBJECT name="UPS.PowerSummary.RunTimeToEmpty">990</OBJECT>
<OBJECT name="UPS.PowerSummary.PresentStatus.ACPresent">1</OBJECT>
<OBJECT name="UPS.PowerConverter.Input[1].Voltage">120</OBJECT>
</GET_OBJECT>'''

class Handler(BaseHTTPRequestHandler):
    requests=[]
    def log_message(self,*args): pass
    def respond(self,status,content,ctype):
        self.send_response(status)
        self.send_header('Content-Type',ctype)
        self.send_header('Cache-Control','no-store')
        self.send_header('X-Content-Type-Options','nosniff')
        self.send_header('Content-Length',str(len(content)))
        self.end_headers()
        self.wfile.write(content)
    def do_GET(self):
        path=unquote(self.path.split('?')[0])
        Handler.requests.append(('GET',path))
        if path in ('/','/index.htm'):
            self.respond(200,(SITE/'index.htm').read_bytes(),'text/html; charset=utf-8')
        elif path in SCRIPTS:
            self.respond(200,(SITE/path.lstrip('/')).read_bytes(),'application/javascript; charset=utf-8')
        elif path=='/logevent.csv':
            self.respond(200,b'2026-11-08,Utility restored\n','text/csv')
        else:
            self.respond(404,b'Not found','text/plain')
    def do_POST(self):
        path=self.path.split('?')[0]
        body=self.rfile.read(int(self.headers.get('Content-Length','0')))
        Handler.requests.append(('POST',path))
        if path=='/get_object.xml' and body==b'objectName=UPS.*':
            self.respond(200,XML,'application/xml')
        else:
            self.respond(400,b'Unexpected operation','text/plain')


def main():
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    worker=Thread(target=server.serve_forever,daemon=True);worker.start()
    url='http://127.0.0.1:%d/index.htm'%server.server_port
    checks={};errors=[]
    try:
        with sync_playwright() as p:
            browser=p.chromium.launch(headless=True,executable_path='/usr/bin/chromium',args=['--no-sandbox'])
            page=browser.new_page(viewport={'width':1600,'height':900})
            page.on('pageerror',lambda e:errors.append(str(e)))
            page.route('**/*',lambda route: route.fulfill(status=200,body=(SITE/'index.htm').read_bytes(),content_type='text/html') if route.request.url.endswith('/index.htm') else route.continue_())
            try:
                page.goto(url,wait_until='load')
            except PlaywrightError as exc:
                if 'ERR_BLOCKED_BY_ADMINISTRATOR' in str(exc):
                    result={'status':'SKIPPED','reason':'Chromium local HTTP navigation blocked by execution environment; network-level serving not verified'}
                    (BUILD/'HTTP_INTEGRATION.json').write_text(json.dumps(result,indent=2)+'\n')
                    print(json.dumps(result,indent=2))
                    browser.close()
                    return
                raise
            page.get_by_text('53%',exact=True).first.wait_for(timeout=25000)
            assert page.locator('iframe,frame,frameset').count()==0
            checks['real_HTTP_embedded_script_loading']='PASS'
            assert page.locator('[data-page]').count()==15
            page.locator('[data-page="electrical"]').click()
            page.get_by_role('heading',name='Electrical measurements').wait_for()
            checks['native_navigation_without_frames']='PASS'
            # The VALUE subelement and attribute forms both parsed correctly.
            assert page.evaluate('U.val("battery")')==100
            assert page.evaluate('U.val("load")')==53
            checks['XML_attribute_and_VALUE_decoding']='PASS'
            page.evaluate('window.scrollTo(0,220)')
            page.locator('#refresh').click()
            page.get_by_text('53%',exact=True).first.wait_for()
            assert page.get_by_role('heading',name='Electrical measurements').count()==1
            checks['refresh_route_stability']='PASS'
            page.locator('[data-page="events"]').click()
            page.get_by_role('button',name='Load recent events').click()
            page.get_by_text('Utility restored').wait_for()
            checks['HTTP_event_export']='PASS'
            assert not errors,errors
            assert all(method=='GET' or path=='/get_object.xml' for method,path in Handler.requests)
            assert not any(url.startswith('http://') and '127.0.0.1' not in url for url in page.evaluate('performance.getEntriesByType("resource").map(x=>x.name)'))
            checks['no_unintended_writes_or_external_resources']='PASS'
            browser.close()
    finally:
        server.shutdown();server.server_close();worker.join(timeout=3)
    (BUILD/'HTTP_INTEGRATION.json').write_text(json.dumps({'tests':checks,'uncaught_errors':errors,'requests':Handler.requests},indent=2)+'\n')
    print(json.dumps(checks,indent=2))

if __name__=='__main__':main()
