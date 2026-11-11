/* Dell documented configuration_descriptor.xml / Forms/set_obj_2 bridge.
 * Deliberately limited to fields marked RW; no power commands or hidden groups.
 * Every write is user-confirmed and read back through get_object.xml.
 */
U.config={groups:[],values:{},busy:false,error:'',message:'',saving:''};
U.configFromCatalog=function(){
 var all=U.oemCatalog||[],groups=[];U.power.groups=[];
 all.forEach(function(g){
  var fields=g[2].map(function(f){return {name:f[0],type:f[1],unit:f[2],restart:!!f[3]}});
  var m=g[0].match(/^Control\.Outlet\[(\d+)\]$/);
  if(m){var cmds={};fields.forEach(function(f){var match=f.name.match(/\.(OffDelay|OnDelay|Toggle)$/);if(match)cmds[match[1]]=f.name});U.power.groups.push({number:Number(m[1]),commands:cmds});return}
  if(!g[1]&&fields.length)groups.push({name:g[0],fields:fields});
 });
 if(!groups.length)throw Error('Embedded OEM settings catalog is unavailable');
 return groups;
};
U.configDoc=function(xml){
 var doc=new DOMParser().parseFromString(xml,'application/xml');
 if(doc.querySelector('parsererror'))throw Error('Configuration endpoint did not return a valid XML descriptor');
 var groups=[];
 Array.from(doc.getElementsByTagName('GROUP')).forEach(function(group){
  if(group.getAttribute('hidden')==='TRUE')return;
  var fields=Array.from(group.getElementsByTagName('ITEM')).filter(function(el){
   var key=el.getAttribute('object')||'';
   return el.getAttribute('access')==='RW'&&/^(?:System|Environment)\.[A-Za-z0-9_.\[\]]{1,110}$/.test(key);
  }).map(function(el){return {name:el.getAttribute('object'),type:el.getAttribute('type')||'String:49',unit:el.getAttribute('unit')||'',restart:el.getAttribute('restart')==='TRUE'};});
  if(fields.length)groups.push({name:group.getAttribute('id')||'Settings',fields:fields});
 });
 if(!groups.length)throw Error('Configuration response has no editable OEM settings');
 return groups;
};
U.loadConfig=function(){
 if(U.config.busy)return Promise.resolve();
 U.config.busy=true;U.config.error='';U.config.message='';U.config.warning='';U.render(false);
 var descriptor=U.readXML('/configuration_descriptor.xml',{},10000).then(function(body){
  try{var parsed=U.configDoc(body);U.config.descriptorSource='device';return parsed}
  catch(e){U.config.warning='The card did not return a readable settings catalog. Showing the original firmware field catalog instead.';U.config.descriptorSource='embedded';return U.configFromCatalog()}
 }).catch(function(){U.config.descriptorSource='embedded';U.config.warning='The card settings catalog is unavailable. Showing the embedded OEM field catalog.';return U.configFromCatalog()});
 var values=U.readXML('/get_object.xml',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'objectName=System.*'},10000)
  .then(function(body){return U.xml(body)}).then(function(system){return U.readXML('/get_object.xml',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'objectName=Environment.*'},10000).then(function(body){return Object.assign(system,U.xml(body))}).catch(function(){return system})})
  .catch(function(e){U.config.warning=(U.config.warning?U.config.warning+' ':'')+'Live configuration values unavailable: '+e.message;return null});
 return Promise.all([descriptor,values]).then(function(result){
  U.config.groups=result[0];U.config.values=result[1]||{};
  U.config.valuesLive=!!result[1];U.config.loaded=true;
  /* Power actions cannot be advertised until their live descriptors are received. */
  if(U.config.descriptorSource==='embedded')U.power.groups=[];
 }).catch(function(e){U.config.loaded=false;U.config.error=e.message})
 .finally(function(){U.config.busy=false;U.render(false)});
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
/* Power commands are represented in the OEM descriptor as hidden Control.Outlet groups.
 * Never mix these imperative commands into ordinary editable configuration. */
U.power={groups:[],busy:false,message:'',error:''};
U.powerGroups=function(doc){var groups=[];Array.from(doc.getElementsByTagName('GROUP')).forEach(function(g){var m=(g.getAttribute('id')||'').match(/^Control\.Outlet\[(\d+)\]$/);if(!m)return;var cmds={};Array.from(g.getElementsByTagName('ITEM')).forEach(function(el){var name=el.getAttribute('object')||'',act=name.match(/^System\.Outlet\[(\d+)\]\.(OffDelay|OnDelay|Toggle)$/);if(act&&act[1]===m[1]&&el.getAttribute('access')==='RW')cmds[act[2]]=name});if(Object.keys(cmds).length)groups.push({number:Number(m[1]),commands:cmds})});return groups};
var U_originalConfigDoc=U.configDoc;
U.configDoc=function(xml){
 var d=new DOMParser().parseFromString(xml,'application/xml');
 if(d.querySelector('parsererror'))throw Error('Invalid device settings XML');
 U.power.groups=U.powerGroups(d);
 return U_originalConfigDoc(xml);
};
U.controlsView=function(){var groups=U.power.groups;
 return U.heading('PROTECTED POWER CONTROL','UPS control','Native power controls using the original Dell controller commands.')+U.warning()+
 (U.power.error?'<div class="notice error" role="alert">'+U.ico('alert')+U.esc(U.power.error)+'</div>':'')+
 (U.power.message?'<div class="notice info" role="status">'+U.ico('info')+U.esc(U.power.message)+'</div>':'')+
 '<div class="instrument-grid power-instruments">'+U.metric('chart','Load',U.money(U.val('load'),'%'),'Present UPS capacity')+U.metric('battery','Battery',U.money(U.val('battery'),'%'),'Available backup charge')+U.metric('clock','Runtime',U.runtime(),'At the present load')+U.metric('zap','Output',U.money(U.val('output'),' V'),'Protected AC voltage')+'</div><section class="panel wide"><div class="section-title"><div><h2>Protected output groups</h2><p>Power actions require the precise confirmation phrase</p></div>'+U.btn(U.config.busy?'Loading...':'Load controls','load-config','refresh')+'</div>'+
 (groups.length?groups.map(function(g){var current=U.get('UPS.OutletSystem.Outlet['+g.number+'].PresentStatus.SwitchOnOff');return '<div class="output-row power-control"><div class="output-icon">'+U.ico('plug')+'</div><div><h2>Output group '+g.number+'</h2><p>'+U.esc(current==='1'?'Currently on':current==='0'?'Currently off':'State not reported')+'</p></div><div class="power-buttons">'+[['OnDelay','Power on'],['Toggle','Restart'],['OffDelay','Power off']].filter(function(a){return !!g.commands[a[0]]}).map(function(a){return '<button type="button" class="btn '+(a[0]==='OffDelay'?'danger-btn':'btn-quiet')+'" data-power="'+g.number+':'+a[0]+'" '+(U.power.busy?'disabled':'')+'>'+U.esc(a[1])+'</button>'}).join('')+'</div></div>'}).join(''):'<div class="empty-state">'+U.ico('shield')+'<strong>Control capability not loaded</strong><p>Load the NMC descriptor before enabling power actions.</p></div>')+'</section>'};
U.sendPower=function(number,action){var group=U.power.groups.find(function(g){return g.number===number}),field=group&&group.commands[action];if(!field||U.power.busy)return;
 if(U.config.descriptorSource!=='device'||!U.config.valuesLive){U.power.error='Power control requires live device capability and configuration data.';U.render(false);return}
 var before=U.get('UPS.OutletSystem.Outlet['+number+'].PresentStatus.SwitchOnOff');if(before!=='0'&&before!=='1'){U.power.error='Current outlet state not confirmed. No power command was sent.';U.render(false);return}
 var verb={OnDelay:'POWER ON',OffDelay:'POWER OFF',Toggle:'RESTART'}[action],phrase=verb+' GROUP '+number;
 if(window.prompt('Power control: '+phrase+'\nType exactly: '+phrase)!==phrase)return;
 var xml='<SET_OBJECT><OBJECT name="'+U.esc(field)+'">0</OBJECT></SET_OBJECT>',form=new FormData();form.append('objectList',new File([xml],'Configuration.xml',{type:'application/xml'}));
 U.power.busy=true;U.power.error='';U.power.message='';U.render(false);
 U.request('/Forms/set_obj_2',{method:'POST',body:form},15000).then(function(){U.power.message=verb+' command submitted for output group '+number+'. Observe the reported output state to confirm completion.';return U.poll()}).catch(function(e){U.power.error='Power command could not be confirmed: '+e.message}).finally(function(){U.power.busy=false;U.render(false)});
};
