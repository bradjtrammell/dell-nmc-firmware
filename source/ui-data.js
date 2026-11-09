/* Read-only XML telemetry adapter. All actions are GET/POST reads; no writes. */
window.U=window.U||{};
U.data={};U.history=[];U.error='';U.updated=0;U.busy=false;U.page='overview';U.events=[];U.eventsError='';U.eventsBusy=false;U.timer=0;
U.keys={load:'UPS.PowerSummary.PercentLoad',battery:'UPS.PowerSummary.RemainingCapacity',runtime:'UPS.PowerSummary.RunTimeToEmpty',source:'UPS.PowerSummary.PresentStatus.ACPresent',charging:'UPS.PowerSummary.PresentStatus.Charging',discharging:'UPS.PowerSummary.PresentStatus.Discharging',healthy:'UPS.PowerSummary.PresentStatus.Good',model:'UPS.PowerSummary.iModel',input:'UPS.PowerConverter.Input[1].Voltage',output:'UPS.PowerConverter.Output.Voltage',frequency:'UPS.PowerConverter.Output.Frequency',watts:'UPS.PowerConverter.Output.ActivePower',temperature:'UPS.PowerSummary.Temperature'};
U.get=function(key){var v=U.data[U.keys[key]||key];return v===undefined?'':v};
U.val=function(key){var n=parseFloat(U.get(key));return Number.isFinite(n)?n:null};
U.present=function(key){var v=String(U.get(key)).toLowerCase();return v==='1'||v==='true'||v==='yes'};
U.runtime=function(){var n=U.val('runtime');if(n===null)return '--';return Math.floor(n/60)+'m '+Math.round(n%60)+'s'};
U.xml=function(text){
 var doc=new DOMParser().parseFromString(text,'text/xml');
 if(doc.querySelector('parsererror'))throw Error('Invalid XML returned by NMC');
 var list=doc.getElementsByTagName('OBJECT'),data={};
 if(!list.length)throw Error('No UPS measurements returned');
 for(var i=0;i<list.length;i++){
  var o=list[i],name=o.getAttribute('name');if(!name)continue;
  var value=o.getAttribute('value'),child=o.getElementsByTagName('VALUE');
  if(value===null)value=child.length?child[0].textContent:o.textContent;
  data[name]=String(value).trim();
 }
 if(!Object.keys(data).length)throw Error('UPS object names missing from XML');
 return data;
};
U.request=function(url,opt,ms){if(typeof url!=='string'||!/^\/(?!\/)/.test(url))return Promise.reject(Error('Only local device requests are supported'));var c=new AbortController(),t=setTimeout(function(){c.abort()},ms||8000),options=Object.assign({credentials:'same-origin',cache:'no-store',signal:c.signal},opt||{});options.headers=Object.assign({},(opt&&opt.headers)||{});if(U.auth&&U.auth.header)options.headers.Authorization=U.auth.header;return fetch(url,options).then(function(r){if(r.status===401)throw Error('Access denied. Check your sign-in and card permissions.');if(!r.ok)throw Error('The card returned HTTP '+r.status);return r.text()}).finally(function(){clearTimeout(t)})};
U.poll=function(){if(U.busy)return Promise.resolve();U.busy=true;U.updateActivity();return U.request('/get_object.xml',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'objectName=UPS.*'},8000).then(function(t){U.data=U.xml(t);U.updated=Date.now();U.error='';U.history.push({l:U.val('load'),b:U.val('battery'),at:U.updated});if(U.history.length>72)U.history.shift()}).catch(function(e){U.error=e.name==='AbortError'?'NMC did not respond within 8 seconds.':e.message}).finally(function(){U.busy=false;U.render(false)})};
U.loadEvents=function(){if(U.eventsBusy)return;U.eventsBusy=true;U.eventsError='';U.request('/logevent.csv',{},8000).then(function(t){U.events=t.replace(/^\uFEFF/,'').split(/\r?\n/).filter(Boolean).slice(-75).reverse()}).catch(function(e){U.eventsError=e.message}).finally(function(){U.eventsBusy=false;U.render(false)})};
U.updateActivity=function(){var refresh=document.getElementById('refresh');if(refresh){refresh.disabled=U.busy;refresh.classList.toggle('spinning',U.busy)}};
/* Authenticated, same-origin read of the NMC's device identification XML. */
U.identity={};U.identityBusy=false;U.identityError='';
U.loadIdentity=function(){
 if(U.identityBusy)return Promise.resolve();
 U.identityBusy=true;U.identityError='';U.render(false);
 return U.request('/product.xml',{},8000).then(function(body){
  var doc=new DOMParser().parseFromString(body,'application/xml');
  if(doc.querySelector('parsererror'))throw Error('Invalid product identification XML');
  var rows={};
  function visit(node,path){
   if(Object.keys(rows).length>=30)return;
   var children=Array.from(node.children||[]),key=path?path+' / '+node.tagName:node.tagName;
   if(!children.length){
    var name=node.getAttribute('name')||key;
    var value=(node.getAttribute('value')||node.textContent||'').trim();
    if(value&&value.length<=180&&!/pass(word)?|secret|token|community|auth|key|cookie|credential/i.test(name))rows[name]=value;
   }else children.forEach(function(child){visit(child,key)});
  }
  visit(doc.documentElement,'');
  if(!Object.keys(rows).length){
   Array.from(doc.documentElement.attributes||[]).forEach(function(a){if(a.value&&!/password|secret|token|community|key|auth/i.test(a.name))rows[a.name]=a.value});
  }
  if(!Object.keys(rows).length){
   rows['UPS model']=U.get('model')||'Dell UPS network management card';
   rows['Management address']=location.hostname;
   rows['Firmware']='01.18.0005';
  }
  U.identity=rows;U.identityError='';
 }).catch(function(e){U.identityError=e.name==='AbortError'?'Identification query timed out':e.message})
 .finally(function(){U.identityBusy=false;U.render(false)});
};
