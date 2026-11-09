/* H910P sign-in: validates credentials against Dell's protected HTTP resource.
 * Client-side screen is UX only; the NMC's native HTTP authorization remains
 * the security boundary. Credentials stay in RAM and are never persisted.
 */
U.auth={header:'',name:'',pending:false};
U.authLogo=U.brandLogo||'';
U.signInScreen=function(error){
 var app=document.getElementById('app');
 app.innerHTML='<main class="signin-page"><section class="signin-panel" aria-labelledby="signin-title"><div class="signin-brand"><img class="signin-logo" alt="Dell" src="'+U.authLogo+'"><span>UPS management</span></div><div class="signin-status">'+U.ico('shield')+' Management card sign-in</div><h1 id="signin-title">Sign in to your UPS</h1><p>Use the username and password configured on your network management card.</p><form id="signin-form"><label for="signin-user">Username</label><input id="signin-user" name="username" autocomplete="username" maxlength="80" required value="'+U.esc(U.auth.name)+'"><label for="signin-pass">Password</label><input id="signin-pass" name="password" type="password" autocomplete="current-password" required><div id="signin-error" class="signin-error" role="alert" '+(error?'':'hidden')+'>'+U.esc(error||'')+'</div><button type="submit" class="signin-submit" id="signin-button" '+(U.auth.pending?'disabled':'')+'>'+U.ico('lock')+' Sign in</button></form><div class="signin-foot">Connected to '+U.esc(location.hostname||'your UPS')+'<span> · </span> Management stays on this device</div></section></main>';
 document.getElementById('signin-form').addEventListener('submit',U.authSubmit);
 (error?document.getElementById('signin-pass'):document.getElementById('signin-user')).focus();
};
U.authSubmit=function(e){
 e.preventDefault();if(U.auth.pending)return;
 var name=document.getElementById('signin-user').value.trim(),pass=document.getElementById('signin-pass').value;
 var msg=document.getElementById('signin-error'),button=document.getElementById('signin-button');
 if(!name||!pass||name.indexOf(':')>=0){msg.textContent='Enter a valid username and password.';msg.hidden=false;return}
 U.auth.pending=true;button.disabled=true;button.textContent='Signing in…';msg.hidden=true;
 /* HTTP Basic is native to Dell's OEM card; use its existing permission check. */
 var header='Basic '+btoa(unescape(encodeURIComponent(name+':'+pass)));
 var controller=new AbortController(),timer=setTimeout(function(){controller.abort()},10000);
 fetch('/app_upload.htm',{method:'GET',credentials:'omit',cache:'no-store',headers:{Authorization:header},signal:controller.signal}).then(function(res){
  if(res.status===401||res.status===403)throw Error('The username or password was not accepted.');
  if(!res.ok)throw Error('The card returned HTTP '+res.status+'. Please try again.');
  U.auth.header=header;U.auth.name=name;U.auth.pending=false;
  U.startApp();
 }).catch(function(err){U.auth.header='';U.auth.pending=false;U.signInScreen(err.name==='AbortError'?'The card did not respond. Check your connection.':err.message)}).finally(function(){clearTimeout(timer)});
};
U.lockInterface=function(){
 if(U.config.saving||U.power.busy)return;
 U.auth.header='';U.auth.name='';U.data={};U.events=[];U.history=[];U.updated=0;U.config.values={};U.config.loaded=false;U.identity={};
 clearInterval(U.timer);U.timer=0;U.signInScreen('');
};
U.startApp=function(){
 U.render(true);if(U.loadPageData)U.loadPageData();U.poll();U.loadEvents();
 clearInterval(U.timer);
 U.timer=setInterval(function(){if(document.visibilityState==='visible'&&U.auth.header)U.poll()},15000);
};
U.login=function(){U.signInScreen('')};
