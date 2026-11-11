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

U.notificationView=function(){
 var slots=U.recipientSlots(),bridge=U.bridge||{},remotes=bridge.destinations||[];
 var configured=slots.filter(function(s){return U.recipientConfigured(s)});
 var rows=configured.map(function(s){
  var address=U.recipientValue(s,'Recipient'),active=U.recipientActive(s);
  return '<tr><td><strong>'+U.esc(address)+'</strong><small>Native slot '+s.index+'</small></td><td>Email</td><td><span class="recipient-status '+(active?'enabled':'')+'">'+(active?'Enabled':'Disabled')+'</span></td><td><div class="destination-actions"><button class="btn btn-quiet" type="button" data-recipient="'+s.index+'">Edit</button>'+(active?'<button class="btn btn-quiet" type="button" data-disable-recipient="'+s.index+'">Disable</button>':'')+'</div></td></tr>';
 });
 remotes.forEach(function(d){rows.push('<tr><td><strong>'+U.esc(d.name)+'</strong><small>'+U.esc(d.type==='discord'?'Webhook configured':d.type==='ntfy'?(d.topic||'ntfy topic'):(d.url||'Monita server'))+'</small></td><td>'+U.esc(d.type==='monita'?'Monita':d.type==='discord'?'Discord':'ntfy')+'</td><td><span class="recipient-status '+(d.enabled?'enabled':'')+'">'+(d.enabled?'Enabled':'Disabled')+'</span></td><td><div class="destination-actions"><button class="btn btn-quiet" type="button" data-bridge-edit="'+U.esc(d.id)+'">Edit</button><button class="btn btn-quiet" type="button" data-bridge-test="'+U.esc(d.id)+'" '+(bridge.busy?'disabled':'')+'>Test</button><button class="btn btn-quiet" type="button" data-bridge-delete="'+U.esc(d.id)+'" '+(bridge.busy?'disabled':'')+'>Delete</button></div></td></tr>')});
 var problem=bridge.error||U.config.error,success=bridge.message||U.config.message;
 var notice=problem?'<div role="alert" class="notice error">'+U.ico('alert')+U.esc(problem)+'</div>':success?'<div role="status" class="notice info">'+U.ico('check')+U.esc(success)+'</div>':'';
 var emailCount=configured.length,free=slots.some(function(s){return !U.recipientConfigured(s)});
 return U.heading('NOTIFICATIONS','Notification manager','Manage UPS notification destinations.')+U.warning()+notice+
 '<div class="recipient-heading"><div><h2>Destinations</h2><p>'+emailCount+' of 4 email slots configured · '+remotes.length+' external destinations</p></div><div class="destination-toolbar"><button type="button" class="btn btn-quiet" id="smtp-settings" '+(!U.config.valuesLive||U.config.saving?'disabled':'')+'>'+U.ico('sliders')+' SMTP settings</button><button type="button" id="recipient-add" class="btn btn-primary" '+(!free&&!bridge.ready?'disabled':'')+'>'+U.ico('check')+' Add Destination</button></div></div>'+
 (U.config.warning?'<p class="editor-help">'+U.esc(U.config.warning)+'</p>':'')+
 '<section class="setting-section"><div class="destination-table-scroll"><table class="destination-table"><thead><tr><th scope="col">Destination</th><th scope="col">Type</th><th scope="col">Status</th><th scope="col">Actions</th></tr></thead><tbody>'+(rows.join('')||'<tr><td colspan="4" class="destination-empty">No destinations yet. Select Add Destination.</td></tr>')+'</tbody></table></div></section>'+
 (!bridge.ready?'<section class="setting-section"><div class="setting-section-head"><h2>Webhook delivery connection</h2><span>Monita · Discord · ntfy</span></div><p class="editor-help">Webhooks use the relay; native email uses the UPS.</p><form id="bridge-connect-form" class="destination-connect"><label for="bridge-url">Relay HTTPS URL</label><input id="bridge-url" type="url" placeholder="https://notifications.example.net" required value="'+U.esc(bridge.url||'')+'"><label for="bridge-token">Relay access token</label><input id="bridge-token" type="password" autocomplete="off" required><button class="btn btn-primary" type="submit" '+(bridge.busy?'disabled':'')+'>Connect</button></form></section>':'<button type="button" class="btn btn-quiet" id="bridge-refresh" '+(bridge.busy?'disabled':'')+'>Refresh destinations</button>');
};
U.openSMTP=function(){
 var dialog=document.getElementById('setting-dialog'),fields=U.mailFields();if(!dialog||U.config.saving)return;
 if(!U.config.valuesLive){U.config.error='SMTP settings require live device values.';U.render(false);return}
 if(!fields.length){U.config.error='The card did not expose SMTP settings.';U.render(false);return}
 U.smtpDraft=fields;
 dialog.innerHTML='<div class="editor-top">'+U.ico('mail')+'<div><h2 id="editor-title">Outgoing email server</h2></div></div><p class="editor-help">Applies to all four email slots.</p><form id="smtp-form">'+fields.map(function(f,i){var secret=U.fieldMasked(f),kind=f.type.split(':')[0],val=String(U.config.values[f.name]||'');var options=kind==='Boolean'?'<select id="smtp-field-'+i+'" data-smtp="'+i+'"><option value="1" '+(val==='1'?'selected':'')+'>Enabled</option><option value="0" '+(val==='0'?'selected':'')+'>Disabled</option></select>':'<input data-smtp="'+i+'" id="smtp-field-'+i+'" type="'+(secret?'password':'text')+'" '+(secret?'autocomplete="new-password" placeholder="Leave blank to keep existing secret"':'value="'+U.esc(val)+'" autocomplete="off"')+'>';return '<div class="smtp-field"><label for="smtp-field-'+i+'">'+U.esc(U.mailFieldLabel(f))+'</label>'+options+'</div>'}).join('')+'<p id="smtp-error" class="editor-error" role="alert" hidden></p><div class="editor-footer"><button type="button" class="btn btn-quiet" id="smtp-cancel">Cancel</button><button type="submit" class="btn btn-primary">Review changes</button></div></form>';
 dialog.showModal();dialog.querySelector('input,select').focus();
};
U.reviewSMTP=function(){
 var changes=[],fields=U.smtpDraft||[],error=document.getElementById('smtp-error');
 try{fields.forEach(function(f,i){var value=document.getElementById('smtp-field-'+i).value,secret=U.fieldMasked(f);if(secret&&!value)return;if(!secret&&value===String(U.config.values[f.name]||''))return;U.configValidate(f,value);changes.push({field:f,value:value})})}catch(e){error.textContent=e.message;error.hidden=false;return}
 if(!changes.length){error.textContent='No changes to save.';error.hidden=false;return}
 U.smtpChanges=changes;var dialog=document.getElementById('setting-dialog');
 dialog.innerHTML='<div class="editor-top">'+U.ico('shield')+'<div><h2 id="editor-title">Confirm SMTP settings</h2></div></div><p class="editor-help">Changes will be submitted to the NMC individually and read back when possible. Credentials cannot be verified by readback.</p>'+changes.map(function(x){return '<div class="review-line"><span>'+U.esc(U.mailFieldLabel(x.field))+'</span><strong>'+U.esc(U.fieldMasked(x.field)?'Hidden credential':x.value)+'</strong></div>'}).join('')+'<p id="smtp-error" class="editor-error" role="alert" hidden></p><div class="editor-footer"><button type="button" class="btn btn-quiet" id="smtp-cancel">Cancel</button><button type="button" class="btn btn-primary" id="smtp-apply">Apply to NMC</button></div>';
};
U.commitSMTP=function(){
 if(U.config.saving||!U.smtpChanges||!U.smtpChanges.length)return;
 var changes=U.smtpChanges,done=0,masked=0,button=document.getElementById('smtp-apply');if(button)button.disabled=true;
 U.config.saving='smtp';U.config.message='';U.config.error='';
 function next(i){if(i===changes.length)return Promise.resolve();var x=changes[i],xml='<?xml version="1.0"?><SET_OBJECT><OBJECT name="'+U.esc(x.field.name)+'">'+U.esc(x.value)+'</OBJECT></SET_OBJECT>',form=new FormData();form.append('objectList',new File([xml],'Configuration.xml',{type:'application/xml'}));
 return U.request('/Forms/set_obj_2',{method:'POST',body:form},15000).then(function(){return U.request('/get_object.xml',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'objectName='+encodeURIComponent(x.field.name)},10000)}).then(function(body){var actual=U.xml(body)[x.field.name];if(U.fieldMasked(x.field)){masked++}else{if(String(actual)!==x.value)throw Error('Readback mismatch for '+U.mailFieldLabel(x.field));U.config.values[x.field.name]=actual}done++;return next(i+1)})}
 next(0).then(function(){U.config.message='SMTP settings saved: '+done+' submitted'+(masked?' ('+masked+' credentials require a delivery test).':'.')}).catch(function(e){U.config.error='SMTP update incomplete: '+done+' of '+changes.length+' submitted. '+e.message}).finally(function(){U.config.saving='';U.smtpDraft=null;U.smtpChanges=null;var dialog=document.getElementById('setting-dialog');if(dialog&&dialog.open)dialog.close();U.render(false)})
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
var originalConsoleClick=U.onConsoleClick;
U.onConsoleClick=function(e){
 var button=e.target.closest('[data-recipient]');if(button){U.openRecipient(Number(button.dataset.recipient));return true}
 if(e.target.closest('#recipient-add')){U.openDestinationPicker();return true}
 if(e.target.closest('#smtp-settings')){U.openSMTP();return true}
 if(e.target.closest('#smtp-cancel')){document.getElementById('setting-dialog').close();U.smtpDraft=null;U.smtpChanges=null;return true}
 if(e.target.closest('#smtp-apply')){U.commitSMTP();return true}
 var dis=e.target.closest('[data-disable-recipient]');if(dis){U.disableRecipient(Number(dis.dataset.disableRecipient));return true}
 if(e.target.closest('#recipient-cancel')){document.getElementById('setting-dialog').close();U.recipientDraft=null;return true}
 if(e.target.closest('#recipient-back')){var d=U.recipientDraft;U.openRecipient(d.index,d.source);return true}
 if(e.target.closest('#recipient-apply')){U.commitRecipient();return true}
 return originalConsoleClick(e);
};
var originalConsoleSubmit=U.onConsoleSubmit;
U.onConsoleSubmit=function(e){if(e.target.id==='recipient-form'){e.preventDefault();U.reviewRecipient();return}if(e.target.id==='smtp-form'){e.preventDefault();U.reviewSMTP();return}originalConsoleSubmit(e)};
