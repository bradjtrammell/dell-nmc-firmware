/* Notification recipients are backed by the four real Dell Email[0..3] groups.
 * Additional delivery services use an addressable SMTP receiver; the NMC does
 * not implement their native HTTPS webhook APIs. No fictitious install flags.
 */
U.recipientSlots=function(){
 var result=[];
 (U.config.groups||[]).forEach(function(g){
  var found=g.name.match(/^Email\.Email\[(\d+)\]$/);
  if(!found&&g.fields.length){found=g.fields[0].name.match(/^System\.Email\[(\d+)\]\./)}
  if(found&&Number(found[1])<4&&g.fields.some(function(f){return f.name==='System.Email['+found[1]+'].Recipient'})){
   result.push({index:Number(found[1]),fields:g.fields});
  }
 });
 return result.sort(function(a,b){return a.index-b.index}).slice(0,4);
};
U.recipientField=function(slot,suffix){return slot.fields.find(function(f){return f.name==='System.Email['+slot.index+'].'+suffix})};
U.recipientValue=function(slot,suffix){return String(U.config.values['System.Email['+slot.index+'].'+suffix]||'')};
U.recipientActive=function(slot){return U.recipientValue(slot,'Selected')==='1'};
U.recipientConfigured=function(slot){var address=U.recipientValue(slot,'Recipient');return address&& !/^recipient\d+@domain\.com$/i.test(address)};
U.mailFields=function(){var fields=[];(U.config.groups||[]).forEach(function(g){g.fields.forEach(function(f){
 if(/^System\.(Network\.SmtpServer\.|Email\.(Sender|Subject|Message))/.test(f.name))fields.push(f);
 })});return fields};
U.mailFieldLabel=function(f){var n=f.name;return /HostName$/.test(n)?'SMTP server':/SmtpPort$/.test(n)?'SMTP port':/Authentication$/.test(n)?'Authentication':/\.Login$/.test(n)?'Username':/\.Password$/.test(n)?'Password':/\.Sender$/.test(n)?'From address':/\.Subject$/.test(n)?'Message subject':'Custom message'};
U.mailFieldInfo=function(f){return /HostName$/.test(f.name)?'Address of the mail server used to deliver alerts.':/SmtpPort$/.test(f.name)?'Port used to contact your outgoing mail server.':/Authentication$/.test(f.name)?'Whether the mail server expects login credentials.':/\.Sender$/.test(f.name)?'Address shown as the sender of alert emails.':'Managed by the UPS built-in email service.'};
U.notificationView=function(){
 var slots=U.recipientSlots(),active=slots.filter(function(s){return U.recipientActive(s)}).length;
 var head=U.heading('NOTIFICATIONS','Notification center','Send UPS alerts to people or services using the card’s built-in email delivery.')+U.warning();
 var alert=U.config.message?'<div class="notice info" role="status">'+U.ico('check')+U.esc(U.config.message)+'</div>':U.config.error?'<div class="notice error" role="alert">'+U.ico('alert')+U.esc(U.config.error)+'</div>':'';
 if(!U.config.loaded)return head+alert+'<div class="empty-state"><strong>'+U.esc(U.config.busy?'Reading notification settings…':'Notification settings are not available')+'</strong><p>'+U.esc(U.config.error||'The card must provide its email configuration before you can manage recipients.')+'</p><button type="button" class="btn btn-primary" id="load-config">Try again</button></div>';
 U.configFieldMap={};var fields=U.mailFields();fields.forEach(function(f,i){U.configFieldMap[i]=f});
 var cards=slots.map(function(slot){var n=slot.index,enabled=U.recipientActive(slot),configured=U.recipientConfigured(slot),address=U.recipientValue(slot,'Recipient');
  return '<article class="recipient"><div class="recipient-index">'+U.ico('mail')+'</div><div class="recipient-main"><div class="recipient-title"><h3>'+U.esc(configured?address:'Available recipient')+'</h3><span class="recipient-status '+(enabled?'enabled':'')+'">'+(enabled?'Active':'Inactive')+'</span></div><p>'+U.esc(configured?'Alerts are delivered through the UPS email service.':'Add an email address to use this notification destination.')+'</p><div class="recipient-buttons"><button type="button" class="btn btn-quiet" data-recipient="'+n+'">'+U.ico('sliders')+(configured?'Manage recipient':'Add recipient')+'</button>'+(configured&&enabled?'<button type="button" class="btn btn-quiet" data-disable-recipient="'+n+'">Disable</button>':'')+'</div></div></article>';
 }).join('');
 var transport=fields.length?'<section class="setting-section"><div class="setting-section-head"><h2>Outgoing email</h2><span>Mail delivery settings</span></div>'+fields.map(function(f,i){return '<div class="setting-row"><div class="setting-copy"><strong>'+U.esc(U.mailFieldLabel(f))+'</strong><p>'+U.esc(U.mailFieldInfo(f))+'</p></div><div class="setting-actions"><span class="setting-current">'+U.esc(U.fieldValue(f))+'</span><button type="button" data-edit="'+i+'" class="btn btn-quiet">Edit '+U.ico('arrow')+'</button></div></div>'}).join('')+'</section>':'<div class="notice info">Email server settings were not advertised by this firmware.</div>';
 return head+alert+'<div class="notification-hero"><div class="notification-stat"><strong>'+active+' / '+slots.length+'</strong><span>Active email destinations</span></div><div><h2>Alerts delivered from the UPS</h2><p>Notifications continue to work when this management page is closed. All destinations use the card’s configured mail server.</p></div><button type="button" id="load-config" class="btn btn-quiet" '+(U.config.busy?'disabled':'')+'>'+U.ico('refresh')+' Refresh</button></div>'+
 '<div class="recipient-heading"><div><h2>Alert recipients</h2><p>Up to '+slots.length+' destinations supported by this card</p></div>'+(slots.some(function(s){return !U.recipientActive(s)})?'<button type="button" id="recipient-add" class="btn btn-primary">'+U.ico('mail')+' Add destination</button>':'<span class="recipient-limit">All recipient slots are active</span>')+'</div>'+
 (slots.length?'<div class="recipient-grid">'+cards+'</div>':'<div class="empty-state">This device did not advertise editable email recipient slots.</div>')+
 '<section class="integration-section"><h2>Connect other notification apps</h2><p>Use a compatible inbound email address to route UPS alerts to your notification service. Direct webhook delivery is not yet supported by this firmware.</p><div class="integration-grid">'+[
  ['ntfy','Email receiver','Configure the email address accepted by your ntfy server.'],
  ['Monita','Inbound email','Route UPS alerts into a Monita channel through its email integration.'],
  ['Discord','SMTP-to-webhook bridge','Provide the email address of an existing bridge that forwards to your Discord webhook.']
 ].map(function(row){return '<div class="integration-card"><strong>'+row[0]+'</strong><span>'+row[1]+'</span><p>'+row[2]+'</p><button type="button" class="btn btn-quiet" data-integration="'+row[0]+'">Set up via email '+U.ico('arrow')+'</button></div>'}).join('')+'</div></section>'+transport;
};
U.findRecipient=function(index){return U.recipientSlots().find(function(s){return s.index===index})};
U.emailEventChoices=[['EventList.Discharging','Running on battery'],['EventList.ACPresent','Utility power restored'],['EventList.RunTimeToShutdown','Shutdown approaching'],['EventList.BelowRemainingCapacityLimit','Low battery capacity'],['EventList.InternalFailure.1','Internal UPS failure'],['EventList.CommunicationLost.1','Communication lost'],['EventList.FirmwareUpgrade','Firmware update']];
U.openRecipient=function(index,source){
 var slot=U.findRecipient(index),dialog=document.getElementById('setting-dialog');
 if(!slot||!dialog||U.config.saving)return;
 U.recipientDraft={index:index,source:source||'Email'};
 var enabled=U.recipientActive(slot),address=U.recipientConfigured(slot)?U.recipientValue(slot,'Recipient'):'';
 var options=U.emailEventChoices.filter(function(c){return U.recipientField(slot,c[0])}).map(function(c){return '<label class="event-option"><input type="checkbox" data-event-field="'+c[0]+'" '+(U.recipientValue(slot,c[0])==='1'?'checked':'')+'><span>'+U.esc(c[1])+'</span></label>'}).join('');
 dialog.innerHTML='<div class="editor-top">'+U.ico('mail')+'<div><span>NOTIFICATION RECIPIENT</span><h2 id="editor-title">'+(address?'Manage destination':'Add a destination')+'</h2></div></div>'+(source&&source!=='Email'?'<p class="editor-help">'+U.esc(source)+' can receive alerts through an email gateway that you configure on its server. Enter that gateway’s receiving address. This does not install a native integration.</p>':'<p class="editor-help">Choose an email address and the UPS events that should trigger notifications.</p>')+
 '<form id="recipient-form"><label for="recipient-address">Receiving email address</label><input id="recipient-address" type="email" name="recipient" maxlength="99" autocomplete="off" required value="'+U.esc(address)+'" placeholder="alerts@example.com">'+(U.recipientField(slot,'Selected')?'<label class="recipient-enable"><input id="recipient-enabled" type="checkbox" '+(enabled||!address?'checked':'')+'> <span>Enable this destination</span></label>':'')+'<fieldset class="event-choice"><legend>Send alerts for</legend>'+options+'</fieldset><p class="editor-error" id="recipient-error" role="alert" hidden></p><div class="editor-footer"><button class="btn btn-quiet" type="button" id="recipient-cancel">Cancel</button><button class="btn btn-primary" type="submit">Review changes '+U.ico('arrow')+'</button></div></form>';
 dialog.showModal();document.getElementById('recipient-address').focus();
};
U.reviewRecipient=function(){
 var slot=U.findRecipient(U.recipientDraft&&U.recipientDraft.index);if(!slot)return;
 var value=document.getElementById('recipient-address').value.trim(),error=document.getElementById('recipient-error');
 var f=U.recipientField(slot,'Recipient');
 try{if(!f)throw Error('This recipient cannot be changed.');U.configValidate(f,value)}catch(e){error.hidden=false;error.textContent=e.message;return}
 var changes=[];
 function push(field,value){var spec=U.recipientField(slot,field);if(spec&&U.recipientValue(slot,field)!==value)changes.push({name:spec.name,value:value,spec:spec})}
 push('Recipient',value);
 Array.from(document.querySelectorAll('[data-event-field]')).forEach(function(el){push(el.dataset.eventField,el.checked?'1':'0')});
 var enabled=document.getElementById('recipient-enabled');if(enabled)push('Selected',enabled.checked?'1':'0');
 if(!changes.length){error.hidden=false;error.textContent='No changes to save.';return}
 /* Enable last so no unfinished recipient becomes active; disable first. */
 changes.sort(function(a,b){return /\.Selected$/.test(a.name)?(a.value==='0'?-1:1):/\.Selected$/.test(b.name)?(b.value==='0'?1:-1):0});
 U.recipientDraft.changes=changes;
 var dialog=document.getElementById('setting-dialog');
 dialog.innerHTML='<div class="editor-top">'+U.ico('shield')+'<div><span>REVIEW NOTIFICATION</span><h2 id="editor-title">Confirm recipient changes</h2></div></div><p class="editor-help">'+changes.length+' device settings will be saved individually and verified. If a write fails, the completed changes remain and will be reported.</p><div class="review-line"><span>Recipient</span><strong>'+U.esc(value)+'</strong></div><div class="review-line"><span>UPS destination</span><strong>'+(slot.index+1)+' of '+U.recipientSlots().length+'</strong></div><p id="recipient-error" class="editor-error" role="alert" hidden></p><div class="editor-footer"><button type="button" class="btn btn-quiet" id="recipient-back">Back</button><button type="button" id="recipient-apply" class="btn btn-primary">Save destination</button></div>';
 document.getElementById('recipient-apply').focus();
};
U.commitRecipient=function(){
 var draft=U.recipientDraft;if(!draft||U.config.saving)return;
 var changes=draft.changes;if(!changes||!changes.length)return;
 U.config.saving='recipient';var done=0;
 var apply=document.getElementById('recipient-apply');if(apply){apply.disabled=true;apply.textContent='Saving…'}
 function next(i){if(i===changes.length)return Promise.resolve();var change=changes[i];var xml='<?xml version="1.0"?><SET_OBJECT><OBJECT name="'+U.esc(change.name)+'">'+U.esc(change.value)+'</OBJECT></SET_OBJECT>';
  var form=new FormData();form.append('objectList',new File([xml],'Configuration.xml',{type:'application/xml'}));
  return U.request('/Forms/set_obj_2',{method:'POST',body:form},15000)
   .then(function(){return U.request('/get_object.xml',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'objectName='+encodeURIComponent(change.name)},10000)})
   .then(function(body){var actual=U.xml(body)[change.name];if(String(actual)!==change.value)throw Error('Device readback did not match '+change.name);U.config.values[change.name]=actual;done++;return next(i+1)});
 }
 next(0).then(function(){U.config.message='Recipient saved and verified ('+done+' settings).';U.config.error=''}).catch(function(e){U.config.error='Only '+done+' of '+changes.length+' changes verified. '+e.message;U.config.message=''}).finally(function(){
  U.config.saving='';U.recipientDraft=null;var dialog=document.getElementById('setting-dialog');if(dialog&&dialog.open)dialog.close();U.render(false);
 });
};
U.disableRecipient=function(index){var slot=U.findRecipient(index);if(!slot||!U.recipientActive(slot))return;
 if(!confirm('Disable alerts for '+U.recipientValue(slot,'Recipient')+'? You can enable this destination again later.'))return;
 U.recipientDraft={index:index,changes:[{name:U.recipientField(slot,'Selected').name,value:'0',spec:U.recipientField(slot,'Selected')}]};U.commitRecipient();
};
U.integrationGuide=function(service){var info={
 ntfy:'Configure the SMTP receiving address supplied by your ntfy server. You will need inbound email enabled on that server.',
 Monita:'Configure Monita inbound email on your server, then use its receiving address as an UPS alert recipient.',
 Discord:'Discord requires a separate SMTP-to-webhook gateway. Configure that gateway first, then enter its receiving email address here.'
 }[service];if(!info)return;
 var available=U.recipientSlots().find(function(s){return !U.recipientActive(s)});
 if(!available){U.config.error='All available recipient slots are active. Edit an existing destination to change it.';U.render(false);return}
 U.openRecipient(available.index,service);
};
var originalConsoleClick=U.onConsoleClick;
U.onConsoleClick=function(e){
 var button=e.target.closest('[data-recipient]');if(button){U.openRecipient(Number(button.dataset.recipient));return true}
 if(e.target.closest('#recipient-add')){var free=U.recipientSlots().find(function(s){return !U.recipientActive(s)});if(free)U.openRecipient(free.index);return true}
 var dis=e.target.closest('[data-disable-recipient]');if(dis){U.disableRecipient(Number(dis.dataset.disableRecipient));return true}
 var service=e.target.closest('[data-integration]');if(service){U.integrationGuide(service.dataset.integration);return true}
 if(e.target.closest('#recipient-cancel')){document.getElementById('setting-dialog').close();U.recipientDraft=null;return true}
 if(e.target.closest('#recipient-back')){var d=U.recipientDraft;U.openRecipient(d.index,d.source);return true}
 if(e.target.closest('#recipient-apply')){U.commitRecipient();return true}
 return originalConsoleClick(e);
};
var originalConsoleSubmit=U.onConsoleSubmit;
U.onConsoleSubmit=function(e){if(e.target.id==='recipient-form'){e.preventDefault();U.reviewRecipient();return}originalConsoleSubmit(e)};
