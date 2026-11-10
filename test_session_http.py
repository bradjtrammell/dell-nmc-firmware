#!/usr/bin/env python3
"""Chromium HTTP regression: same-tab reload, lock, XML retry and CGI serialisation.

The mock responds as an H910P-like HTTP endpoint; hardware is not accessed.
"""
import base64
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from threading import Lock, Thread
from urllib.parse import unquote
import json
import time
from playwright.sync_api import sync_playwright, Error as PlaywrightError

ROOT=Path(__file__).resolve().parent
SITE=ROOT/'build'/'served-native-ui'
META=json.loads((ROOT/'build'/'VALIDATION.json').read_text())
SCRIPTS=set(META['repurposed_original_help_pages'])
AUTH='Basic '+base64.b64encode(b'admin:admin').decode()
XML=b'''<?xml version="1.0"?><GET_OBJECT><OBJECT name="UPS.PowerSummary.PercentLoad">53</OBJECT><OBJECT name="UPS.PowerSummary.RemainingCapacity">100</OBJECT><OBJECT name="UPS.PowerSummary.RunTimeToEmpty">990</OBJECT><OBJECT name="UPS.PowerSummary.PresentStatus.ACPresent">1</OBJECT><OBJECT name="UPS.PowerConverter.Input[1].Voltage">120</OBJECT><OBJECT name="UPS.PowerConverter.Output.Voltage">120</OBJECT><OBJECT name="UPS.PowerConverter.Output.ActivePower">840</OBJECT></GET_OBJECT>'''

class Endpoint(BaseHTTPRequestHandler):
    bad_once=False
    bad_always=False
    allow_auth=True
    started=0
    completed=0
    inflight=0
    max_inflight=0
    mutex=Lock()
    def log_message(self,*args):pass
    def send(self,body,typ='text/plain',status=200):
        self.send_response(status)
        self.send_header('Content-Type',typ)
        self.send_header('Content-Length',str(len(body)))
        self.send_header('Cache-Control','no-store')
        if status==401:self.send_header('WWW-Authenticate','Basic realm="Network Management Card :"')
        self.end_headers()
        self.wfile.write(body)
    def authorized(self):
        if Endpoint.allow_auth and self.headers.get('Authorization')==AUTH:return True
        self.send(b'Unauthorized','text/plain',401)
        return False
    def do_GET(self):
        path=unquote(self.path.split('?',1)[0])
        if path in ('/','/index.htm','/default.htm'):
            self.send((SITE/'default.htm').read_bytes(),'text/html')
        elif path in SCRIPTS:
            self.send((SITE/path.lstrip('/')).read_bytes(),'application/javascript')
        elif path=='/app_upload.htm':
            if self.authorized():self.send(b'Firmware page','text/html')
        elif path=='/logevent.csv':
            if self.authorized():self.send(b'2026-11-08,Utility restored\n','text/csv')
        elif path=='/product.xml':
            if self.authorized():self.send(b'<PRODUCT><MODEL>Dell H910P</MODEL></PRODUCT>','application/xml')
        elif path=='/configuration_descriptor.xml':
            if self.authorized():self.send(b'<SYSTEM_CONFIG><GROUP id="Network"></GROUP></SYSTEM_CONFIG>','application/xml')
        else:self.send(b'Not found',status=404)
    def do_POST(self):
        path=unquote(self.path.split('?',1)[0])
        body=self.rfile.read(int(self.headers.get('Content-Length','0')))
        if not self.authorized():return
        if path!='/get_object.xml':self.send(b'Not found',status=404);return
        with Endpoint.mutex:
            Endpoint.inflight+=1
            Endpoint.max_inflight=max(Endpoint.max_inflight,Endpoint.inflight)
            Endpoint.started+=1
            is_bad=Endpoint.bad_once or Endpoint.bad_always
            Endpoint.bad_once=False
        try:
            time.sleep(.035)  # expose accidental overlapping GET_OBJECT requests
            self.send(b'<GET_OBJECT><OBJECT name="UPS.PowerSummary.PercentLoad">' if is_bad else XML,'application/xml')
        finally:
            with Endpoint.mutex:
                Endpoint.inflight-=1
                Endpoint.completed+=1

def main():
    server=ThreadingHTTPServer(('127.0.0.1',0),Endpoint)
    worker=Thread(target=server.serve_forever,daemon=True);worker.start()
    url=f'http://127.0.0.1:{server.server_port}/default.htm'
    results={};errors=[]
    try:
        with sync_playwright() as p:
            browser=p.chromium.launch(headless=True,executable_path='/usr/bin/chromium',args=['--no-sandbox'])
            page=browser.new_page(viewport={'width':1366,'height':900})
            page.on('pageerror',lambda e:errors.append(str(e)))
            try:
                page.goto(url,wait_until='load')
            except PlaywrightError as exc:
                if 'ERR_BLOCKED_BY_ADMINISTRATOR' in str(exc):
                    result={'status':'SKIPPED','reason':'Local browser navigation is blocked by execution environment; on-device reload not verified'}
                    (ROOT/'build'/'SESSION_HTTP_TEST.json').write_text(json.dumps(result,indent=2)+'\n')
                    print(json.dumps(result));browser.close();return
                raise
            page.get_by_role('heading',name='Sign in to your UPS').wait_for(timeout=20000)
            page.locator('#signin-user').fill('admin')
            page.locator('#signin-pass').fill('admin')
            page.get_by_role('button',name='Sign in').click()
            page.get_by_text('53%',exact=True).first.wait_for(timeout=20000)
            assert page.evaluate('!!sessionStorage.getItem(U.sessionKey)')
            results['authenticated_login_creates_tab_session']='PASS'
            page.reload(wait_until='load')
            page.get_by_text('53%',exact=True).first.wait_for(timeout=20000)
            assert page.locator('#signin-form').count()==0
            results['page_reload_restores_session_without_password']='PASS'
            # One malformed XML response is retried and must not flash an error.
            Endpoint.bad_once=True
            before=Endpoint.started
            page.locator('#refresh').click()
            page.wait_for_function('!U.busy',{ },timeout=20000) if False else page.wait_for_function('U.busy===false && U.updated>0',timeout=20000)
            page.wait_for_timeout(550)
            assert Endpoint.started>=before+2, (before,Endpoint.started)
            assert page.evaluate('U.error')==''
            results['single_bad_xml_response_is_retried']='PASS'
            # Sustained faults are shown after two failed sampling cycles.
            Endpoint.bad_always=True
            for _ in range(2):
                page.locator('#refresh').click()
                page.wait_for_timeout(950)
            assert page.evaluate('U.pollFailures')>=2
            assert 'XML' in page.evaluate('U.error')
            assert page.get_by_text('53%',exact=True).count()>=1
            results['sustained_xml_fault_retains_last_good_and_reports']='PASS'
            Endpoint.bad_always=False
            page.locator('#retry').click()
            page.wait_for_function("U.error==='' && U.pollFailures===0",timeout=20000)
            results['xml_error_recovers_without_relogin']='PASS'
            assert Endpoint.max_inflight==1, Endpoint.max_inflight
            results['nmc_xml_cgi_calls_serialized']='PASS'
            page.locator('#lock-interface').click()
            page.get_by_role('heading',name='Sign in to your UPS').wait_for(timeout=10000)
            assert page.evaluate('sessionStorage.getItem(U.sessionKey)') is None
            page.reload(wait_until='load')
            page.get_by_role('heading',name='Sign in to your UPS').wait_for(timeout=12000)
            results['lock_wipes_tab_credentials_and_stays_logged_out']='PASS'
            page.locator('#signin-user').fill('admin');page.locator('#signin-pass').fill('admin')
            page.get_by_role('button',name='Sign in').click()
            page.get_by_text('53%',exact=True).first.wait_for(timeout=15000)
            Endpoint.allow_auth=False
            page.reload(wait_until='load')
            page.get_by_role('heading',name='Sign in to your UPS').wait_for(timeout=15000)
            assert page.evaluate('sessionStorage.getItem(U.sessionKey)') is None
            Endpoint.allow_auth=True
            results['expired_credentials_removed_after_revalidation']='PASS'
            assert not errors,errors
            browser.close()
    finally:
        server.shutdown();server.server_close();worker.join(timeout=3)
    (ROOT/'build'/'SESSION_HTTP_TEST.json').write_text(json.dumps({'tests':results,'uncaught_errors':errors,'cgi_requests':Endpoint.started,'max_concurrent_cgi':Endpoint.max_inflight},indent=2)+'\n')
    print(json.dumps(results,indent=2))

if __name__=='__main__':main()
