/* GitHub release discovery and native firmware upload; no remote runtime assets.
 * Browser HTTP upload cannot select HTTP/1.0. Device confirmation is authoritative.
 */
U.updater={checking:false,release:null,error:'',busy:false,result:'',file:null,channel:'main'};
U.checkGitHub=function(){
 if(U.updater.checking)return;
 U.updater.checking=true;U.updater.error='';U.updater.release=null;U.render(false);var channel=U.updater.channel;
 var c=new AbortController(),timer=setTimeout(function(){c.abort()},12000);
 fetch('https://api.github.com/repos/bradjtrammell/dell-nmc-firmware/releases'+(channel==='dev'?'?per_page=30':'/latest'),{
  method:'GET',cache:'no-store',signal:c.signal,headers:{Accept:'application/vnd.github+json'}
 }).then(function(r){if(r.status===404)throw Error('No firmware release is published.');if(!r.ok)throw Error('GitHub returned HTTP '+r.status);return r.json()})
 .then(function(r){if(channel==='dev')r=Array.isArray(r)?r.find(function(x){return x.prerelease&&!x.draft&&x.target_commitish==='dev'}):null;if(!r)throw Error('No published dev beta firmware');var assets=(r.assets||[]).filter(function(a){return /^H910P[_-].*\.bin$/i.test(a.name||'')&&/^https:\/\/github\.com\//.test(a.browser_download_url||'')});
  U.updater.release={name:String(r.name||r.tag_name||'Release'),tag:String(r.tag_name||''),url:r.html_url,assets:assets};
  if(!assets.length)U.updater.error='No compatible H910P BIN in this release.';
 }).catch(function(e){U.updater.error=e.name==='AbortError'?'GitHub request timed out.':e.message})
 .finally(function(){clearTimeout(timer);U.updater.checking=false;if(U.page==='firmware')U.render(false)});
};
U.updaterView=function(){var r=U.updater.release,assets=r?r.assets:[];
 return '<section class="setting-section"><div class="setting-section-head"><h2>Firmware releases</h2><span>GitHub</span></div><div class="maintenance-text">'+
  '<label for="firmware-channel">Update channel</label><select id="firmware-channel" '+(U.updater.checking?'disabled':'')+'><option value="main" '+(U.updater.channel==='main'?'selected':'')+'>Release (main)</option><option value="dev" '+(U.updater.channel==='dev'?'selected':'')+'>Beta (dev)</option></select>'+
  '<button type="button" class="btn btn-primary" id="github-check" '+(U.updater.checking?'disabled':'')+'>'+U.ico('refresh')+(U.updater.checking?' Checking GitHub…':' Check GitHub for firmware')+'</button>'+
  (U.updater.error?'<p role="alert" class="notice error">'+U.esc(U.updater.error)+'</p>':'')+
  (r?'<p><strong>'+U.esc(r.name)+'</strong> · '+U.esc(r.tag)+'</p>'+assets.map(function(a){return '<p><a class="btn btn-quiet" target="_blank" rel="noopener noreferrer" href="'+U.esc(a.browser_download_url)+'">'+U.ico('upload')+' Download '+U.esc(a.name)+'</a></p>'}).join(''):'')+
 '</div></section><section class="setting-section"><div class="setting-section-head"><h2>Install firmware</h2><span>Dell upload service</span></div><div class="maintenance-text">'+
 '<p>Use a verified BIN. Installation is complete only after the card confirms the FLASH write. Some cards require an external HTTP/1.0 uploader.</p>'+
 '<form id="firmware-local-form"><label for="firmware-file">Firmware BIN</label><input required id="firmware-file" type="file" accept=".bin,application/octet-stream">'+
 '<button class="btn btn-primary" id="firmware-install" type="submit" '+(U.updater.busy?'disabled':'')+'>'+U.ico('upload')+(U.updater.busy?' Uploading…':' Send BIN to NMC')+'</button></form>'+
 (U.updater.result?'<p role="status">'+U.esc(U.updater.result)+'</p>':'')+
 '<p><a href="/app_upload.htm" target="_blank" rel="noopener noreferrer">Check FLASH status</a> · <a href="https://github.com/bradjtrammell/dell-nmc-firmware/releases" target="_blank" rel="noopener noreferrer">All releases</a></p></div></section>';
};
U.sendLocalFirmware=function(e){e.preventDefault();if(U.updater.busy)return;
 var field=document.getElementById('firmware-file'),file=field&&field.files&&field.files[0];
 if(!file||!/\.bin$/i.test(file.name)||file.size<100000||file.size>1517029){U.updater.result='Select a compatible H910P BIN within the factory size limit.';U.render(false);return}
 /* Verify the complete NET+OS boot header and CRC before offering a flash. */
 file.arrayBuffer().then(function(buffer){
  var bytes=new Uint8Array(buffer),dv=new DataView(buffer),magic='';
  for(var i=8;i<15&&i<bytes.length;i++)magic+=String.fromCharCode(bytes[i]);
  if(magic!=='bootHdr'||dv.getUint32(0)!==36||dv.getUint32(4)!==36||
     dv.getUint32(16)!==2||dv.getUint32(20)!==9||dv.getUint32(32)!==bytes.length-40)
    throw Error('Unsupported H910P firmware image.');
  var crc=0,poly=0x04C11DB7;
  for(var j=0;j<bytes.length;j++){
   crc=(crc^((bytes[j]&255)<<24))>>>0;
   for(var bit=0;bit<8;bit++)crc=((crc&0x80000000)?((crc<<1)^poly):(crc<<1))>>>0;
  }
  if(crc!==0)throw Error('Firmware CRC invalid; upload stopped.');
  if(!confirm('Verified firmware. Send '+file.name+' ('+file.size+' bytes) to the NMC? Management may restart.'))return;
  U.performFirmwareUpload(file);
 }).catch(function(e){U.updater.result=e.message;U.render(false)});
};
U.performFirmwareUpload=function(file){
 var body=new FormData();body.append('file',file,'Dell_nmc_01_18_0005.bin');
 U.updater.busy=true;U.updater.result='Sending firmware to the NMC…';U.render(false);
 var c=new AbortController(),timer=setTimeout(function(){c.abort()},180000),headers={};if(U.auth&&U.auth.header)headers.Authorization=U.auth.header;
 fetch('/Forms/app_upload_1',{method:'POST',credentials:'omit',headers:headers,body:body,redirect:'manual',signal:c.signal})
  .then(function(r){if(r.status===401||r.status===403)throw Error('Firmware upload rejected by the card.');
   if(r.type==='opaqueredirect'||r.status===302||r.status===303)U.updater.result='Upload redirected. Verify the NMC FLASH-write status before proceeding.';
   else if(!r.ok)throw Error('Upload returned HTTP '+r.status);
   else U.updater.result='Transfer complete. Verify the NMC FLASH-write status.';
  }).catch(function(err){U.updater.result='Upload unverified: '+err.message+'. Use HTTP/1.0 if browser upload fails.'})
  .finally(function(){clearTimeout(timer);U.updater.busy=false;U.render(false)});
};
var oldUpdateClick=U.onConsoleClick;
U.onConsoleClick=function(e){if(e.target.closest('#github-check')){U.checkGitHub();return true}return oldUpdateClick(e)};
var oldUpdateChange=U.onConsoleChange;U.onConsoleChange=function(e){if(e.target.id==='firmware-channel'){U.updater.channel=e.target.value==='dev'?'dev':'main';U.updater.release=null;U.updater.error='';U.render(false);return}if(oldUpdateChange)oldUpdateChange(e)};var oldUpdateSubmit=U.onConsoleSubmit;
U.onConsoleSubmit=function(e){if(e.target.id==='firmware-local-form'){U.sendLocalFirmware(e);return}oldUpdateSubmit(e)};

var priorChannelChange=U.onConsoleChange;U.onConsoleChange=function(e){if(e.target.id==='firmware-channel'){U.updater.channel=e.target.value==='dev'?'dev':'main';U.updater.release=null;U.updater.error='';U.render(false)}else if(priorChannelChange)priorChannelChange(e)};
