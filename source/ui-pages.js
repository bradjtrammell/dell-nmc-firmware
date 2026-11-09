/* Native management routes remain backed by Dell's original handlers. */
U.systemView=function(){
 var identity=Object.keys(U.identity||{}).map(function(name){return U.row(name,U.identity[name])}).join('');
 var data=[['NMC address',location.host||'unknown'],['Integration','Same-origin Dell XML'],['Last UPS measurement',U.time()],['Measurement objects returned',String(Object.keys(U.data).length)]];
 return U.heading('CARD STATUS','Management card','Live card identification and protocol diagnostics.')+U.warning()+
 '<section class="panel wide"><div class="section-title"><div><h2>Card identification</h2><p>Read from /product.xml on this NMC; no configuration writes</p></div>'+U.btn(U.identityBusy?'Loading...':'Read device identity','load-identity','refresh')+'</div>'+
 (U.identityError?'<div role="alert" class="notice error">'+U.ico('alert')+U.esc(U.identityError)+'</div>':'')+
 (identity?'<div class="identity-grid">'+identity+'</div>':'<div class="empty-state">'+U.ico('server')+'<strong>Identity not yet retrieved</strong><p>Select Read device identity to inspect the card’s actual reported properties.</p></div>')+
 '</section><section class="panel wide"><div class="section-title"><h2>Connection diagnostics</h2></div><div class="identity-grid">'+data.map(function(x){return U.row(x[0],x[1])}).join('')+'</div></section>'+
 '<div class="section-actions">'+U.nativeLink('/product.xml','Open original product XML')+U.nativeLink('/default.htm','Original Dell management')+'</div>';
};
