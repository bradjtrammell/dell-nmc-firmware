/* Stable, keyboard-operable shell. Refreshes do not destroy navigation or scroll. */
U.render=function(navigate){
 var app=document.getElementById('app');if(!app)return;
 if(!document.getElementById('sidebar')){
  app.innerHTML=U.chrome();app.addEventListener('click',U.click);
  window.addEventListener('keydown',function(e){
   if(e.key==='Escape'){var rail=document.getElementById('sidebar'),toggle=document.getElementById('menu-toggle');
    if(rail&&toggle){rail.classList.remove('open');toggle.setAttribute('aria-expanded','false')}}
  });
 }
 var s=U.stateText(),state=document.getElementById('live-state'),sync=document.getElementById('sync-time');
 if(state)state.innerHTML='<i class="live-led '+s[1]+'"></i>'+U.esc(s[0]);
 if(sync)sync.textContent='Updated '+U.time();
 U.updateActivity();
 var v=document.getElementById('view');
 var views={overview:U.overview,electrical:U.electrical,outlets:U.outlets,events:U.eventsView,system:U.systemView,controls:U.controlsView};
 if(v){
  var isDynamic=['overview','electrical','outlets','events','system'].indexOf(U.page)!==-1||!!(U.config&&U.config.groups.length);
  var shouldUpdate=navigate||isDynamic;
  if(shouldUpdate){
   var html=(views[U.page]||U.management)();
   /* No destruction of focus, selection or scroll when the view is unchanged. */
   if(navigate||v.__renderedHtml!==html){
    var focus=document.activeElement,focusId=focus&&focus.id;
    var selection=focus&&focus.tagName==='INPUT'?{start:focus.selectionStart,end:focus.selectionEnd}:null;
    var x=window.scrollX,y=window.scrollY;
    v.innerHTML=html;v.__renderedHtml=html;
    if(!navigate){
     if(focusId){var replacement=document.getElementById(focusId);if(replacement){replacement.focus({preventScroll:true});
      if(selection&&replacement.setSelectionRange)replacement.setSelectionRange(selection.start,selection.end)}}
     window.scrollTo(x,y);
    }
   }
  }
 }
 app.querySelectorAll('[data-page]').forEach(function(b){var active=b.dataset.page===U.page;b.setAttribute('aria-current',active?'page':'false');b.classList.toggle('is-active',active)});
 var crumb=app.querySelector('.breadcrumbs strong');if(crumb)crumb.textContent=U.pageInfo(U.page)[1];
 if(navigate){var title=v&&v.querySelector('h1');if(title)title.focus({preventScroll:true})}
};
U.click=function(e){
 var nav=e.target.closest('[data-page]');if(nav){U.navto(nav.dataset.page);var rail=document.getElementById('sidebar');rail.classList.remove('open');var toggle=document.getElementById('menu-toggle');if(toggle)toggle.setAttribute('aria-expanded','false');return}
 if(e.target.closest('#refresh')||e.target.closest('#retry')){U.poll();return}
 if(e.target.closest('#load-events')){U.loadEvents();return}
 if(e.target.closest('#load-identity')){U.loadIdentity();return}
 if(e.target.closest('#load-config')){U.loadConfig();return}
 var power=e.target.closest('[data-power]');if(power){var parts=power.dataset.power.split(':');U.sendPower(Number(parts[0]),parts[1]);return}
 var save=e.target.closest('[data-save]');if(save){U.saveConfig(Number(save.dataset.save));return}
 if(e.target.closest('#menu-toggle')){var rail=document.getElementById('sidebar'),open=rail.classList.toggle('open');document.getElementById('menu-toggle').setAttribute('aria-expanded',String(open))}
};
