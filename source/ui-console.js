/* H910P v0.5: local, desktop-first management surfaces; OEM XML is transport, never UI.
 * No network fetches except explicit same-origin NMC endpoints. No CDN.
 */
U.uiVersion='0.5.3';
U.config.loaded=false;
U.config.search='';
U.eventSearch='';
U.eventLevel='all';
U.editor=null;
U.consoleMeta={
 network:['Network configuration','Addressing, interfaces and name resolution','network','Current address, DNS, gateways and DHCP are provided by the NMC. Saving a network change may end this session.'],
 notification:['Notification delivery','Email relay and event recipients','mail','The original event engine delivers email even while this browser is closed. Other destinations require a supported relay or a future native transport.'],
 schedule:['Weekly schedule','Scheduled UPS operations','calendar','Schedule controls are displayed only when the firmware advertises editable schedule objects.'],
 shutdown:['Shutdown parameters','Grace periods and shutdown coordination','sliders','Changes here can affect the power-down sequence. Review each setting before applying.'],
 clients:['Shutdown clients','Network shutdown coordination','users','Client settings depend on the configured shutdown agents and the card’s firmware capabilities.'],
 access:['Access & security','Administrator and management access','lock','Review credentials and access policy before changing them. Secrets are never displayed.'],
 snmp:['SNMP management','Monitoring agents and trap destinations','signal','Keep monitoring available while editing community or trap destinations.'],
 time:['Time & synchronization','Local clock and NTP settings','clock','Clock accuracy matters for event timestamps and scheduled shutdowns.']
};
U.pretty=function(name){return String(name||'').replace(/^System\./,'').replace(/\[([0-9]+)\]/g,' $1 ').replace(/([A-Za-z])([0-9]+)/g,'$1 $2').replace(/([A-Z]+)([A-Z][a-z])/g,'$1 $2').replace(/([a-z0-9])([A-Z])/g,'$1 $2').replace(/[._]/g,' ').replace(/\s+/g,' ').trim().replace(/\b(Dhcp|Dns|Smtp|Snmp|Ntp|Ip|Mac|Ups|Url|Id)\b/g,function(s){return s.toUpperCase()})};
U.fieldCategory=function(name,group){
 var s=(name+' '+group).toLowerCase();
 if(/(smtp|mail|notify|notification|recipient|alert|email)/.test(s))return 'notification';
 if(/(snmp|trap|community|networkmanagementsystem)/.test(s))return 'snmp';
 if(/(ntp|timezone|time.zone|clock|timeserver|datetime)/.test(s))return 'time';
 if(/(weekly|schedule|calendar)/.test(s))return 'schedule';
 if(/(shutdown|client|agent|grace|autostart)/.test(s))return /clientcfg|client|agent/.test(s)?'clients':'shutdown';
 if(/(password|security|login|access|auth|user|https|ssh|telnet|session)/.test(s))return 'access';
 if(/(network|ethernet|ipv4|ipv6|ipaddress|gateway|netmask|subnet|dns|dhcp|hostname|domain|tcpport)/.test(s))return 'network';
 return 'system';
};
U.configFor=function(page){return (U.config.groups||[]).map(function(g){return {
 name:g.name,fields:g.fields.filter(function(f){return U.fieldCategory(f.name,g.name)===page})
 }}).filter(function(g){return g.fields.length})};
U.fieldHelp=function(f){var n=f.name.toLowerCase();
 if(/password|token|community|secret/.test(n))return 'Sensitive value. Existing credentials are never shown.';
 if(/dhcp/.test(n))return 'Let the network provide addressing automatically.';
 if(/gateway/.test(n))return 'Router used to reach destinations outside the subnet.';
 if(/mask|subnet/.test(n))return 'IPv4 subnet mask for this interface.';
 if(/dns/.test(n))return 'Name resolution for hostnames and notification endpoints.';
 if(/smtp|mailserver/.test(n))return 'Mail relay used by the card’s built-in notification engine.';
 if(/recipient|emailaddress/.test(n))return 'Destination for selected UPS events.';
 if(/ntp/.test(n))return 'Time server used to synchronize event timestamps.';
 if(/shutdown|delay|timer/.test(n))return 'Review the operating consequences before changing this delay.';
 if(f.restart)return 'Saving may restart the NMC network service.';
 return 'This setting is managed by the device.';
};
U.fieldMasked=function(f){return /password|community|secret|token|credential|privatekey/i.test(f.name)};
U.fieldValue=function(f){var raw=U.config.values[f.name];if(U.fieldMasked(f))return raw?'Configured — hidden':'Not reported';if(raw===undefined||raw===null||raw==='')return 'Not reported';if(f.type==='Boolean')return String(raw)==='1'?'Enabled':String(raw)==='0'?'Disabled':String(raw);return String(raw)+(f.unit?' '+f.unit:'')};
U.fieldInput=function(f,value){var type=f.type||'',kind=type.split(':')[0];var label=U.pretty(f.name),v=value==null?'':String(value),attrs='id="editor-value" name="editor-value" aria-label="'+U.esc(label)+'" required';
 if(kind==='Boolean')return '<select '+attrs+'><option value="1" '+(v==='1'?'selected':'')+'>Enabled</option><option value="0" '+(v==='0'?'selected':'')+'>Disabled</option></select>';
 if(kind==='List')return '<select '+attrs+'>'+type.slice(5).split(',').map(function(x){return '<option value="'+U.esc(x)+'" '+(x===v?'selected':'')+'>'+U.esc(U.pretty(x))+'</option>'}).join('')+'</select>';
 var secret=U.fieldMasked(f),inputType=secret?'password':kind==='Email'||/\.Email|MailAddress/i.test(f.name)?'email':'text';
 return '<input '+attrs+' type="'+inputType+'" value="'+U.esc(secret?'':v)+'" '+(secret?'autocomplete="new-password"':'autocomplete="off"')+' spellcheck="false" inputmode="'+(kind==='Integer'||kind==='Time'?'numeric':'text')+'">';
};
U.configToolbar=function(){return '<div class="settings-toolbar"><label class="search-field">'+U.ico('chart')+'<span class="sr-only">Search settings</span><input id="settings-search" type="search" placeholder="Search available device settings" value="'+U.esc(U.config.search)+'" autocomplete="off" aria-label="Search available device settings"></label><button id="load-config" type="button" class="btn btn-quiet" '+(U.config.busy?'disabled':'')+'>'+U.ico('refresh')+(U.config.busy?'Reading card…':'Refresh settings')+'</button></div>'};
U.configView=function(page){if(page==='notification')return U.notificationView();var meta=U.consoleMeta[page]||['Device settings','Available settings','sliders',''],groups=U.configFor(page),count=0;
 U.configFieldMap={};
 var head=U.heading('MANAGEMENT / '+page.toUpperCase(),meta[0],meta[1])+U.warning();
 var info='<div class="section-lead">'+U.ico(meta[2])+'<p>'+U.esc(meta[3])+'</p></div>';
 var notice=(U.config.warning?'<div class="notice info" role="status">'+U.ico('info')+'<span>'+U.esc(U.config.warning)+'</span></div>':'')+(U.config.error?'<div class="notice error" role="alert">'+U.ico('alert')+'<span>'+U.esc(U.config.error)+'</span></div>':'')+(U.config.message?'<div class="notice info" role="status">'+U.ico('check')+'<span>'+U.esc(U.config.message)+'</span></div>':'');
 var content='';
 if(U.config.busy&&!U.config.loaded)content='<div class="loading-lines" role="status">'+U.ico('refresh')+'<strong>Reading configuration from the NMC…</strong><p>Discovering the controls supported by this firmware.</p></div>';
 else if(!U.config.loaded)content='<div class="empty-state">'+U.ico('network')+'<strong>Waiting for device settings</strong><p>Select Refresh settings to try again.</p></div>';
 else if(!groups.length)content='<div class="empty-state">'+U.ico('info')+'<strong>No settings available for this section</strong><p>The device has not provided editable settings for this section. You may be able to find them in the advanced device pages.</p></div>';
 else content=groups.map(function(g){return '<section class="setting-section" aria-label="'+U.esc(U.pretty(g.name))+'"><div class="setting-section-head"><h2>'+U.esc(U.pretty(g.name))+'</h2><span>'+g.fields.length+' settings</span></div>'+g.fields.map(function(f){var id=count++;U.configFieldMap[id]=f;var label=U.pretty(f.name);return '<div class="setting-row" data-search="'+U.esc((label+' '+g.name+' '+U.fieldValue(f)).toLowerCase())+'"><div class="setting-copy"><strong>'+U.esc(label)+'</strong><p>'+U.esc(U.fieldHelp(f))+'</p></div><div class="setting-actions"><span class="setting-current" title="Current NMC value">'+U.esc(U.fieldValue(f))+'</span><button type="button" class="btn btn-quiet" data-edit="'+id+'" '+(U.config.saving?'disabled':'')+' aria-label="Edit '+U.esc(label)+'">Edit '+U.ico('arrow')+'</button></div></div>'}).join('')+'</section>'}).join('');
 var desc='<div class="inline-summary"><strong>'+count+'</strong><span>editable settings discovered</span><i></i><span>Changes require review and confirmation from the device</span></div>';
 return head+info+notice+(page==='notification'?U.notificationDestinations():'')+U.configToolbar()+desc+content+U.nativeContext(page);
};
U.nativeContext=function(page){var info=U.pageInfo(page);return info[5]?'<div class="device-reference"><span>'+U.ico('info')+' Some advanced settings are still available in the original device pages.</span><a href="'+U.esc(info[5])+'">Open advanced '+U.esc(info[1])+U.ico('external')+'</a></div>':''};
U.editorBody=function(f,index){return '<div class="editor-top">'+U.ico('sliders')+'<div><span>MANAGEMENT SETTING</span><h2 id="editor-title">'+U.esc(U.pretty(f.name))+'</h2></div></div><p class="editor-help">'+U.esc(U.fieldHelp(f))+'</p><div class="editor-current"><span>Current value</span><strong>'+U.esc(U.fieldValue(f))+'</strong></div><form id="editor-form"><label for="editor-value">New value</label>'+U.fieldInput(f,U.fieldMasked(f)?'':U.config.values[f.name])+'<small>'+U.esc(f.type)+(f.restart?' · Card restart may be required':'')+'</small><p class="editor-error" role="alert" id="editor-error" hidden></p><div class="editor-footer"><button type="button" class="btn btn-quiet" id="editor-cancel">Cancel</button><button type="submit" class="btn btn-primary" id="editor-next">Review change '+U.ico('arrow')+'</button></div></form>'};
U.editSetting=function(index){var f=U.configFieldMap[index],dialog=document.getElementById('setting-dialog');if(!f||!dialog||U.config.saving)return;if(!U.config.valuesLive){U.config.error='Editing is disabled until the card supplies live configuration values. No changes were submitted.';U.render(false);return}
 U.editor={field:f,index:index,step:'edit',value:''};dialog.innerHTML=U.editorBody(f,index);dialog.showModal();document.getElementById('editor-value').focus();};
U.reviewEdit=function(){if(!U.editor)return;var f=U.editor.field,input=document.getElementById('editor-value'),error=document.getElementById('editor-error'),value=input&&input.value;
 try{U.configValidate(f,value)}catch(e){error.hidden=false;error.textContent=e.message;input.focus();return}
 if(!U.fieldMasked(f)&&String(U.config.values[f.name])===value){error.hidden=false;error.textContent='This value is already configured.';return}
 U.editor.value=value;U.editor.step='review';var box=document.getElementById('setting-dialog');box.innerHTML='<div class="editor-top">'+U.ico('shield')+'<div><span>REVIEW BEFORE APPLYING</span><h2 id="editor-title">Confirm device change</h2></div></div><p class="editor-help">'+(f.restart?'This setting may restart the management card and disconnect your browser. ':'')+'The change will be written to the NMC, then read back where possible.</p><div class="review-line"><span>Setting</span><strong>'+U.esc(U.pretty(f.name))+'</strong></div><div class="review-line"><span>Current</span><strong>'+U.esc(U.fieldValue(f))+'</strong></div><div class="review-line"><span>Requested</span><strong>'+U.esc(U.fieldMasked(f)?'Hidden credential':value)+'</strong></div><p class="editor-error" role="alert" id="editor-error" hidden></p><div class="editor-footer"><button type="button" class="btn btn-quiet" id="editor-back">Back</button><button type="button" class="btn btn-primary" id="editor-apply">Apply to NMC '+U.ico('check')+'</button></div>';document.getElementById('editor-apply').focus();
};
U.commitEdit=function(){var ed=U.editor;if(!ed||ed.step!=='review'||U.config.saving)return;var f=ed.field,value=ed.value;
 var xml='<?xml version="1.0"?><SET_OBJECT><OBJECT name="'+U.esc(f.name)+'">'+U.esc(value)+'</OBJECT></SET_OBJECT>';
 var data=new FormData();data.append('objectList',new File([xml],'Configuration.xml',{type:'application/xml'}));
 U.config.saving=f.name;U.config.error='';U.config.message='';
 var apply=document.getElementById('editor-apply');if(apply){apply.disabled=true;apply.textContent='Applying…'}
 U.request('/Forms/set_obj_2',{method:'POST',body:data},15000).then(function(){
  return U.request('/get_object.xml',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'objectName='+encodeURIComponent(f.name)},10000)
 }).then(function(response){var found=U.xml(response),actual=found[f.name];
  if(U.fieldMasked(f)){U.config.message='Credential submitted to NMC. The card masks readback; verify authentication before ending this session.';return}
  if(String(actual)!==value)throw Error('Readback differs from the requested value; change not verified');
  U.config.values[f.name]=actual;U.config.message=U.pretty(f.name)+' updated. The device confirmed the change.';
 }).catch(function(e){U.config.error='Update not confirmed: '+e.message}).finally(function(){U.config.saving='';U.editor=null;var dialog=document.getElementById('setting-dialog');if(dialog)dialog.close();U.render(false)});
};
U.searchSettings=function(text){U.config.search=text;var rows=Array.from(document.querySelectorAll('.setting-row'));var q=text.toLowerCase().trim();rows.forEach(function(row){row.hidden=!!q&&!row.dataset.search.includes(q)});document.querySelectorAll('.setting-section').forEach(function(group){group.hidden=!Array.from(group.querySelectorAll('.setting-row')).some(function(r){return !r.hidden})});};
U.eventsView=function(){var has=U.events.length;
 var rows=U.events.map(function(raw,i){var entry=String(raw),severity=/fail|critical|alarm|low battery|fault/i.test(entry)?'critical':/warning|battery|shutdown|disconnect/i.test(entry)?'warning':'information';var m=entry.match(/^([^,]+),(.+)$/),time=m?m[1].replace(/^"|"$/g,'').trim():'',detail=m?m[2].trim():entry;
 return '<tr class="event-table-row" data-event="'+U.esc(entry.toLowerCase())+'" data-level="'+severity+'"><td><span class="event-level '+severity+'">'+U.esc(severity)+'</span></td><td class="event-detail">'+U.esc(detail)+'</td><td>'+U.esc(time||'NMC log')+'</td><td class="event-count">'+(i+1)+'</td></tr>'}).join('');
 return U.heading('EVENTS / HISTORY','Event history','Search and review events recorded by the card—not merely events observed in this browser.')+U.warning()+
 '<div class="event-summary"><div>'+U.ico('activity')+'<span><strong>'+has+'</strong> recent events</span></div><span>'+U.esc(U.eventsBusy?'Loading from card…':U.eventsError||'Sourced from the NMC event log')+'</span></div><section class="panel wide"><div class="section-title"><div><h2>Recorded activity</h2><p>Event severity is inferred from recorded message text</p></div><button type="button" class="btn btn-quiet" id="load-events" '+(U.eventsBusy?'disabled':'')+'>'+U.ico('refresh')+'Refresh log</button></div>'+
 '<div class="event-tools"><label class="search-field">'+U.ico('chart')+'<span class="sr-only">Search event messages</span><input id="event-search" type="search" placeholder="Search event messages" value="'+U.esc(U.eventSearch)+'"></label><label class="sr-only" for="event-level">Severity filter</label><select id="event-level"><option value="all">All severity</option><option value="critical" '+(U.eventLevel==='critical'?'selected':'')+'>Critical</option><option value="warning" '+(U.eventLevel==='warning'?'selected':'')+'>Warning</option><option value="information" '+(U.eventLevel==='information'?'selected':'')+'>Information</option></select></div>'+
 (has?'<div class="table-scroll"><table class="events-table"><thead><tr><th>Level</th><th>Event</th><th>Recorded</th><th>#</th></tr></thead><tbody>'+rows+'</tbody></table></div>':'<div class="empty-state">'+U.ico('calendar')+'<strong>'+U.esc(U.eventsBusy?'Reading event history…':U.eventsError?'Unable to read event history':'No events returned')+'</strong><p>'+U.esc(U.eventsError||'Use Refresh log to query the device again.')+'</p></div>')+'</section><div class="section-actions"><a class="btn btn-quiet" href="/logevent.csv" download>'+U.ico('upload')+'Download event CSV</a></div>';
};
U.filterEvents=function(){var q=U.eventSearch.toLowerCase().trim(),l=U.eventLevel;Array.from(document.querySelectorAll('.event-table-row')).forEach(function(r){r.hidden=!!q&&!r.dataset.event.includes(q)||l!=='all'&&r.dataset.level!==l})};
/* Keep branded logo inside the self-hosted UI package. Caller owns the image. */
U.embedLogo=function(uri){
 U.brandLogo=uri;
 var old=U.chrome;
 U.chrome=function(){
  return old()
   .replace('<span class="brand-mark">DELL<span class="brand-slash">/</span></span>',
    '<img class="dell-brand-logo" src="'+uri+'" width="150" height="48" alt="Dell">')
   .replace('</div></header>', '<button type="button" class="lock-action" id="lock-interface" title="Lock this management screen">'+U.ico('lock')+' Lock</button></div></header>')
   .replace('Firmware 01.18.0005','Firmware 01.18.0005 · UI v'+U.uiVersion)
   +'<dialog class="setting-dialog" id="setting-dialog" aria-labelledby="editor-title"></dialog>';
 };
};
U.management=function(){return U.page==='firmware'?U.firmwareView():U.configView(U.page)};
U.onConsoleClick=function(e){if(e.target.closest('#lock-interface')){U.lockInterface();return true}var edit=e.target.closest('[data-edit]');if(edit){U.editSetting(Number(edit.dataset.edit));return true}
 if(e.target.closest('#editor-cancel')){document.getElementById('setting-dialog').close();U.editor=null;return true}
 if(e.target.closest('#editor-back')){U.editSetting(U.editor.index);return true}
 if(e.target.closest('#editor-apply')){U.commitEdit();return true}
 return false;};
U.onConsoleInput=function(e){if(e.target.id==='settings-search')U.searchSettings(e.target.value);if(e.target.id==='event-search'){U.eventSearch=e.target.value;U.filterEvents()}};
U.onConsoleChange=function(e){if(e.target.id==='event-level'){U.eventLevel=e.target.value;U.filterEvents()}};
U.onConsoleSubmit=function(e){if(e.target.id==='editor-form'){e.preventDefault();U.reviewEdit()}};
U.installConsole=function(){
 var oldRender=U.render;U.render=function(navigate){oldRender(navigate);if(U.page==='events')U.filterEvents();if(U.config.search)U.searchSettings(U.config.search)};
 var nav=U.navto;U.navto=function(page){if(U.page!==page){U.config.message='';U.config.error='';U.config.search=''}nav(page);U.loadPageData()};
 U.loadPageData=function(){if(U.page==='events'&&!U.events.length&&!U.eventsBusy)U.loadEvents();else if(U.consoleMeta[U.page]||U.page==='controls'){if(!U.config.loaded&&!U.config.busy)U.loadConfig()}else if(U.page==='system'){if(!Object.keys(U.identity).length&&!U.identityBusy)U.loadIdentity();if(!U.config.loaded&&!U.config.busy)U.loadConfig()}};
 /* The authentication gate starts data loads after a successful sign-in. */
};
