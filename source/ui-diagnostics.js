/* v0.5.4: independent same-origin XML diagnostics; never log response bodies,
 * credential headers, configuration values, or full URLs with query strings. */
U.descriptorDiagnostics={busy:false,result:null,error:''};
U.inspectDescriptor=function(){
 if(U.descriptorDiagnostics.busy)return Promise.resolve();
 var state=U.descriptorDiagnostics;
 state.busy=true;state.error='';state.result=null;U.render(false);
 var controller=new AbortController(),timer=setTimeout(function(){controller.abort()},10000);
 var headers={Accept:'application/xml,text/xml;q=0.9,*/*;q=0.1'};
 if(U.auth&&U.auth.header)headers.Authorization=U.auth.header;
 return fetch('/configuration_descriptor.xml',{method:'GET',credentials:'omit',cache:'no-store',headers:headers,signal:controller.signal})
 .then(function(response){
  var record={status:response.status,ok:response.ok,contentType:(response.headers.get('content-type')||'not provided').slice(0,100),length:0,root:'',groups:0,items:0,parseError:'',source:'live NMC'};
  return response.text().then(function(body){
   record.length=new TextEncoder().encode(body).length;
   if(record.length>1024*1024)throw Error('Settings response exceeded the 1 MiB diagnostic limit');
   var doc=new DOMParser().parseFromString(body,'application/xml');
   var bad=doc.getElementsByTagName('parsererror');
   if(bad.length){
    /* Parser errors may include snippets of secret configuration content; display only
       a generic classification and the status / MIME / length for safe reporting. */
    record.parseError='XML parser rejected the response';
   }else{
    record.root=doc.documentElement?doc.documentElement.nodeName.slice(0,80):'missing';
    record.groups=doc.getElementsByTagName('GROUP').length;
    record.items=doc.getElementsByTagName('ITEM').length;
    if(record.root!=='SYSTEM_CONFIG')record.parseError='Unexpected document root';
    else if(!record.groups||!record.items)record.parseError='No configuration groups or items found';
   }
   state.result=record;
  });
 }).catch(function(e){state.error=e.name==='AbortError'?'Settings diagnostic timed out after 10 seconds.':String(e.message||e).slice(0,160)})
 .finally(function(){clearTimeout(timer);state.busy=false;U.render(false)});
};
U.descriptorDiagnosticView=function(){
 var d=U.descriptorDiagnostics,r=d.result;
 var rows=r?[
  ['HTTP status',String(r.status)],['Content type',r.contentType],['Received bytes',String(r.length)],
  ['XML root',r.root||'unavailable'],['GROUP entries',String(r.groups)],['ITEM entries',String(r.items)],
  ['Validation',r.parseError|| (r.ok?'XML document parsed':'HTTP response rejected')]
 ]:[];
 return '<section class="setting-section" aria-label="Settings connection diagnostics"><div class="setting-section-head"><h2>Settings connection diagnostics</h2><span>Read-only device inspection</span></div><div class="maintenance-text"><p>Inspect the actual device settings endpoint without exposing your password or configuration values. This does not save or modify settings.</p><button type="button" id="diagnose-config" class="btn btn-quiet" '+(d.busy?'disabled':'')+'>'+U.ico('refresh')+(d.busy?' Inspecting…':' Inspect XML response')+'</button>'+
 (d.error?'<p class="notice error" role="alert">'+U.esc(d.error)+'</p>':'')+
 (r?'<div class="identity-grid">'+rows.map(function(row){return U.row(row[0],row[1])}).join('')+'</div><button type="button" id="copy-descriptor-diagnostic" class="btn btn-quiet">Copy safe diagnostic summary</button>':'')+'</div></section>';
};
U.diagnosticText=function(){
 var r=U.descriptorDiagnostics.result;if(!r)return '';
 return 'Dell H910P XML diagnostics\nHTTP: '+r.status+'\nContent-Type: '+r.contentType+'\nBytes: '+r.length+'\nRoot: '+(r.root||'unavailable')+'\nGROUP: '+r.groups+'\nITEM: '+r.items+'\nValidation: '+(r.parseError||'parsed');
};
var U_previousConfigView=U.configView;
U.configView=function(page){return U_previousConfigView(page)+U.descriptorDiagnosticView()};
var U_previousNotificationView=U.notificationView;
U.notificationView=function(){return U_previousNotificationView()+U.descriptorDiagnosticView()};
var U_previousConsoleClick=U.onConsoleClick;
U.onConsoleClick=function(e){
 if(e.target.closest('#diagnose-config')){U.inspectDescriptor();return true}
 if(e.target.closest('#copy-descriptor-diagnostic')){
  var report=U.diagnosticText();if(report&&navigator.clipboard&&navigator.clipboard.writeText){
   navigator.clipboard.writeText(report).catch(function(){U.descriptorDiagnostics.error='Clipboard access is unavailable. Select the displayed diagnostic values instead.';U.render(false)});
  }else{U.descriptorDiagnostics.error='Clipboard access is unavailable. Select the displayed diagnostic values instead.';U.render(false)}
  return true;
 }
 return U_previousConsoleClick(e);
};

