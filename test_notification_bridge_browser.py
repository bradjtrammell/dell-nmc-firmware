#!/usr/bin/env python3
"""Browser contract for multiple independent webhook destinations (mock relay)."""
from __future__ import annotations
import json
from pathlib import Path
from playwright.sync_api import sync_playwright
from test_config_browser import mock as DEVICE_MOCK
from tools.embedded_assets import inline_html

ROOT=Path(__file__).resolve().parent
RELAY_MOCK=r'''<script>
window.__destinations=[];window.__deliveryTests=0;
const originalDeviceFetch=window.fetch;
window.fetch=async function(url,opt){
 if(!String(url).startsWith('https://relay.example.test/api/v1/'))return originalDeviceFetch(url,opt);
 const path=String(url).slice('https://relay.example.test'.length),method=(opt&&opt.method)||'GET';
 if(!opt||!opt.headers||opt.headers.Authorization!=='Bearer relay-test-token')return {ok:false,status:401,json:async()=>({error:'Unauthorized'})};
 const parts=path.split('/').filter(Boolean),id=parts[3],old=window.__destinations.find(x=>x.id===id);
 let data={},status=200;
 if(method==='GET')data={destinations:window.__destinations.map(x=>({...x,token:undefined}))};
 else if(method==='POST'&&parts.length===3){let payload=JSON.parse(opt.body);payload.id='id'+(window.__destinations.length+1);window.__destinations.push(payload);status=201;data={id:payload.id};}
 else if(method==='POST'&&parts[4]==='test'&&old){window.__deliveryTests++;data={sent:true,status:200};}
 else if(method==='PUT'&&old){Object.assign(old,JSON.parse(opt.body));data={id:old.id};}
 else if(method==='DELETE'&&old){window.__destinations=window.__destinations.filter(x=>x.id!==id);data={deleted:true};}
 else {status=404;data={error:'Missing destination'};}
 return {ok:status>=200&&status<300,status:status,json:async()=>data};
};
</script>'''

def main():
    page_html=inline_html(ROOT).replace('<html>','<html>'+DEVICE_MOCK+RELAY_MOCK)
    report={};errors=[]
    with sync_playwright() as p:
        browser=p.chromium.launch(headless=True,executable_path='/usr/bin/chromium',args=['--no-sandbox'])
        page=browser.new_page(viewport={'width':1440,'height':900})
        page.on('pageerror',lambda e:errors.append(str(e)))
        page.set_content(page_html,wait_until='load')
        page.locator('#signin-user').fill('admin');page.locator('#signin-pass').fill('admin')
        page.locator('#signin-button').click()
        page.locator('#sidebar [data-page="notification"]').click()
        page.locator('.destination-table').wait_for(timeout=12000)
        assert page.locator('.destination-table tbody tr').count()==1
        page.evaluate('''() => {for(let i=0;i<4;i++){U.config.values['System.Email['+i+'].Recipient']='alerts'+i+'@example.test';U.config.values['System.Email['+i+'].Selected']='1'}U.render(false)}''')
        assert page.locator('.destination-table tbody tr').count()==4
        assert page.locator('#recipient-add').is_disabled()
        page.locator('#bridge-url').fill('https://relay.example.test')
        page.locator('#bridge-token').fill('relay-test-token')
        page.locator('#bridge-connect-form button[type=submit]').click()
        page.locator('#bridge-refresh').wait_for(timeout=10000)
        page.get_by_role('button',name='Add Destination').click()
        page.locator('#destination-type').select_option('email')
        page.locator('#destination-picker button[type=submit]').click()
        page.get_by_text('All four native email slots',exact=False).wait_for()
        assert page.locator('dialog[open] #recipient-form').count()==0
        assert page.locator('.destination-table tbody tr').count()==4
        for kind in ['monita','discord','ntfy','monita','discord','ntfy']:
            page.locator('#recipient-add').click()
            page.locator('#destination-type').select_option(kind)
            page.locator('#destination-picker button[type=submit]').click()
            page.locator('#bridge-target-name').fill(kind+' recipient')
            assert page.locator('[data-bridge-event]').count()==6
            page.locator('[data-bridge-event="power_restored"]').uncheck()
            if kind=='monita':
                page.locator('#bridge-target-url').fill('https://monita.example.net')
                page.locator('#bridge-target-token').fill('app-token')
            elif kind=='discord':
                page.locator('#bridge-target-url').fill('https://discord.com/api/webhooks/123456789/secure-token')
            else:
                page.locator('#bridge-target-url').fill('https://ntfy.example.net')
                page.locator('#bridge-target-topic').fill('h910p')
            page.locator('#bridge-destination-form button[type=submit]').click()
            page.wait_for_timeout(120)
        page.wait_for_function('window.__destinations.length===6',timeout=10000)
        assert all('power_restored' not in d['events'] for d in page.evaluate('window.__destinations'))
        page.locator('[data-bridge-test]').first.click()
        page.wait_for_function('window.__deliveryTests===1',timeout=10000)
        assert page.locator('.destination-table tbody tr').count()==10
        assert page.locator('.destination-table tbody tr').filter(has=page.locator('[data-bridge-test]')).count()==6
        assert page.get_by_text('6 external destinations',exact=False).count()>=1
        report['six_webhooks_not_limited_by_four_email_slots']='PASS'
        report['provider_specific_monita_discord_ntfy_forms']='PASS'
        report['authenticated_relay_test_request']='PASS'
        assert not errors,errors
        browser.close()
    (ROOT/'build'/'NOTIFICATION_BRIDGE_BROWSER_TEST.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
