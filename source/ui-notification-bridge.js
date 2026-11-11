/* Destination manager: remote webhook credentials are stored only by the paired relay. */
U.bridge={url:'',token:'',ready:false,busy:false,error:'',message:'',destinations:[]};
U.bridgeRequest=function(path,method,body){
 var b=U.bridge;if(!b.url||!b.token)return Promise.reject(Error('Connect your notification relay first.'));
 var controller=new AbortController(),timer=setTimeout(function(){controller.abort()},12000);
 var options={method:method||'GET',headers:{Authorization:'Bearer '+b.token},cache:'no-store',credentials:'omit',redirect:'error',signal:controller.signal};
 if(body){options.headers['Content-Type']='application/json';options.body=JSON.stringify(body)}
 return fetch(b.url+path,options).then(function(response){return response.json().catch(function(){return {error:'Invalid response from notification relay'}}).then(function(data){if(!response.ok)throw Error(data.error||'Notification relay returned HTTP '+response.status);return data})}).finally(function(){clearTimeout(timer)});
};
U.bridgeRefresh=function(){var b=U.bridge;if(b.busy)return Promise.resolve();b.busy=true;b.error='';U.render(false);
 return U.bridgeRequest('/api/v1/destinations','GET').then(function(result){b.destinations=Array.isArray(result.destinations)?result.destinations:[];b.ready=true;b.error='';}).catch(function(e){b.error=e.message;b.ready=false;}).finally(function(){b.busy=false;U.render(false)});
};
U.bridgeConnect=function(){var url=document.getElementById('bridge-url').value.trim(),token=document.getElementById('bridge-token').value.trim();
 try{var parsed=new URL(url);if(parsed.protocol!=='https:'||parsed.username||parsed.password||parsed.search||parsed.hash)throw Error('Enter a credential-free HTTPS URL.');}catch(e){U.bridge.error=e.message;U.render(false);return}
 U.bridge.url=url.replace(/\/$/,'');U.bridge.token=token;U.bridgeRefresh();
};
U.openDestinationPicker=function(){var dialog=document.getElementById('setting-dialog');if(!dialog)return;
 dialog.innerHTML='<div class="editor-top">'+U.ico('mail')+'<div><h2 id="editor-title">Add destination</h2></div></div><form id="destination-picker"><label for="destination-type">Destination type</label><select id="destination-type"><option value="email">Email · one of four NMC slots</option><option value="monita">Monita · URL and API token</option><option value="discord">Discord · webhook URL</option><option value="ntfy">ntfy · server, topic and token</option></select><p class="editor-help">Email is native. Webhooks use the relay.</p><div class="editor-footer"><button type="button" id="destination-cancel" class="btn btn-quiet">Cancel</button><button type="submit" class="btn btn-primary">Continue</button></div></form>';
 dialog.showModal();document.getElementById('destination-type').focus();
};
U.pickDestination=function(){var kind=document.getElementById('destination-type').value;var dialog=document.getElementById('setting-dialog');dialog.close();
 if(kind==='email'){var free=U.recipientSlots().find(function(s){return !U.recipientConfigured(s)});if(!free){U.config.error='All four native email slots are active. Edit an existing email destination.';U.render(false);return}U.openRecipient(free.index);return}
 if(!U.bridge.ready){U.bridge.error='Connect the relay before adding '+kind+' destinations.';U.render(false);return}
 U.editBridgeDestination(null,kind);
};
U.editBridgeDestination=function(existing,kind){var dialog=document.getElementById('setting-dialog');if(!dialog)return;
 kind=kind||(existing&&existing.type);U.bridgeDraft=existing||{type:kind,enabled:true};
 var d=U.bridgeDraft,name=d.name||'',url=d.url||'',tokenHelp=existing?'Blank keeps current token.':'Enter a token.',events=['power_lost','power_restored','battery_low','battery_recovered','connection_lost','connection_restored'];
 var specific=kind==='monita'?'<label for="bridge-target-url">Monita server URL</label><input id="bridge-target-url" type="url" required value="'+U.esc(url)+'" placeholder="https://monita.example.net"><label for="bridge-target-token">Application API token</label><input id="bridge-target-token" type="password" '+(existing?'':'required')+' autocomplete="new-password"><small>'+tokenHelp+'</small>':kind==='discord'?'<label for="bridge-target-url">Discord webhook URL</label><input id="bridge-target-url" type="url" '+(existing?'':'required')+' placeholder="https://discord.com/api/webhooks/..."><small>Webhook URLs are kept private.</small>':'<label for="bridge-target-url">ntfy server URL</label><input id="bridge-target-url" type="url" required value="'+U.esc(url)+'" placeholder="https://ntfy.example.net"><label for="bridge-target-topic">Topic</label><input id="bridge-target-topic" required value="'+U.esc(d.topic||'')+'"><label for="bridge-target-token">Bearer token (optional)</label><input id="bridge-target-token" type="password" autocomplete="new-password"><small>'+tokenHelp+'</small>';
 dialog.innerHTML='<div class="editor-top">'+U.ico('mail')+'<div><h2 id="editor-title">'+U.esc(existing?'Edit ':'Add ')+U.esc(kind==='monita'?'Monita':kind==='discord'?'Discord':'ntfy')+'</h2></div></div><form id="bridge-destination-form"><label for="bridge-target-name">Destination name</label><input id="bridge-target-name" required maxlength="80" value="'+U.esc(name)+'">'+specific+'<fieldset class="event-choice"><legend>Automatic alerts</legend>'+events.map(function(t){return '<label class="event-option"><input type="checkbox" data-bridge-event="'+t+'" '+((!d.events||d.events.indexOf(t)>=0)?'checked':'')+'> '+U.esc(t.replace(/_/g,' '))+'</label>'}).join('')+'</fieldset><label class="recipient-enable"><input id="bridge-target-enabled" type="checkbox" '+(d.enabled?'checked':'')+'> Enable automatic alerts</label><p class="editor-error" id="bridge-editor-error" hidden role="alert"></p><div class="editor-footer"><button type="button" class="btn btn-quiet" id="destination-cancel">Cancel</button><button class="btn btn-primary" type="submit">Save destination</button></div></form>';
 dialog.showModal();document.getElementById('bridge-target-name').focus();
};
U.saveBridgeDestination=function(){var old=U.bridgeDraft,b=U.bridge;if(!old||b.busy)return;
 var payload={type:old.type,name:document.getElementById('bridge-target-name').value.trim(),enabled:document.getElementById('bridge-target-enabled').checked,events:Array.from(document.querySelectorAll('[data-bridge-event]:checked')).map(function(x){return x.dataset.bridgeEvent})};
 var url=document.getElementById('bridge-target-url').value.trim(),token=document.getElementById('bridge-target-token');
 if(url)payload.url=url;else if(old.type!=='discord')payload.url=old.url;
 if(token&&token.value)payload.token=token.value;
 if(old.type==='ntfy')payload.topic=document.getElementById('bridge-target-topic').value.trim();
 if(!payload.name){var e=document.getElementById('bridge-editor-error');e.textContent='Provide a destination name.';e.hidden=false;return}
 b.busy=true;b.error='';var dialog=document.getElementById('setting-dialog');var action=dialog.querySelector('[type=submit]');if(action)action.disabled=true;
 U.bridgeRequest('/api/v1/destinations'+(old.id?'/'+encodeURIComponent(old.id):''),old.id?'PUT':'POST',payload).then(function(){dialog.close();U.bridgeDraft=null;b.message='Saved destination.';return U.bridgeRequest('/api/v1/destinations','GET')}).then(function(data){b.destinations=data.destinations||[]}).catch(function(err){var e=document.getElementById('bridge-editor-error');if(e){e.hidden=false;e.textContent=err.message}else b.error=err.message}).finally(function(){b.busy=false;U.render(false)});
};
U.testBridgeDestination=function(id){var b=U.bridge;if(b.busy)return;b.busy=true;b.message='';b.error='';U.render(false);
 U.bridgeRequest('/api/v1/destinations/'+encodeURIComponent(id)+'/test','POST',{}).then(function(){b.message='Test delivered.'}).catch(function(err){b.error='Test failed: '+err.message}).finally(function(){b.busy=false;U.render(false)});
};
U.deleteBridgeDestination=function(id){var b=U.bridge;if(b.busy||!confirm('Delete this destination permanently?'))return;
 b.busy=true;b.error='';U.bridgeRequest('/api/v1/destinations/'+encodeURIComponent(id),'DELETE').then(function(){b.destinations=b.destinations.filter(function(d){return d.id!==id});b.message='Destination deleted.'}).catch(function(e){b.error=e.message}).finally(function(){b.busy=false;U.render(false)});
};
var previousBridgeClick=U.onConsoleClick;
U.onConsoleClick=function(e){
 if(e.target.closest('#destination-cancel')){document.getElementById('setting-dialog').close();return true}
 if(e.target.closest('#bridge-refresh')){U.bridgeRefresh();return true}
 var edit=e.target.closest('[data-bridge-edit]');if(edit){var d=U.bridge.destinations.find(function(x){return x.id===edit.dataset.bridgeEdit});if(d)U.editBridgeDestination(d);return true}
 var test=e.target.closest('[data-bridge-test]');if(test){U.testBridgeDestination(test.dataset.bridgeTest);return true}
 var del=e.target.closest('[data-bridge-delete]');if(del){U.deleteBridgeDestination(del.dataset.bridgeDelete);return true}
 return previousBridgeClick(e)
};
var previousBridgeSubmit=U.onConsoleSubmit;
U.onConsoleSubmit=function(e){
 if(e.target.id==='bridge-connect-form'){e.preventDefault();U.bridgeConnect();return}
 if(e.target.id==='destination-picker'){e.preventDefault();U.pickDestination();return}
 if(e.target.id==='bridge-destination-form'){e.preventDefault();U.saveBridgeDestination();return}
 previousBridgeSubmit(e);
};
