/* Native management routes remain backed by Dell's original handlers. */
U.systemView=function(){
 var identity=Object.keys(U.identity||{}).map(function(name){return U.row(name,U.identity[name])}).join('');
 var fields=(U.configFor?U.configFor('system'):[]).flatMap(function(g){return g.fields});
 U.configFieldMap={};var settings=fields.map(function(f,i){U.configFieldMap[i]=f;return '<div class="setting-row"><div class="setting-copy"><strong>'+U.esc(U.pretty(f.name))+'</strong><p>'+U.esc(U.fieldHelp(f))+'</p></div><div class="setting-actions"><span class="setting-current">'+U.esc(U.fieldValue(f))+'</span><button class="btn btn-quiet" data-edit="'+i+'">Edit</button></div></div>'}).join('');
 var data=[['NMC address',location.host||'unknown'],['Integration','Same-origin Dell XML'],['Last UPS measurement',U.time()],['Measurement objects returned',String(Object.keys(U.data).length)]];
 return U.heading('CARD STATUS','Management card','Live card identification and protocol diagnostics.')+U.warning()+
 '<section class="panel wide"><div class="section-title"><div><h2>Card identification</h2><p>Read from /product.xml on this NMC; no configuration writes</p></div>'+U.btn(U.identityBusy?'Loading...':'Read device identity','load-identity','refresh')+'</div>'+
 (U.identityError?'<div role="alert" class="notice error">'+U.ico('alert')+U.esc(U.identityError)+'</div>':'')+
 (identity?'<div class="identity-grid">'+identity+'</div>':'<div class="empty-state">'+U.ico('server')+'<strong>Identity not yet retrieved</strong><p>Select Read device identity to inspect the card’s actual reported properties.</p></div>')+
 '</section><section class="panel wide"><div class="section-title"><h2>Device profile settings</h2></div>'+(U.config.warning?'<div class="notice info">'+U.esc(U.config.warning)+'</div>':'')+(settings||'<div class="empty-state">Device configuration values are still loading.</div>')+'</section><section class="panel wide"><div class="section-title"><h2>Connection diagnostics</h2></div><div class="identity-grid">'+data.map(function(x){return U.row(x[0],x[1])}).join('')+'</div></section>'+
 '<div class="section-actions">'+U.nativeLink('/product.xml','Open original product XML')+U.nativeLink('/app_upload.htm','Firmware recovery page')+'</div>';
};
