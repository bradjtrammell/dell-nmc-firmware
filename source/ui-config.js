/* Dell documented configuration_descriptor.xml / Forms/set_obj_2 bridge.
 * Deliberately limited to fields marked RW; no power commands or hidden groups.
 * Every write is user-confirmed and read back through get_object.xml.
 */
U.config={groups:[],values:{},busy:false,error:'',message:'',saving:''};
U.configDoc=function(xml){
 var doc=new DOMParser().parseFromString(xml,'application/xml');
 if(doc.querySelector('parsererror'))throw Error('Invalid configuration descriptor');
 var groups=[];
 Array.from(doc.getElementsByTagName('GROUP')).forEach(function(group){
  if(group.getAttribute('hidden')==='TRUE')return;
  var fields=Array.from(group.getElementsByTagName('ITEM')).filter(function(el){
   var key=el.getAttribute('object')||'';
   return el.getAttribute('access')==='RW'&&/^System\.[A-Za-z0-9_.\[\]]{1,110}$/.test(key);
  }).map(function(el){return {name:el.getAttribute('object'),type:el.getAttribute('type')||'String:49',unit:el.getAttribute('unit')||'',restart:el.getAttribute('restart')==='TRUE'};});
  if(fields.length)groups.push({name:group.getAttribute('id')||'Settings',fields:fields});
 });
 if(!groups.length)throw Error('No editable configuration objects in descriptor');
 return groups;
};
U.loadConfig=function(){
 if(U.config.busy)return Promise.resolve();
 U.config.busy=true;U.config.error='';U.config.message='';U.render(false);
 return Promise.all([
  U.request('/configuration_descriptor.xml',{},10000),
  U.request('/get_object.xml',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'objectName=System.*'},10000)
 ]).then(function(response){
  U.config.groups=U.configDoc(response[0]);
  U.config.values=U.xml(response[1]);
 }).catch(function(e){U.config.error=e.message})
 .finally(function(){U.config.busy=false;U.render(false)});
};
U.configFor=function(page){
 var regex={network:/^Network$/i,system:/^System$/i,shutdown:/^Shutdown/i,notification:/^Email/i,access:/^Access$/i,snmp:/^Access$/i,controls:/^Control/i,schedule:/^Schedule/i,clients:/^Clients/i,time:/^Time/i,firmware:/^Firmware/i}[page];
 return (U.config.groups||[]).filter(function(g){return regex&&regex.test(g.name)}).map(function(g){
  return {name:g.name,fields:g.fields.filter(function(f){
   if(page==='snmp')return /SNMP|Community/i.test(f.name);
   if(page==='access')return !/SNMP|Community/i.test(f.name);
   return !/\.Password$|\.ReadCommunity$|\.WriteCommunity$/i.test(f.name)||/Password|Community/i.test(f.name);
  })};
 }).filter(function(g){return g.fields.length});
};
U.inputFor=function(f,i){
 var saved=U.config.values[f.name],secret=/Password|Community|Secret|Token/i.test(f.name),type=f.type;
 var label=f.name.replace(/^System\./,'').replace(/\[[0-9]+\]/g,function(s){return ' '+s});
 var val=secret?'':(saved==null?'':String(saved)),input='';
 if(type==='Boolean'){
  input='<select id="cfg-'+i+'" aria-label="'+U.esc(label)+'"><option value="">Select value</option><option value="1" '+(val==='1'?'selected':'')+'>Enabled</option><option value="0" '+(val==='0'?'selected':'')+'>Disabled</option></select>';
 }else if(type.indexOf('List:')===0){
  input='<select id="cfg-'+i+'" aria-label="'+U.esc(label)+'">'+type.slice(5).split(',').map(function(opt){return '<option value="'+U.esc(opt)+'" '+(val===opt?'selected':'')+'>'+U.esc(opt)+'</option>'}).join('')+'</select>';
 }else{
  input='<input id="cfg-'+i+'" type="'+(secret?'password':'text')+'" value="'+U.esc(val)+'" autocomplete="off" spellcheck="false" '+(secret?'placeholder="Enter new value"':'')+'>';
 }
 return '<div class="config-row"><label for="cfg-'+i+'"><strong>'+U.esc(label)+'</strong><small>'+U.esc(type)+(f.unit?' · '+U.esc(f.unit):'')+(f.restart?' · NMC restart required':'')+'</small></label><div class="config-edit">'+input+'<button type="button" class="btn btn-quiet" data-save="'+i+'" '+(U.config.saving?'disabled':'')+'>Apply</button></div></div>';
};
U.configView=function(page){
 var p=U.pageInfo(page),groups=U.configFor(page),i=0;
 var msg=U.config.message?'<div role="status" class="notice info">'+U.ico('check')+'<span>'+U.esc(U.config.message)+'</span></div>':'';
 var err=U.config.error?'<div role="alert" class="notice error">'+U.ico('alert')+'<span>'+U.esc(U.config.error)+'</span></div>':'';
 var html=U.heading('DEVICE CONFIGURATION',p[1],'Settings supplied by the NMC configuration descriptor, using the original management firmware.')+U.warning()+msg+err+
 '<section class="panel wide"><div class="section-title"><div><h2>Embedded configuration</h2><p>Each change requires confirmation and a device read-back check</p></div>'+U.btn(U.config.busy?'Loading...':'Load NMC settings','load-config','refresh')+'</div>';
 if(!U.config.groups.length){html+='<div class="empty-state">'+U.ico('sliders')+'<strong>No settings loaded</strong><p>Select Load NMC settings to query the card.</p></div>'}
 else if(!groups.length){html+='<div class="empty-state">'+U.ico('info')+'<strong>No supported editable fields in this section</strong><p>The original firmware management page remains accessible below.</p></div>'}
 else html+=groups.map(function(g){return '<div class="config-group"><h3>'+U.esc(g.name)+'</h3>'+g.fields.map(function(f){var id=i++;U.configFieldMap=U.configFieldMap||{};U.configFieldMap[id]=f;return U.inputFor(f,id)}).join('')+'</div>'}).join('');
 html+='</section><div class="section-actions">'+U.nativeLink('/default.htm','Full Dell management')+(p[5]?U.nativeLink(p[5],'Original '+p[1]+' page'):'')+'</div>';
 return html;
};
U.configValidate=function(f,value){
 var type=f.type,kind=type.split(':')[0],suffix=type.indexOf(':')>=0?type.slice(type.indexOf(':')+1):'';
 if(/[\x00-\x1f\x7f]/.test(value))throw Error('Control characters are not allowed');
 if(kind==='Boolean'&&value!=='0'&&value!=='1')throw Error('Choose Enabled or Disabled');
 if(kind==='IPv4'&&(!/^(\d{1,3}\.){3}\d{1,3}$/.test(value)||value.split('.').some(function(n){return Number(n)>255})))throw Error('Enter a valid IPv4 address');
 if(kind==='List'&&suffix.split(',').indexOf(value)<0)throw Error('Choose an available option');
 if(['String','Email','Password'].indexOf(kind)>=0){
  if(value.length>Number(suffix||49))throw Error('Value exceeds the card length limit');
  if(kind==='Email'&&value&&!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(value))throw Error('Enter a valid email address');
 }
 if(kind==='Integer'||kind==='Time'){
  if(!/^\d+$/.test(value))throw Error('Enter a nonnegative whole number');
  var match=suffix.match(/^(\d+)\.\.(\d+)/);
  if(match&&(Number(value)<Number(match[1])||Number(value)>Number(match[2])))throw Error('Value is outside the allowed range');
 }
 if(value.length===0)throw Error('A value is required');
 return value;
};
U.configEscape=function(value){return U.esc(value)};
U.saveConfig=function(index){
 if(U.config.saving||U.config.busy)return;
 var f=U.configFieldMap&&U.configFieldMap[index],el=document.getElementById('cfg-'+index);
 if(!f||!el)return;
 var value=el.value;
 try{U.configValidate(f,value)}catch(e){U.config.error=e.message;U.render(false);return}
 if(value===U.config.values[f.name]){U.config.message='No change to apply';U.config.error='';U.render(false);return}
 var warning=(f.restart?'The NMC may restart and disconnect after saving. ':'')+'Apply '+f.name+'?';
 if(!window.confirm(warning))return;
 var xml='<?xml version="1.0"?><SET_OBJECT><OBJECT name="'+U.configEscape(f.name)+'">'+U.configEscape(value)+'</OBJECT></SET_OBJECT>';
 var data=new FormData();data.append('objectList',new File([xml],'Configuration.xml',{type:'application/xml'}));
 U.config.saving=f.name;U.config.error='';U.config.message='';U.render(false);
 U.request('/Forms/set_obj_2',{method:'POST',body:data},15000).then(function(){
  return U.request('/get_object.xml',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'objectName='+encodeURIComponent(f.name)},10000);
 }).then(function(response){
  var current=U.xml(response)[f.name];
  if(current!==value)throw Error('Card did not confirm the new value; check original management');
  U.config.values[f.name]=current;U.config.message='Confirmed by NMC: '+f.name;
 }).catch(function(e){U.config.error='Save not confirmed: '+e.message})
 .finally(function(){U.config.saving='';U.render(false)});
};
/* Power commands are represented in the OEM descriptor as hidden Control.Outlet groups.
 * Never mix these imperative commands into ordinary editable configuration. */
U.power={groups:[],busy:false,message:'',error:''};
U.powerGroups=function(doc){var groups=[];Array.from(doc.getElementsByTagName('GROUP')).forEach(function(g){var m=(g.getAttribute('id')||'').match(/^Control\.Outlet\[(\d+)\]$/);if(!m)return;var cmds={};Array.from(g.getElementsByTagName('ITEM')).forEach(function(el){var name=el.getAttribute('object')||'',act=name.match(/^System\.Outlet\[(\d+)\]\.(OffDelay|OnDelay|Toggle)$/);if(act&&act[1]===m[1]&&el.getAttribute('access')==='RW')cmds[act[2]]=name});if(Object.keys(cmds).length)groups.push({number:Number(m[1]),commands:cmds})});return groups};
var U_originalConfigDoc=U.configDoc;
U.configDoc=function(xml){var d=new DOMParser().parseFromString(xml,'application/xml');if(!d.querySelector('parsererror'))U.power.groups=U.powerGroups(d);return U_originalConfigDoc(xml)};
U.controlsView=function(){var groups=U.power.groups;
 return U.heading('PROTECTED POWER CONTROL','UPS control','Native power controls using the original Dell controller commands.')+U.warning()+
 (U.power.error?'<div class="notice error" role="alert">'+U.ico('alert')+U.esc(U.power.error)+'</div>':'')+
 (U.power.message?'<div class="notice info" role="status">'+U.ico('info')+U.esc(U.power.message)+'</div>':'')+
 '<div class="instrument-grid power-instruments">'+U.metric('chart','Load',U.money(U.val('load'),'%'),'Present UPS capacity')+U.metric('battery','Battery',U.money(U.val('battery'),'%'),'Available backup charge')+U.metric('clock','Runtime',U.runtime(),'At the present load')+U.metric('zap','Output',U.money(U.val('output'),' V'),'Protected AC voltage')+'</div><section class="panel wide"><div class="section-title"><div><h2>Protected output groups</h2><p>Power actions require the precise confirmation phrase</p></div>'+U.btn(U.config.busy?'Loading...':'Load controls','load-config','refresh')+'</div>'+
 (groups.length?groups.map(function(g){var current=U.get('UPS.OutletSystem.Outlet['+g.number+'].PresentStatus.SwitchOnOff');return '<div class="output-row power-control"><div class="output-icon">'+U.ico('plug')+'</div><div><h2>Output group '+g.number+'</h2><p>'+U.esc(current==='1'?'Currently on':current==='0'?'Currently off':'State not reported')+'</p></div><div class="power-buttons">'+[['OnDelay','Power on'],['Toggle','Restart'],['OffDelay','Power off']].filter(function(a){return !!g.commands[a[0]]}).map(function(a){return '<button type="button" class="btn '+(a[0]==='OffDelay'?'danger-btn':'btn-quiet')+'" data-power="'+g.number+':'+a[0]+'" '+(U.power.busy?'disabled':'')+'>'+U.esc(a[1])+'</button>'}).join('')+'</div></div>'}).join(''):'<div class="empty-state">'+U.ico('shield')+'<strong>Control capability not loaded</strong><p>Load the NMC descriptor before enabling power actions.</p></div>')+'</section><div class="section-actions">'+U.nativeLink('/ups_cntr.htm','Original Dell power controls')+'</div>'};
U.sendPower=function(number,action){var group=U.power.groups.find(function(g){return g.number===number}),field=group&&group.commands[action];if(!field||U.power.busy)return;
 var verb={OnDelay:'POWER ON',OffDelay:'POWER OFF',Toggle:'RESTART'}[action],phrase=verb+' GROUP '+number;
 if(window.prompt('Power control: '+phrase+'\nType exactly: '+phrase)!==phrase)return;
 var xml='<SET_OBJECT><OBJECT name="'+U.esc(field)+'">0</OBJECT></SET_OBJECT>',form=new FormData();form.append('objectList',new File([xml],'Configuration.xml',{type:'application/xml'}));
 U.power.busy=true;U.power.error='';U.power.message='';U.render(false);
 U.request('/Forms/set_obj_2',{method:'POST',body:form},15000).then(function(){U.power.message=verb+' command submitted for output group '+number+'. Observe the reported output state to confirm completion.';return U.poll()}).catch(function(e){U.power.error='Power command could not be confirmed: '+e.message}).finally(function(){U.power.busy=false;U.render(false)});
};
