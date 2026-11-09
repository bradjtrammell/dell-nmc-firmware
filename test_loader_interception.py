#!/usr/bin/env python3
"""Exercise actual packaged RomPager bootstrap and sequential load semantics.

The sandbox disallows navigation to even mocked HTTP hosts, so only the
script-fetch step is intercepted. The OEM startup script is executed unchanged.
"""
from pathlib import Path
import json
from playwright.sync_api import sync_playwright
ROOT=Path(__file__).resolve().parent
SITE=ROOT/'build'/'served-native-ui'
meta=json.loads((ROOT/'build'/'VALIDATION.json').read_text())
paths=meta['repurposed_original_help_pages']
assets={name:(SITE/name.lstrip('/')).read_text('ascii') for name in paths[1:]}
bootstrap=(SITE/paths[0].lstrip('/')).read_text('ascii')
init='''window.__loaded=[];
const assets=JSON.parse(document.getElementById('resources').textContent);
const append=document.head.appendChild.bind(document.head);
document.head.appendChild=function(node){
 if(node.tagName==='SCRIPT'&&node.getAttribute('src')&&assets[node.getAttribute('src')]){
  const name=node.getAttribute('src');window.__loaded.push(name);
  try{(0,eval)(assets[name]);Promise.resolve().then(()=>node.onload&&node.onload());}
  catch(error){Promise.resolve().then(()=>node.onerror&&node.onerror(error));}
  return node;
 }
 return append(node);
};
window.fetch=async function(url,opt){
 if(url==='/app_upload.htm')return {ok:true,status:200};
 if(url==='/logevent.csv')return {ok:true,status:200,text:async()=>''};
 if(url==='/get_object.xml')return {ok:true,status:200,text:async()=>'<GET_OBJECT><OBJECT name="UPS.PowerSummary.PercentLoad">53</OBJECT><OBJECT name="UPS.PowerSummary.RemainingCapacity">100</OBJECT><OBJECT name="UPS.PowerSummary.PresentStatus.ACPresent">1</OBJECT></GET_OBJECT>'};
 return {ok:false,status:404};
};'''
# JSON data in non-executable script node; no closing script sequence in chunks.
html='<!doctype html><html><head><script type="application/json" id="resources">'+json.dumps(assets)+'</script><script>'+init+'</script></head><body><main id="app"></main><script>'+bootstrap+'</script></body></html>'
with sync_playwright() as p:
 browser=p.chromium.launch(headless=True,executable_path='/usr/bin/chromium',args=['--no-sandbox'])
 page=browser.new_page()
 errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.set_content(html,wait_until='load')
 page.get_by_role('heading',name='Sign in to your UPS').wait_for(timeout=15000)
 assert page.evaluate('window.__loaded')==paths[1:]
 page.locator('#signin-user').fill('admin');page.locator('#signin-pass').fill('admin');page.locator('#signin-button').click()
 page.get_by_text('53%',exact=True).first.wait_for(timeout=15000)
 assert not errors,errors
 print(json.dumps({'dynamic_bootstrap':'PASS','sequential_resources':len(paths)-1,'sign_in':'PASS','dashboard':'PASS','javascript_errors':errors},indent=2))
 browser.close()
