import re
#!/usr/bin/env python3
"""Embedded configuration UI browser tests with a deterministic Dell M2M simulator.
No physical network card is contacted.
"""
from pathlib import Path
import json
from playwright.sync_api import sync_playwright

ROOT=Path(__file__).resolve().parent
SITE=ROOT/'build'/'served-native-ui'
META=json.loads((ROOT/'build'/'VALIDATION.json').read_text())
html=(SITE/'index.htm').read_text('ascii')
paths=META['repurposed_original_help_pages']
script_paths=re.findall(r'<script src=([^>]+)></script>',html)
assert script_paths == paths[1:] + paths[:1], 'Native entry page omits or reorders embedded scripts'
html=re.sub(r'<script src=([^>]+)></script>',
    lambda m:'<script>'+(SITE/m.group(1).lstrip('/')).read_text('ascii')+'</script>',html)
assert '<script src=' not in html
mock=r'''<script>
window.__calls=[];window.__writes=[];window.__current={'System.Network.IPMask':'255.255.255.0','System.Network.DHCP':'0'};
window.confirm=function(){return true};window.prompt=function(){return null};
window.fetch=async function(url,opt){
 var m=(opt&&opt.method)||'GET',b=(opt&&opt.body)||'';
 window.__calls.push({url:url,method:m,body:typeof b==='string'?b:'FORMDATA'});
 var response='';
 if(url==='/get_object.xml'){
  if(b==='objectName=UPS.*')response='<GET_OBJECT><OBJECT name="UPS.PowerSummary.PercentLoad">53</OBJECT><OBJECT name="UPS.PowerSummary.RemainingCapacity">100</OBJECT></GET_OBJECT>';
  else if(b==='objectName=System.*')response='<GET_OBJECT>'+Object.keys(window.__current).map(function(k){return '<OBJECT name="'+k+'">'+window.__current[k]+'</OBJECT>'}).join('')+'</GET_OBJECT>';
  else {var name=decodeURIComponent(String(b).slice(11));response='<GET_OBJECT><OBJECT name="'+name+'">'+(window.__current[name]||'')+'</OBJECT></GET_OBJECT>'}
 }else if(url==='/configuration_descriptor.xml')response='<SYSTEM_CONFIG><GROUP id="Network"><ITEM object="System.Network.IPMask" type="IPv4" access="RW"/><ITEM object="System.Network.DHCP" type="Boolean" access="RW"/><ITEM object="System.Password" type="Password:10" access="RW"/></GROUP><GROUP id="Control.Outlet[1]" hidden="TRUE"><ITEM object="System.Outlet[1].OffDelay" type="Time:0..99999" access="RW"/></GROUP></SYSTEM_CONFIG>';
 else if(url==='/Forms/set_obj_2'){
  var f=b.get('objectList');var xml=await f.text();window.__writes.push({filename:f.name,xml:xml});
  var parsed=new DOMParser().parseFromString(xml,'application/xml');
  var obj=parsed.querySelector('OBJECT');window.__current[obj.getAttribute('name')]=obj.textContent;
  response='OK';
 }else return {ok:false,status:404,text:async function(){return ''}};
 return {ok:true,text:async function(){return response}};
};
</script>'''
html=html.replace('<html>','<html>'+mock)

def main():
    report={};errors=[]
    with sync_playwright() as p:
        browser=p.chromium.launch(headless=True,executable_path='/usr/bin/chromium',args=['--no-sandbox'])
        page=browser.new_page(viewport={'width':1500,'height':950})
        page.on('pageerror',lambda e:errors.append(str(e)))
        page.set_content(html,wait_until='load')
        page.get_by_text('53%',exact=True).first.wait_for(timeout=15000)
        page.locator('[data-page="network"]').click()
        page.get_by_role('button',name='Load NMC settings').click()
        page.locator('.config-row').first.wait_for(timeout=10000)
        assert page.locator('.config-row').count()==3
        report['descriptor_driven_native_form']='PASS'
        page.screenshot(path=str(ROOT/'build'/'network-settings-desktop.png'),full_page=True)
        assert page.get_by_text('System.Outlet[1].OffDelay').count()==0
        assert page.locator('input[type=password]').count()==1
        assert page.locator('input[type=password]').input_value()==''
        report['hidden_actions_and_secrets_protected']='PASS'
        ip=page.locator('.config-row').first.locator('input')
        ip.fill('999.999.0.0')
        page.locator('.config-row').first.get_by_role('button',name='Apply').click()
        page.get_by_text('Enter a valid IPv4 address').wait_for()
        assert len(page.evaluate('window.__writes'))==0
        report['invalid_input_rejected_without_write']='PASS'
        ip=page.locator('.config-row').first.locator('input')
        ip.fill('255.255.254.0')
        page.locator('.config-row').first.get_by_role('button',name='Apply').click()
        page.get_by_text('Confirmed by NMC: System.Network.IPMask').wait_for(timeout=10000)
        writes=page.evaluate('window.__writes')
        assert len(writes)==1 and writes[0]['filename']=='Configuration.xml'
        assert '<OBJECT name="System.Network.IPMask">255.255.254.0</OBJECT>' in writes[0]['xml']
        report['authenticated_multipart_write_readback']='PASS'
        page.locator('[data-page="controls"]').click()
        assert page.locator('.config-row').count()==0
        report['power_actions_not_auto_exposed']='PASS'
        assert page.locator('[aria-current="page"]').count()==1
        assert page.locator('[aria-current="page"]').get_attribute('data-page')=='controls'
        assert page.locator('.side-nav button.is-active').get_attribute('data-page')=='controls'
        page.wait_for_timeout(200)
        page.screenshot(path=str(ROOT/'build'/'power-control-desktop.png'),full_page=True)
        page.get_by_role('button',name='Power off').click()
        assert len(page.evaluate('window.__writes'))==1
        report['power_action_cancel_does_not_write']='PASS'
        page.evaluate('window.prompt=function(){return "POWER OFF GROUP 1"}')
        page.get_by_role('button',name='Power off').click()
        page.get_by_text('POWER OFF command submitted for output group 1').wait_for(timeout=10000)
        pw=page.evaluate('window.__writes')
        assert len(pw)==2 and 'System.Outlet[1].OffDelay' in pw[1]['xml']
        report['power_action_requires_typed_confirmation']='PASS' 
        assert not errors,errors
        report['no_browser_exceptions']='PASS'
        browser.close()
    (ROOT/'build'/'CONFIG_BROWSER_TEST.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
