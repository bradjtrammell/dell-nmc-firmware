#!/usr/bin/env python3
"""Browser regression using the *unaltered* 713-byte firmware entry HTML.

External script responses are taken directly from the final BIN exports.
Requests are fulfilled in Chromium; unlike prior tests, no scripts are pre-
injected or manually eval'd, and no dynamic chunk loader is mocked.
"""
from pathlib import Path
from urllib.parse import urlparse
import json
from playwright.sync_api import sync_playwright

ROOT=Path(__file__).resolve().parent
SITE=ROOT/'build'/'served-native-ui'
meta=json.loads((ROOT/'build'/'VALIDATION.json').read_text())
paths=meta['repurposed_original_help_pages']
asset_map={name:(SITE/name.lstrip('/')).read_bytes() for name in paths}
native=(SITE/'index.htm').read_text('ascii')

mock='''
window.__requests=[];
window.fetch=async function(url,opt){
  window.__requests.push(url);
  if(url==='/app_upload.htm')return {ok:true,status:200};
  if(url==='/logevent.csv')return {ok:true,status:200,text:async()=>''};
  if(url==='/get_object.xml')return {ok:true,status:200,text:async()=>'<GET_OBJECT><OBJECT name="UPS.PowerSummary.PercentLoad">53</OBJECT><OBJECT name="UPS.PowerSummary.RemainingCapacity">100</OBJECT><OBJECT name="UPS.PowerSummary.PresentStatus.ACPresent">1</OBJECT></GET_OBJECT>'};
  return {ok:false,status:404,text:async()=>''};
};'''

# The browser sees the exact bytes of index.htm (which intentionally has no
# doctype, head or body to conserve embedded storage) plus the test harness.
page_markup='<html><head><base href="https://h910p.test/"><script>'+mock+'</script></head><body>'+native+'</body></html>'


def run(missing=None):
    requests=[];errors=[]
    with sync_playwright() as p:
        browser=p.chromium.launch(headless=True,executable_path='/usr/bin/chromium',args=['--no-sandbox'])
        page=browser.new_page(viewport={'width':1500,'height':900})
        page.on('pageerror',lambda e:errors.append(str(e)))
        def serve(route):
            pathname=urlparse(route.request.url).path
            requests.append(pathname)
            if pathname in asset_map and pathname != missing:
                route.fulfill(status=200,body=asset_map[pathname],content_type='application/javascript')
            else:
                route.fulfill(status=404,body='Not Found',content_type='text/plain')
        page.route('https://h910p.test/**',serve)
        page.set_content(page_markup,wait_until='load')
        if missing:
            page.get_by_text('Unable to load the custom interface:',exact=False).wait_for(timeout=15000)
            assert 'Incomplete assets (' in page.locator('#app').inner_text()
            assert page.locator('#app a[href="/app_upload.htm"]').count()==1
            assert page.get_by_role('heading',name='Sign in to your UPS').count()==0
        else:
            page.get_by_role('heading',name='Sign in to your UPS').wait_for(timeout=15000)
            page.locator('#signin-user').fill('admin')
            page.locator('#signin-pass').fill('admin')
            page.locator('#signin-button').click()
            page.get_by_text('53%',exact=True).first.wait_for(timeout=15000)
            assert page.locator('#sidebar [data-page]').count()==15
            assert not errors,errors
        assert requests==paths[1:]+paths[:1],(requests,paths)
        browser.close()
    return {'mode':'missing_resource' if missing else 'normal','native_index_bytes':len(native),'scripts_requested':len(requests),'requests_in_parser_order':True,'expected_result':'recovery_error' if missing else 'dashboard','uncaught_errors':errors}

if __name__=='__main__':
    good=run()
    bad=run(paths[6])
    result={'native_firmware_entry':good,'injected_missing_script':bad}
    (ROOT/'build'/'PARSER_ORDER_INTEGRATION.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
