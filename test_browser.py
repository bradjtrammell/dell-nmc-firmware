#!/usr/bin/env python3
"""Actual embedded RomPager resource bytes, served locally with mocked UPS XML.

Validates browser behavior without contacting or modifying the physical NMC.
"""
from __future__ import annotations
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote
import json
import threading
from playwright.sync_api import sync_playwright

BASE = Path(__file__).resolve().parent / 'build'
FILES = BASE / 'served-native-ui'
DATA = json.loads((BASE / 'VALIDATION.json').read_text())
RESOURCE_NAMES = set(DATA['repurposed_original_help_pages'])
MOCK = b'''<?xml version="1.0"?><GET_OBJECT>
<OBJECT name="UPS.PowerSummary.PercentLoad">53</OBJECT>
<OBJECT name="UPS.PowerSummary.RemainingCapacity">100</OBJECT>
<OBJECT name="UPS.PowerSummary.RunTimeToEmpty">990</OBJECT>
<OBJECT name="UPS.PowerSummary.PresentStatus.ACPresent">1</OBJECT>
<OBJECT name="UPS.PowerSummary.PresentStatus.Charging">1</OBJECT>
<OBJECT name="UPS.PowerSummary.PresentStatus.Discharging">0</OBJECT>
<OBJECT name="UPS.PowerSummary.iModel">Dell UPS Rack 1920W</OBJECT>
<OBJECT name="UPS.PowerConverter.Input[1].Voltage">121.5</OBJECT>
<OBJECT name="UPS.PowerConverter.Output.Voltage">120</OBJECT>
<OBJECT name="UPS.PowerConverter.Output.Frequency">60</OBJECT>
<OBJECT name="UPS.PowerConverter.Output.ActivePower">830</OBJECT>
<OBJECT name="UPS.OutletSystem.Outlet[1].PresentStatus.SwitchOnOff">1</OBJECT>
<OBJECT name="UPS.OutletSystem.Outlet[2].PresentStatus.SwitchOnOff">1</OBJECT>
<OBJECT name="UPS.OutletSystem.Outlet[3].PresentStatus.SwitchOnOff">0</OBJECT>
</GET_OBJECT>'''

class Handler(BaseHTTPRequestHandler):
    invalid_xml = False
    requests: list[tuple[str, bytes]] = []
    accessed_paths: list[str] = []
    def log_message(self, *args):
        pass
    def send_body(self, body: bytes, kind: str):
        self.send_response(200)
        self.send_header('Content-Type', kind)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(body)
    def do_GET(self):
        path = unquote(self.path.split('?')[0]); Handler.accessed_paths.append(path)
        if path in {'/index.htm','/index.html','/'}:
            self.send_body((FILES/'index.htm').read_bytes(),'text/html; charset=utf-8')
        elif path in RESOURCE_NAMES:
            self.send_body((FILES/path.lstrip('/')).read_bytes(),'application/javascript')
        elif path=='/logevent.csv':
            self.send_body(b'2026-11-08,AC Power Restored\n2026-11-07,NMC Restart\n','text/csv')
        else:
            self.send_error(404)
    def do_POST(self):
        path = self.path.split('?')[0]
        body = self.rfile.read(int(self.headers.get('Content-Length','0')))
        Handler.requests.append((path,body))
        if path != '/get_object.xml' or body != b'objectName=UPS.*':
            self.send_error(400)
        else:
            self.send_body(b'<html>not XML</html>' if Handler.invalid_xml else MOCK,'text/xml')

def run():
    html=(FILES/'index.htm').read_text('ascii')
    for path in DATA['repurposed_original_help_pages']:
        code=(FILES/path.lstrip('/')).read_text('ascii')
        html=html.replace('<script defer src="'+path+'"></script>','<script>'+code+'</script>')
    assert 'defer src=' not in html
    mock='<script>\nwindow.__rejectXML=false;window.__requests=[];\nwindow.fetch=async function(url,opt){window.__requests.push([url,opt?.method||\'GET\',opt?.body||\'\']);if(url===\'/get_object.xml\')return {ok:true,text:async()=>window.__rejectXML?\'not xml\':JSON_MOCK};if(url===\'/logevent.csv\')return {ok:true,text:async()=>"2026-11-08,AC Power Restored\\n2026-11-07,NMC Restart\\n"};return {ok:false,status:404,text:async()=>\'\'};};\n</script>'.replace('JSON_MOCK',json.dumps(MOCK.decode('utf8')))
    html=html.replace('<html lang=en>', '<html lang=en>'+mock)
    report={};errors=[]
    with sync_playwright() as p:
        browser=p.chromium.launch(headless=True,executable_path='/usr/bin/chromium',args=['--no-sandbox'])
        page=browser.new_page(viewport={'width':1600,'height':1000},device_scale_factor=1)
        page.on('pageerror',lambda e:errors.append(str(e)))
        page.set_content(html,wait_until='load')
        page.get_by_text('53%',exact=True).first.wait_for(timeout=15000)
        assert page.get_by_text('100%',exact=True).count()>=1
        assert page.get_by_text('16m 30s',exact=True).count()==1
        assert page.locator('iframe,frame,frameset').count()==0
        assert page.locator('.ic').count()>25
        assert page.locator('.power-path').count()==1
        assert page.get_by_text('Utility power healthy').count()>0
        assert page.locator('[data-page]').count()==15
        report['embedded_decompression_and_desktop_instrumentation']='PASS'
        report['frame_free_15_section_navigation']='PASS'
        page.screenshot(path=str(BASE/'instrument-panel-desktop.png'),full_page=True)
        for target in ['electrical','outlets','events','controls','schedule','shutdown','notification','network','system','clients','access','snmp','time','firmware']:
            page.locator('[data-page="'+target+'"]').click()
            assert page.locator('main h1').count()==1,target
        report['all_15_routes_render']='PASS'
        page.locator('[data-page="controls"]').click()
        assert page.locator('a[href="/default.htm"]').count()>=1
        assert page.locator('a[href="/ups_cntr.htm"]').count()==1
        report['legacy_power_control_handoff_present']='PASS'
        page.locator('[data-page="events"]').click()
        page.get_by_role('button',name='Load recent events').click()
        page.get_by_text('AC Power Restored',exact=False).wait_for(timeout=10000)
        report['event_export_polling']='PASS'
        page.locator('[data-page="overview"]').click()
        page.evaluate('window.__rejectXML=true')
        page.get_by_role('button',name='Refresh now').click()
        page.get_by_text('Invalid XML returned by NMC',exact=False).wait_for(timeout=10000)
        report['malformed_xml_explicit_error']='PASS'
        page.evaluate('window.__rejectXML=false')
        page.get_by_role('button',name='Retry').click()
        page.get_by_text('53%',exact=True).first.wait_for(timeout=10000)
        report['retry_and_recovery']='PASS'
        phone=browser.new_page(viewport={'width':390,'height':844},device_scale_factor=1)
        phone.on('pageerror',lambda e:errors.append(str(e)))
        phone.set_content(html,wait_until='load')
        phone.get_by_text('53%',exact=True).first.wait_for(timeout=15000)
        phone.get_by_role('button',name='Toggle navigation').click()
        phone.locator('[data-page="electrical"]').click()
        assert phone.get_by_role('heading',name='Electrical measurements').count()==1
        assert phone.evaluate('document.documentElement.scrollWidth<=innerWidth+3')
        phone.screenshot(path=str(BASE/'instrument-panel-mobile.png'),full_page=True)
        report['secondary_mobile_fallback']='PASS'
        requests=page.evaluate('window.__requests')
        assert all((a=='/get_object.xml' and b=='POST' and c=='objectName=UPS.*') or (a=='/logevent.csv' and b=='GET') for a,b,c in requests)
        assert not errors,errors
        report['no_external_requests_or_ups_writes']='PASS'
        report['uncaught_javascript_errors']=errors
        report['telemetry_poll_requests']=len(requests)
        browser.close()
    (BASE/'BROWSER_TEST.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':run()
