/* Operational overview driven only by live card measurements and recorded events. */
var baseOverview=U.overview;
U.overview=function(){
 var recent=U.events.slice(0,5),has=!!U.updated;
 var shortcuts='<div class="overview-shortcuts">'+[
  ['events','calendar','Event history','Review recorded alerts'],
  ['outlets','plug','Output groups','Inspect protected outlets'],
  ['notification','mail','Notifications','Manage alert recipients'],
  ['firmware','upload','Updates','Check firmware status']
 ].map(function(a){return '<button class="overview-shortcut" type="button" data-page="'+a[0]+'">'+U.ico(a[1])+'<span><strong>'+a[2]+'</strong><small>'+a[3]+'</small></span>'+U.ico('arrow')+'</button>'}).join('')+'</div>';
 var activity='<section class="recent-activity"><div class="section-title"><div><h2>Recent UPS activity</h2><p>Latest entries from the card’s event log</p></div><button type="button" data-page="events" class="btn btn-quiet">View all events '+U.ico('arrow')+'</button></div><div class="activity-lines">'+
 (recent.length?recent.map(function(line){var parts=line.match(/^([^,]+),(.+)$/),msg=parts?parts[2].trim():line,dt=parts?parts[1].trim():'';var bad=/critical|fault|fail|low battery/i.test(msg);return '<div class="activity-line"><span class="activity-dot '+(bad?'critical':'')+'"></span><strong>'+U.esc(msg)+'</strong><time>'+U.esc(dt||'Recorded event')+'</time></div>'}).join(''):
 '<div class="activity-empty">'+U.ico('info')+' '+U.esc(U.eventsBusy?'Loading recent events…':U.eventsError?'Event history could not be loaded.':'No recent events were reported.')+'</div>')+
 '</div></section>';
 return baseOverview().replace('<div class="hero-status">',shortcuts+'<div class="hero-status">')+activity;
};
/* Local maintenance and GitHub release discovery. Native asset-store OTA is not claimed. */
U.firmwareView=function(){return U.heading('MAINTENANCE','Software updates','Check releases and install a compatible device firmware image.')+U.warning()+
 '<div class="maintenance-band"><div>'+U.ico('shield')+'<span>Management interface</span><strong>v'+U.uiVersion+'</strong></div><div>'+U.ico('server')+'<span>Device firmware</span><strong>01.18.0005</strong></div></div>'+
 U.updaterView();
};
