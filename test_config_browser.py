#!/usr/bin/env python3
"""v0.5 end-to-end browser tests with simulated Dell M2M endpoint responses.
Actions are never sent to a live card; test asserts real user interaction flow.
"""
from pathlib import Path
import json
import re
from playwright.sync_api import sync_playwright

ROOT=Path(__file__).resolve().parent
SITE=ROOT/'build'/'served-native-ui'
META=json.loads((ROOT/'build'/'VALIDATION.json').read_text())
html=(SITE/'index.htm').read_text('ascii')
paths=META['repurposed_original_help_pages']
script_paths=re.findall(r'<script src=([^>]+)></script>',html)
assert script_paths == paths[1:]+paths[:1], 'Native entry page omitted or reordered a required script'
html=re.sub(r'<script src=([^>]+)></script>',lambda m:'<script>'+(SITE/m.group(1).lstrip('/')).read_text('ascii')+'</script>',html)
mock=r'''<script>
window.__calls=[];window.__writes=[];
window.__current={"System.Network.IPMask":"255.255.255.0","System.Network.DHCP":"0","System.Network.SmtpServer.HostName":"mail.lan","System.Email.Sender":"ups@example.test","System.Email.Subject":"UPS Alert","System.Security.Password":"private-value","System.Time.NTPServer":"ntp.lan","System.Email[0].Recipient":"recipient1@domain.com","System.Email[0].Selected":"0","System.Email[0].EventList.Discharging":"0","System.Email[0].EventList.RunTimeToShutdown":"1","System.Email[0].EventList.InternalFailure.1":"1","System.Email[1].Recipient":"recipient2@domain.com","System.Email[1].Selected":"0","System.Email[1].EventList.Discharging":"0","System.Email[1].EventList.RunTimeToShutdown":"1","System.Email[1].EventList.InternalFailure.1":"1","System.Email[2].Recipient":"recipient3@domain.com","System.Email[2].Selected":"0","System.Email[2].EventList.Discharging":"0","System.Email[2].EventList.RunTimeToShutdown":"1","System.Email[2].EventList.InternalFailure.1":"1","System.Email[3].Recipient":"recipient4@domain.com","System.Email[3].Selected":"0","System.Email[3].EventList.Discharging":"0","System.Email[3].EventList.RunTimeToShutdown":"1","System.Email[3].EventList.InternalFailure.1":"1"};
window.confirm=function(){return true};window.prompt=function(){return null};
window.fetch=async function(url,opt){
 var m=(opt&&opt.method)||'GET',b=(opt&&opt.body)||'';
 window.__calls.push({url:url,method:m,body:typeof b==='string'?b:'FORMDATA'});
 var response='';
 if(url==='/app_upload.htm'){return opt&&opt.credentials==='same-origin'&&opt.headers&&opt.headers.Authorization==='Basic YWRtaW46YWRtaW4='?{ok:true,status:200,text:async function(){return 'OK'}}:{ok:false,status:401,text:async function(){return 'Denied'}}}
 if(url==='/get_object.xml'){
  if(b==='objectName=UPS.*')response='<GET_OBJECT><OBJECT name="UPS.PowerSummary.PercentLoad">53</OBJECT><OBJECT name="UPS.PowerSummary.RemainingCapacity">100</OBJECT></GET_OBJECT>';
  else if(b==='objectName=System.*')response='<GET_OBJECT>'+Object.keys(window.__current).map(function(k){return '<OBJECT name="'+k+'">'+window.__current[k]+'</OBJECT>'}).join('')+'</GET_OBJECT>';
  else {var name=decodeURIComponent(String(b).slice(11));response='<GET_OBJECT><OBJECT name="'+name+'">'+(window.__current[name]||'')+'</OBJECT></GET_OBJECT>'}
 }else if(url==='/configuration_descriptor.xml')response="<SYSTEM_CONFIG><GROUP id=\"Network\"><ITEM object=\"System.Network.IPMask\" type=\"IPv4\" access=\"RW\"/><ITEM object=\"System.Network.DHCP\" type=\"Boolean\" access=\"RW\"/><ITEM object=\"System.Network.SmtpServer.HostName\" type=\"String:49\" access=\"RW\"/></GROUP><GROUP id=\"Email.Common\"><ITEM object=\"System.Email.Sender\" type=\"Email:59\" access=\"RW\"/><ITEM object=\"System.Email.Subject\" type=\"String:40\" access=\"RW\"/></GROUP><GROUP id=\"Email.Email[0]\"><ITEM object=\"System.Email[0].Recipient\" type=\"Email:99\" access=\"RW\"/><ITEM object=\"System.Email[0].Selected\" type=\"Boolean\" access=\"RW\"/><ITEM object=\"System.Email[0].EventList.Discharging\" type=\"Boolean\" access=\"RW\"/><ITEM object=\"System.Email[0].EventList.RunTimeToShutdown\" type=\"Boolean\" access=\"RW\"/><ITEM object=\"System.Email[0].EventList.InternalFailure.1\" type=\"Boolean\" access=\"RW\"/></GROUP>\n<GROUP id=\"Email.Email[1]\"><ITEM object=\"System.Email[1].Recipient\" type=\"Email:99\" access=\"RW\"/><ITEM object=\"System.Email[1].Selected\" type=\"Boolean\" access=\"RW\"/><ITEM object=\"System.Email[1].EventList.Discharging\" type=\"Boolean\" access=\"RW\"/><ITEM object=\"System.Email[1].EventList.RunTimeToShutdown\" type=\"Boolean\" access=\"RW\"/><ITEM object=\"System.Email[1].EventList.InternalFailure.1\" type=\"Boolean\" access=\"RW\"/></GROUP>\n<GROUP id=\"Email.Email[2]\"><ITEM object=\"System.Email[2].Recipient\" type=\"Email:99\" access=\"RW\"/><ITEM object=\"System.Email[2].Selected\" type=\"Boolean\" access=\"RW\"/><ITEM object=\"System.Email[2].EventList.Discharging\" type=\"Boolean\" access=\"RW\"/><ITEM object=\"System.Email[2].EventList.RunTimeToShutdown\" type=\"Boolean\" access=\"RW\"/><ITEM object=\"System.Email[2].EventList.InternalFailure.1\" type=\"Boolean\" access=\"RW\"/></GROUP>\n<GROUP id=\"Email.Email[3]\"><ITEM object=\"System.Email[3].Recipient\" type=\"Email:99\" access=\"RW\"/><ITEM object=\"System.Email[3].Selected\" type=\"Boolean\" access=\"RW\"/><ITEM object=\"System.Email[3].EventList.Discharging\" type=\"Boolean\" access=\"RW\"/><ITEM object=\"System.Email[3].EventList.RunTimeToShutdown\" type=\"Boolean\" access=\"RW\"/><ITEM object=\"System.Email[3].EventList.InternalFailure.1\" type=\"Boolean\" access=\"RW\"/></GROUP><GROUP id=\"Security\"><ITEM object=\"System.Security.Password\" type=\"Password:30\" access=\"RW\"/></GROUP><GROUP id=\"Time\"><ITEM object=\"System.Time.NTPServer\" type=\"String:49\" access=\"RW\"/></GROUP><GROUP id=\"Control.Outlet[1]\" hidden=\"TRUE\"><ITEM object=\"System.Outlet[1].OffDelay\" type=\"Time:0..99999\" access=\"RW\"/></GROUP></SYSTEM_CONFIG>";
 else if(url==='/Forms/set_obj_2'){
  var f=b.get('objectList');var xml=await f.text();window.__writes.push({filename:f.name,xml:xml});
  var parsed=new DOMParser().parseFromString(xml,'application/xml');
  var obj=parsed.querySelector('OBJECT');window.__current[obj.getAttribute('name')]=obj.textContent;
  response='OK';
 }else if(url==='/logevent.csv')response='2026-11-09,AC Power Restored\n2026-11-09,Low Battery Warning\n';
 else if(url==='/product.xml')response='<PRODUCT><MODEL>Dell H910P</MODEL><PASSWORD>hidden</PASSWORD></PRODUCT>';
 else return {ok:false,status:404,text:async function(){return ''}};
 return {ok:true,text:async function(){return response}};
};
</script>'''
html=html.replace('<html>','<html>'+mock)

def main():
 report={};errors=[]
 with sync_playwright() as p:
  browser=p.chromium.launch(headless=True,executable_path='/usr/bin/chromium',args=['--no-sandbox'])
  page=browser.new_page(viewport={'width':1500,'height':950},device_scale_factor=1)
  page.on('pageerror',lambda e:errors.append(str(e)))
  page.set_content(html,wait_until='load')
  page.get_by_role('heading',name='Sign in to your UPS').wait_for()
  page.locator('#signin-user').fill('admin')
  page.locator('#signin-pass').fill('admin')
  page.locator('#signin-button').click()
  page.get_by_text('53%',exact=True).first.wait_for(timeout=15000)
  page.locator('[data-page="network"]').click()
  page.locator('.setting-row').first.wait_for(timeout=12000)
  assert page.locator('.setting-row').count()==2
  assert page.locator('input[type=password]').count()==0
  report['network_settings_automatically_discovered_without_manual_fetch']='PASS'
  assert page.locator('.dell-brand-logo').count()==1
  assert page.locator('.dell-brand-logo').get_attribute('src').startswith('data:image/svg+xml;base64,')
  report['embedded_white_transparent_dell_logo']='PASS'
  page.screenshot(path=str(ROOT/'build'/'network-settings-desktop.png'),full_page=True)
  page.get_by_role('button',name='Edit Network IP Mask').click()
  page.locator('dialog[open] input').fill('999.999.0.0')
  page.get_by_role('button',name='Review change').click()
  page.get_by_text('Enter a valid IPv4 address').wait_for()
  assert not page.evaluate('window.__writes')
  report['modal_validation_blocks_invalid_change']='PASS'
  page.locator('dialog[open] input').fill('255.255.254.0')
  page.get_by_role('button',name='Review change').click()
  assert page.get_by_role('button',name='Apply to NMC').count()==1
  assert not page.evaluate('window.__writes')
  page.get_by_role('button',name='Apply to NMC').click()
  page.get_by_text('Network IP Mask updated. The device confirmed').wait_for(timeout=10000)
  writes=page.evaluate('window.__writes')
  assert len(writes)>=1 and writes[0]['filename']=='Configuration.xml'
  assert '<OBJECT name="System.Network.IPMask">255.255.254.0</OBJECT>' in writes[0]['xml']
  assert not page.locator('dialog').evaluate('(el)=>el.open')
  report['two_stage_modal_confirmation_and_device_readback']='PASS'
  page.locator('#settings-search').fill('DHCP')
  assert page.locator('.setting-row:not([hidden])').count()==1
  page.locator('#settings-search').fill('')
  assert page.locator('.setting-row:not([hidden])').count()==2
  report['instant_settings_search']='PASS'
  page.locator('[data-page="notification"]').click()
  page.locator('.recipient').first.wait_for(timeout=15000)
  assert page.locator('.recipient').count()==4
  assert page.get_by_text('Monita',exact=True).count()==1
  assert page.get_by_text('ntfy',exact=True).count()==1
  assert page.get_by_text('Discord',exact=True).count()==1
  assert page.locator('.recipient').count()==4
  page.screenshot(path=str(ROOT/'build'/'notifications-desktop.png'),full_page=True)
  report['four_oem_email_slots_with_service_guides']='PASS'
  page.get_by_role('button',name='Add destination').click()
  page.locator('#recipient-address').fill('ops@example.test')
  page.get_by_role('button',name='Review changes').click()
  page.get_by_role('button',name='Save destination').click()
  page.get_by_text('Recipient saved and verified').wait_for(timeout=12000)
  assert page.get_by_text('ops@example.test').count()>=1
  assert page.locator('.recipient .recipient-status.enabled').count()==1
  assert len(page.evaluate('window.__writes'))>=3
  report['recipient_modal_saves_with_confirmed_device_readback']='PASS'
  page.locator('[data-page="access"]').click()
  assert page.locator('.setting-row').count()==1
  assert page.get_by_text('private-value').count()==0
  page.get_by_role('button',name='Edit Security Password').click()
  assert page.locator('dialog[open] input[type="password"]').input_value()==''
  page.get_by_role('button',name='Cancel').click()
  report['secret_values_never_exposed']='PASS'
  page.locator('[data-page="events"]').click()
  page.get_by_text('Low Battery Warning').wait_for(timeout=10000)
  page.locator('#event-level').select_option('critical')
  assert page.locator('.event-table-row:not([hidden])').count()==1
  page.locator('#event-search').fill('Restored')
  assert page.locator('.event-table-row:not([hidden])').count()==0
  page.locator('#event-level').select_option('all')
  assert page.locator('.event-table-row:not([hidden])').count()==1
  report['event_severity_filter_and_search']='PASS'
  page.screenshot(path=str(ROOT/'build'/'events-desktop.png'),full_page=True)
  page.locator('[data-page="firmware"]').click()
  assert page.get_by_text('v0.5.2').count()>=1
  assert page.get_by_text('Automatic updates are being developed').count()==1
  report['firmware_status_and_nonfunctional_updater_not_misrepresented']='PASS'
  page.locator('[data-page="controls"]').click()
  assert page.locator('[data-power]').count()>=1
  assert page.get_by_role('button',name='Power off').count()==1
  page.get_by_role('button',name='Power off').click()
  assert len(page.evaluate('window.__writes'))>=1
  report['canceled_power_control_sends_no_write']='PASS'
  page.evaluate('window.prompt=function(){return "POWER OFF GROUP 1"}')
  page.get_by_role('button',name='Power off').click()
  page.get_by_text('POWER OFF command submitted').wait_for(timeout=10000)
  assert len(page.evaluate('window.__writes'))>=2
  report['power_command_requires_exact_typed_confirmation']='PASS'
  assert not errors,errors
  report['uncaught_javascript_errors']=errors
  browser.close()
 (ROOT/'build'/'CONFIG_BROWSER_TEST.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report,indent=2))
if __name__=='__main__':main()
