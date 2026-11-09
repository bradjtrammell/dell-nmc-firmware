/* Dell H910P - native UI state, accessibility and navigation. */
window.U=window.U||{};
U.pages=[
 ['overview','Overview','dashboard','activity','Power monitoring'],
 ['electrical','Measurements','dashboard','chart','Power monitoring'],
 ['outlets','Output groups','dashboard','plug','Power monitoring'],
 ['events','Event history','dashboard','calendar','Power monitoring'],
 ['controls','UPS control','control','power','UPS operations','/ups_cntr.htm'],
 ['schedule','Weekly schedule','control','clock','UPS operations','/set_sche.htm'],
 ['shutdown','Shutdown parameters','control','sliders','UPS operations','/ups_conf.htm'],
 ['notification','Notification center','configuration','mail','Configuration','/ups_noti.htm'],
 ['network','Network','configuration','network','Configuration','/set_net.htm'],
 ['system','System','configuration','server','Configuration','/set_sys.htm'],
 ['clients','Shutdown clients','configuration','users','Configuration','/ups_cli.htm'],
 ['access','Access control','configuration','lock','Configuration','/ups_cont.htm'],
 ['snmp','SNMP','configuration','signal','Configuration','/set_snmp.htm'],
 ['time','Time & NTP','configuration','clock','Configuration','/set_time.htm'],
 ['firmware','Firmware','configuration','upload','Configuration','/app_upload.htm']
];
U.icons={activity:'M4 13h4l3-8 4 14 3-6h3',chart:'M4 18V6m0 12h16M7 14l4-4 3 2 4-6',plug:'M9 3v6m6-6v6M8 9h8v3a4 4 0 0 1-8 0V9zm4 7v8m-4 0h8',calendar:'M4 6h16v14H4zM8 3v6m8-6v6M4 11h16',power:'M12 3v9m-5-6a8 8 0 1 0 10 0',clock:'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18zm0 5v5l4 2',sliders:'M4 6h16M4 12h16M4 18h16M8 3v6m8 0v6m-8 0v6',mail:'M3 5h18v14H3zM3 6l9 7 9-7',network:'M12 3v5M4 13h16M6 13v7m12-7v7M8 8h8M12 8v5',server:'M4 4h16v7H4zM4 13h16v7H4zM8 8h.01M8 17h.01',users:'M8 12a4 4 0 1 0 0-8 4 4 0 0 0 0 8zm-6 9v-2a6 6 0 0 1 12 0v2m3-15a4 4 0 0 1 0 8m2 7v-2a6 6 0 0 0-3-5',lock:'M5 11h14v10H5zM8 11V7a4 4 0 0 1 8 0v4',signal:'M3 19v-3m5 3v-7m5 7V8m5 11V4',upload:'M12 17V4m-5 5 5-5 5 5M4 17v4h16v-4',shield:'m12 2 8 4v6c0 5-4 8-8 10-4-2-8-5-8-10V6z',refresh:'M20 7v5h-5M4 17v-5h5m10-4A8 8 0 0 0 5 9m0 6a8 8 0 0 0 14-1',arrow:'M5 12h14m-5-5 5 5-5 5',external:'M13 5h6v6m0-6-8 8M19 14v5H5V5h5',battery:'M3 7h16v10H3zM19 10h2v4',zap:'M13 2 5 13h7l-1 9 8-12h-7z',check:'M4 12l5 5L20 6',alert:'M12 3 2 21h20zM12 9v5m0 4h.01',menu:'M3 6h18M3 12h18M3 18h18',info:'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18zm0 8v6m0-9h.01',therm:'M10 13V5a2 2 0 0 1 4 0v8a5 5 0 1 1-4 0z'};
U.ico=function(name,cls){return '<svg class="ic '+(cls||'')+'" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.85" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="'+(U.icons[name]||U.icons.info)+'"/></svg>'};
U.esc=function(s){return String(s==null?'':s).replace(/[&<>"']/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]})};
U.money=function(n,suffix){return n===null?'--':(Math.round(n*10)/10).toLocaleString()+(suffix||'')};
U.mark=function(label,good){return '<span class="mark '+(good?'ok':'caution')+'"><i></i>'+U.esc(label)+'</span>'};
U.pageInfo=function(key){return U.pages.find(function(x){return x[0]===key})||U.pages[0]};
U.nativeLink=function(url,label){return '<a class="btn btn-primary" href="'+U.esc(url)+'">'+U.ico('external')+U.esc(label||'Open original management screen')+'</a>'};
U.btn=function(txt,id,icon){return '<button class="btn btn-quiet" id="'+id+'">'+U.ico(icon)+U.esc(txt)+'</button>'};
U.row=function(a,b){return '<div class="value-row"><span>'+U.esc(a)+'</span><strong>'+U.esc(b)+'</strong></div>'};
U.heading=function(k,t,d){return '<div class="page-head"><div><div class="page-meta">'+U.esc(k)+'</div><h1 tabindex="-1">'+U.esc(t)+'</h1><p>'+U.esc(d)+'</p></div></div>'};
U.time=function(){return U.updated?new Date(U.updated).toLocaleTimeString():'Awaiting NMC'};
U.stateText=function(){if(U.error)return ['Connection interrupted','bad'];if(!U.updated)return ['Connecting to NMC','pending'];if(U.present('discharging'))return ['Running on battery','warning'];if(U.present('source'))return ['Utility power healthy','good'];return ['Power state unverified','pending']};
U.navto=function(page){if(!U.pages.some(function(x){return x[0]===page}))page='overview';U.page=page;history.replaceState(null,'','#'+page);U.render(true)};
U.chrome=function(){var s=U.stateText(),groups=['Power monitoring','UPS operations','Configuration'];return '<a class="skip" href="#view">Skip to main content</a><div class="app-shell"><aside class="rail" id="sidebar"><div class="brand"><span class="brand-mark">DELL<span class="brand-slash">/</span></span><span class="brand-text">Power Management<br><small>H910P NETWORK CARD</small></span></div><div class="device-label"><span class="device-dot"></span><div><strong>UPS Rack 1920W / 1500W</strong><small>Network interface - '+U.esc(location.hostname||'local card')+'</small></div></div><nav aria-label="Management" class="side-nav">'+groups.map(function(g){return '<div class="nav-group"><div class="nav-section">'+g+'</div>'+U.pages.filter(function(x){return x[4]===g}).map(function(x){return '<button type="button" data-page="'+x[0]+'" '+(U.page===x[0]?'aria-current="page"':'')+'>'+U.ico(x[3])+'<span>'+x[1]+'</span>'+(x[5]?'<span class="nav-deep" aria-label="Original control screen"></span>':'')+'</button>'}).join('')+'</div>'}).join('')+'</nav><div class="rail-foot"><div class="rail-identity">'+U.ico('shield')+'<div><strong>Direct NMC connection</strong><small>No cloud or separate service</small></div></div><span>Firmware 01.18.0005</span></div></aside><div class="workspace"><header class="topbar"><button id="menu-toggle" class="icon-btn mobile" type="button" aria-label="Toggle navigation" aria-expanded="false">'+U.ico('menu')+'</button><div class="breadcrumbs min-w-0">DELL UPS <span>/</span> <strong>'+U.esc(U.pageInfo(U.page)[1])+'</strong></div><div class="top-actions flex items-center"><span class="live-badge" id="live-state"><i class="live-led '+s[1]+'"></i>'+U.esc(s[0])+'</span><span class="sync-time" id="sync-time">Updated '+U.time()+'</span><button type="button" class="icon-btn" id="refresh" aria-label="Refresh now" title="Refresh now">'+U.ico('refresh')+'</button></div></header><main class="main" id="view" tabindex="-1"></main><footer class="app-footer"><span>DELL H910P <b> / </b> LOCAL MANAGEMENT</span><span>01.18.0005 <b> / </b> <a href="/app_upload.htm">Firmware status</a></span></footer></div></div>'};
U.warning=function(){return U.error?'<div class="notice error" role="alert">'+U.ico('alert')+'<div><strong>Unable to update measurements</strong><span>'+U.esc(U.error)+'</span></div><button class="btn btn-quiet" id="retry" type="button">Retry</button></div>':''};
