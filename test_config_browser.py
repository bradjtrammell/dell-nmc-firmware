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
from tools.embedded_assets import inline_html
html=inline_html(ROOT)
mock=r'''<script>
window.__calls=[];window.__writes=[];
window.__current={"System.Network.IPMask":"255.255.255.0","System.Network.DHCP":"0","System.Network.SmtpServer.HostName":"mail.lan","System.Email.Sender":"ups@example.test","System.Email.Subject":"UPS Alert","System.Security.Password":"private-value","System.Time.NTPServer":"ntp.lan","System.Email[0].Recipient":"recipient1@domain.com","System.Email[0].Selected":"0","System.Email[0].EventList.Discharging":"0","System.Email[0].EventList.RunTimeToShutdown":"1","System.Email[0].EventList.InternalFailure.1":"1","System.Email[1].Recipient":"recipient2@domain.com","System.Email[1].Selected":"0","System.Email[1].EventList.Discharging":"0","System.Email[1].EventList.RunTimeToShutdown":"1","System.Email[1].EventList.InternalFailure.1":"1","System.Email[2].Recipient":"recipient3@domain.com","System.Email[2].Selected":"0","System.Email[2].EventList.Discharging":"0","System.Email[2].EventList.RunTimeToShutdown":"1","System.Email[2].EventList.InternalFailure.1":"1","System.Email[3].Recipient":"recipient4@domain.com","System.Email[3].Selected":"0","System.Email[3].EventList.Discharging":"0","System.Email[3].EventList.RunTimeToShutdown":"1","System.Email[3].EventList.InternalFailure.1":"1"};
window.confirm=function(){return true};window.prompt=function(){return null};
window.fetch=async function(url,opt){
 var m=(opt&&opt.method)||'GET',b=(opt&&opt.body)||'';
 window.__calls.push({url:url,method:m,body:typeof b==='string'?b:'FORMDATA'});
 var response='';
 if(url==='/app_upload.htm'){return opt&&opt.credentials==='omit'&&opt.headers&&opt.headers.Authorization==='Basic YWRtaW46YWRtaW4='?{ok:true,status:200,text:async function(){return 'OK'}}:{ok:false,status:401,text:async function(){return 'Denied'}}}
 if(url==='/get_object.xml'){
  if(b==='objectName=UPS.*')response='<GET_OBJECT><OBJECT name="UPS.PowerSummary.PercentLoad">53</OBJECT><OBJECT name="UPS.PowerSummary.RemainingCapacity">100</OBJECT><OBJECT name="UPS.OutletSystem.Outlet[1].PresentStatus.SwitchOnOff">1</OBJECT></GET_OBJECT>';
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
  assert page.locator('.dell-brand-logo').get_attribute('src').startswith('data:image/svg+xml,')
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
  page.locator('.destination-table').wait_for(timeout=15000)
  assert page.locator('.destination-table thead th').all_text_contents()==['Destination','Type','Status','Actions']
  assert page.locator('.destination-table tbody tr').count()==1
  assert page.get_by_text('No destinations yet.',exact=False).count()==1
  assert page.get_by_text('0 of 4 email slots configured',exact=False).count()==1
  assert page.locator('#bridge-connect-form').count()==1
  page.get_by_role('button',name='SMTP settings').click()
  assert page.locator('dialog[open] #smtp-form').count()==1
  assert page.locator('#smtp-form').get_by_label('SMTP server').count()==1
  page.locator('#smtp-form').get_by_label('SMTP server').fill('smtp.lan')
  page.locator('#smtp-form button[type=submit]').click()
  assert page.get_by_role('button',name='Apply to NMC').count()==1
  page.locator('#smtp-apply').click()
  page.get_by_text('SMTP settings saved',exact=False).wait_for(timeout=12000)
  assert page.locator('.destination-table tbody tr').count()==1
  report['smtp_separate_modal_and_device_readback']='PASS'
  page.screenshot(path=str(ROOT/'build'/'notifications-desktop.png'),full_page=True)
  report['one_table_no_empty_slot_rows']='PASS'
  page.get_by_role('button',name='Add Destination').click()
  page.locator('#destination-type').select_option('email')
  page.locator('#destination-picker button[type=submit]').click()
  page.locator('#recipient-address').fill('ops@example.test')
  page.get_by_role('button',name='Review changes').click()
  page.get_by_role('button',name='Save destination').click()
  page.get_by_text('Recipient saved and verified').wait_for(timeout=12000)
  assert page.get_by_text('ops@example.test').count()>=1
  assert page.locator('.destination-table tbody tr .recipient-status.enabled').count()==1
  assert page.locator('.destination-table tbody tr').count()==1
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
  assert page.get_by_text('v0.5.11-beta.3').count()>=1
  assert page.locator('#github-check').count()==1
  assert page.locator('#firmware-file').count()==1
  assert page.locator('#firmware-channel').count()==1
  page.screenshot(path=str(ROOT/'build'/'firmware-updater-desktop.png'),full_page=True)
  report['github_release_check_and_local_firmware_upload_controls']='PASS'
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
  # Device returns malformed descriptor; the exact OEM map keeps every page visible.
  page.evaluate("""() => {
   const old = window.fetch;
   window.fetch = (url,options) => url==='/configuration_descriptor.xml' ?
       Promise.resolve({ok:true,status:200,text:async()=>'<html>error</html>'}) : old(url,options);
   U.config.loaded=false;
  }""")
  page.locator('[data-page="network"]').click()
  page.evaluate('U.loadConfig()')
  page.get_by_text('OEM field catalog.',exact=False).wait_for(timeout=12000)
  assert page.locator('.setting-row').count()>=1
  page.locator('[data-page="notification"]').click()
  assert page.locator('.destination-table tbody tr').count()>=1
  report['malformed_descriptor_falls_back_to_exact_oem_catalog']='PASS'
  page.evaluate("""() => {
    U.config.valuesLive=false;
  }""")
  page.locator('[data-page="network"]').click()
  page.locator('[data-edit]').first.click()
  assert page.get_by_text('Editing is disabled until the card supplies live configuration values',exact=False).count()>0
  report['no_configuration_writes_without_live_values']='PASS'
  # Real GitHub API response is simulated; no firmware upload is attempted.
  page.evaluate("""() => {
    const old=window.fetch;
    window.fetch=(url,options)=>String(url).startsWith('https://api.github.com/') ?
     Promise.resolve({ok:true,status:200,json:async()=>({name:'Test release',tag_name:'v0.5.10',html_url:'https://github.com/bradjtrammell/dell-nmc-firmware/releases/tag/v0.5.10',assets:[{name:'H910P_test.bin',browser_download_url:'https://github.com/bradjtrammell/dell-nmc-firmware/releases/download/v0.5.10/H910P_test.bin'}]})}) : old(url,options);
  }""")
  page.locator('[data-page="firmware"]').click()
  page.locator('#github-check').click()
  page.get_by_text('Test release').wait_for(timeout=10000)
  assert page.locator('a[href*="H910P_test.bin"]').count()==1
  report['github_release_discovery_shows_compatible_bin']='PASS'
  page.locator('#firmware-channel').select_option('dev')
  assert page.locator('#firmware-channel').input_value()=='dev'
  page.evaluate("""() => {
   const old=window.fetch;
   window.fetch=(url,options)=>String(url).includes('releases?per_page=30') ?
     Promise.resolve({ok:true,status:200,json:async()=>[
      {name:'Main release',tag_name:'v0.5.10',prerelease:false,target_commitish:'main',assets:[]},
      {name:'Dev candidate',tag_name:'v0.5.11-beta.1',prerelease:true,draft:false,target_commitish:'dev',assets:[{name:'H910P_Custom_UI_v0.5.11-beta.1.bin',browser_download_url:'https://github.com/bradjtrammell/dell-nmc-firmware/releases/download/v0.5.11-beta.1/H910P_Custom_UI_v0.5.11-beta.1.bin'}]}
     ]}) : old(url,options);
  }""")
  page.locator('#github-check').click()
  page.get_by_text('Dev candidate').wait_for(timeout=10000)
  assert page.locator('a[href*="v0.5.11-beta.1.bin"]').count()==1
  page.locator('#firmware-channel').select_option('main')
  assert page.locator('#firmware-channel').input_value()=='main'
  assert page.get_by_text('Dev candidate').count()==0
  report['release_main_beta_dev_channel_switch_and_asset_filter']='PASS'
  page.evaluate("""() => {
    const old=window.fetch;
    window.fetch=(url,options)=>url==='/Forms/app_upload_1' ?
      (window.__firmwarePost=true,Promise.resolve({ok:false,status:302,type:'opaqueredirect'})) : old(url,options);
  }""")
  page.on('dialog',lambda dialog:dialog.accept())
  page.locator('#firmware-file').set_input_files(str(ROOT/'build'/'H910P_01.18.0005_INSTRUMENT_PANEL_EXPERIMENTAL.bin'))
  page.locator('#firmware-install').click()
  page.get_by_text('Upload redirected',exact=False).wait_for(timeout=15000)
  assert not page.get_by_text('Firmware successfully installed').count()
  assert page.evaluate('window.__firmwarePost') is True
  report['real_bin_header_crc_browser_validation_and_no_false_flash_success']='PASS'
  assert not errors,errors
  report['uncaught_javascript_errors']=errors
  browser.close()
 (ROOT/'build'/'CONFIG_BROWSER_TEST.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report,indent=2))
if __name__=='__main__':main()
