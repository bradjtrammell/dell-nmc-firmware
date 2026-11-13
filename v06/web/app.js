"use strict";
/* Native-management UI for the isolated development host. Never manufacture
 * measurements, enable commands absent in /capabilities or turn 503 into 200.
 * All externally sourced strings enter the DOM through textContent. */
(function () {
  const groups = [
    ["UPS", [["properties","UPS Properties"],["control","UPS Control"],["schedule","Weekly Schedule"],["shutdown","Shutdown Parameters"]]],
    ["Logs and Notification", [["measurements","Measurements"],["events","Event Log"],["system-log","System Log"],["email","Email Notification"]]],
    ["Settings", [["network","Network"],["system","System"],["applications","Notified Applications"],["access","Access Control"],["snmp","SNMP"],["time","Time"],["firmware","Firmware Upload"]]]
  ];
  const pageMap = new Map();
  const nav = document.getElementById("navigation");
  const view = document.getElementById("view");
  const banner = document.getElementById("banner");
  const refreshButton = document.getElementById("refresh");
  const indicator = document.getElementById("health-indicator");
  const sidebar = document.getElementById("sidebar");
  const menuButton = document.getElementById("menu");
  const fieldsByPage = new Map();
  const state = {page:"properties", generation:0, busy:false, summary:null, fields:null, eventPage:0, search:""};
  const API = "/api/v1/";
  function element(tag, classes, value) {
    const n = document.createElement(tag);
    if (classes) n.className = classes;
    if (value !== undefined && value !== null) n.textContent = String(value);
    return n;
  }
  function add(parent, ...children) {
    children.forEach(child => { if(child) parent.appendChild(child); });
    return parent;
  }
  function node(tag, label, cls) { return element(tag, cls || "", label); }
  function button(label, action, disabled) {
    const b = element("button","button",label);
    b.type="button";
    b.disabled=!!disabled;
    if(action && !disabled)b.addEventListener("click",action);
    return b;
  }
  function title(name, desc) {
    const box=element("div","page-title");
    const left=element("div");
    add(left,node("h1",name),node("p",desc));
    add(box,left);
    return box;
  }
  function section(name, desc) {
    const b=element("div","section-heading");
    add(b,node("h2",name));
    if(desc)add(b,node("p",desc));
    return b;
  }
  function notice(head, desc, style) {
    const b=element("div","notice "+(style||""));
    add(b,node("strong",head),node("p",desc));
    return b;
  }
  function valueOrDash(x, suffix) {
    return Number.isFinite(Number(x)) && x!==null && x!==undefined ?
      String(x)+(suffix||"") : "Unavailable";
  }
  function table(headers,rows) {
    const wrap=element("div","table-wrap");
    const t=element("table");
    const thead=element("thead");
    const tr=element("tr");
    headers.forEach(s=>add(tr,node("th",s)));
    add(thead,tr);add(t,thead);
    const tb=element("tbody");
    if(!rows.length){
      const empty=element("tr");
      const cell=node("td","No records available");
      cell.colSpan=headers.length;
      add(empty,cell);add(tb,empty);
    } else {
      rows.forEach(row=>{
        const line=element("tr");
        row.forEach((text,index)=>{
          const cell=element("td",index===0?"numeric":"");
          if(text && text.nodeType)cell.appendChild(text);
          else cell.textContent=text===undefined||text===null?"—":String(text);
          line.appendChild(cell);
        });
        tb.appendChild(line);
      });
    }
    add(t,tb);add(wrap,t);
    return wrap;
  }
  function properties(items) {
    const grid=element("div","property-grid");
    items.forEach(pair=>{
      const r=element("div","property");
      add(r,node("span",pair[0]),node("strong",pair[1]));
      grid.appendChild(r);
    });
    return grid;
  }
  function metric(label,value,unit) {
    const item=element("div","metric");
    add(item,node("span",label,"label"),node("strong",value),node("small",unit||""));
    return item;
  }
  function statusTag(value, severity) {
    const tag=element("span","tag "+(severity||""),value);
    return tag;
  }
  function command(name,description,why) {
    const container=element("div","command");
    const details=element("div");
    add(details,node("strong",name),node("p",description));
    const action=button("Not available",null,true);
    action.setAttribute("title",why||"No supported authenticated backend action exists");
    add(container,details,action);
    return container;
  }
  function commands(list) {
    const box=element("div","command-list");
    list.forEach(entry=>box.appendChild(command(entry[0],entry[1])));
    return box;
  }
  function setBanner(mode, heading, details) {
    banner.className="banner "+(mode||"");
    banner.replaceChildren(node("strong",heading),node("span",details));
  }
  function sourceInfo(s) {
    const data=s.health||s.device||s.capabilities;
    const sim=!!(data&&data.simulated);
    const available=!!s.health;
    indicator.className="health-indicator "+(available?"ok":"bad");
    indicator.textContent=available?(sim?"Simulator API":"Management API"):"API unavailable";
    document.getElementById("device-status").textContent=available?
      (sim?"Isolated development simulator":"Native management endpoint"):"Management service unavailable";
    if(!available){
      setBanner("error","Management API unavailable","Check the local development service. No measurements or command results are being inferred.");
    }else if(sim){
      setBanner("warning","DEVELOPMENT SIMULATOR","All displayed measurements are explicitly simulated. Power operations, settings changes and firmware installation are disabled.");
    }else if(s.status && s.status.error){
      setBanner("error","UPS telemetry unavailable","The management API is reachable, but it has not supplied validated UPS readings.");
    }else{
      setBanner("","Native management source","Values are taken directly from the connected management API.");
    }
  }
  async function request(path,signal) {
    const response=await fetch(API+path,{method:"GET",cache:"no-store",credentials:"same-origin",signal,headers:{"Accept":"application/json"}});
    const data=await response.json();
    if(!response.ok){
      const error=new Error(String(data.code||data.error||"HTTP_"+response.status));
      error.httpStatus=response.status;
      throw error;
    }
    if(data.schemaVersion!==1)throw new Error("Unexpected management API schema");
    return data;
  }
  async function getSummary(signal) {
    const endpoints=[["health","health"],["device","device"],["capabilities","capabilities"],
                     ["status","ups/status"],["measurements","ups/measurements"],["outlets","ups/outlets"]];
    const results=await Promise.allSettled(endpoints.map(item=>request(item[1],signal)));
    const summary={};
    results.forEach((r,i)=>{
      if(r.status==="fulfilled")summary[endpoints[i][0]]=r.value;
      else summary[endpoints[i][0]+"Error"]=r.reason;
    });
    if(!summary.health) summary.fatal=summary.healthError||new Error("Health endpoint unavailable");
    return summary;
  }
  function failPage(err) {
    return add(element("div"),notice("Unable to load this management page",String(err.message||err),"error"),
      button("Try again",()=>load(true),false));
  }
  function readOnlyMessage() {
    return notice("Read-only firmware service","The current native API has no authenticated mutation endpoint for this operation. The control is intentionally disabled; it is not a successful or simulated action.","info");
  }
  function renderOverview(summary) {
    const container=element("div");
    const status=summary.status;
    const measures=summary.measurements;
    const outlets=summary.outlets;
    add(container,title("UPS Properties","Power and battery overview, measurement quality, device identity, and independently reported outlet states."));
    if(!status){
      add(container,notice("Verified measurements unavailable","The monitoring API did not provide a valid UPS status response. No sample values will be substituted.","error"));
    }
    const metrics=element("div","metrics");
    add(metrics,
      metric("Battery capacity",status?valueOrDash(status.batteryPercent,"%"):"—","Remaining charge"),
      metric("Estimated runtime",status?valueOrDash(Math.round(status.estimatedRuntimeSeconds/60)," min"):"—","At current reported load"),
      metric("Output load",status?valueOrDash(status.loadPercent,"%"):"—","UPS capacity"),
      metric("Power source",status?String(status.powerSource||"Unavailable"):"—","Verified UPS state"));
    container.appendChild(metrics);
    add(container,section("Electrical power path","Values from the management API, not local calculations."));
    const power=element("div","power-flow");
    const input=element("div","power-node");
    add(input,node("span","INPUT"),node("strong",measures?valueOrDash(measures.inputVoltage?.valueDecivolts/10," V"):"—"),node("small","Utility input voltage"));
    const ups=element("div","power-node central");
    add(ups,node("span","UPS SYSTEM"),node("strong",status?"UPS operating":"Not connected"),node("small",status?"Battery present: "+String(status.batteryPresent):"Telemetry unavailable"));
    const output=element("div","power-node");
    add(output,node("span","OUTPUT"),node("strong",measures?valueOrDash(measures.outputVoltage?.valueDecivolts/10," V"):"—"),node("small","Load-side voltage"));
    add(power,input,element("span","power-arrow"),ups,element("span","power-arrow"),output);
    container.appendChild(power);
    add(container,section("Device identity and output configuration"));
    const device=summary.device||{};
    const propertiesList=[
      ["Management firmware",String(device.firmwareVersion||"Unknown")],
      ["Platform",String(device.platform||"Unknown")],
      ["Data source",status?(status.simulated?"Simulator":"Native"):"Unavailable"],
      ["API protocol",summary.capabilities?String(summary.capabilities.transport||"Unknown"):"Unknown"],
      ["Configuration writes",summary.capabilities?.configurationWritable?"Enabled":"Unavailable"],
      ["Firmware installation",summary.capabilities?.firmwareInstall?"Enabled":"Unavailable"]
    ];
    add(container,properties(propertiesList));
    add(container,section("Load segment status","Individual load segments are read-only until a qualified control backend exists."));
    if(outlets&&Array.isArray(outlets.outlets)){
      add(container,table(["Output","State","Switchable","Control"],outlets.outlets.map(o=>[o.label||"Output "+o.id,
       o.state||"Unknown",o.switchable?"Yes":"No",statusTag(o.controlEnabled?"Enabled":"Unavailable",o.controlEnabled?"ok":"warning")])));
    }else add(container,notice("Outlet state unavailable","The API has not returned an outlet inventory.","error"));
    return container;
  }
  function renderMeasurements(summary) {
    const container=element("div");
    const status=summary.status,measurements=summary.measurements;
    add(container,title("Measurements","Power measurements from the UPS management API and their reported units."));
    if(!status || !measurements)add(container,notice("No verified sample","Input, output and battery values are unavailable until the UPS reports a valid measurement set.","error"));
    add(container,table(["Measurement","Value","Unit","Source"],[
      ["Input voltage",measurements?valueOrDash(measurements.inputVoltage?.valueDecivolts/10):"Unavailable","V",measurements?"UPS API":"Unavailable"],
      ["Output voltage",measurements?valueOrDash(measurements.outputVoltage?.valueDecivolts/10):"Unavailable","V",measurements?"UPS API":"Unavailable"],
      ["Output load",status?status.loadPercent:"Unavailable","% of rated capacity",status?"UPS API":"Unavailable"],
      ["Battery charge",status?status.batteryPercent:"Unavailable","%",status?"UPS API":"Unavailable"],
      ["Estimated runtime",status?status.estimatedRuntimeSeconds:"Unavailable","seconds",status?"UPS API":"Unavailable"],
      ["Utility supply",status?status.powerSource:"Unavailable","state",status?"UPS API":"Unavailable"]
    ]));
    add(container,section("Measurement retention"));
    add(container,readOnlyMessage());
    add(container,commands([["Export measurements","Historical export requires a persistent measurement repository."],["Clear history","No persistent measurement history is connected to this firmware service."]]));
    return container;
  }
  function renderControl(summary) {
    const container=element("div");
    add(container,title("UPS Control","Review each output segment and its available operations without risking unexpected power interruption."));
    add(container,notice("Power-changing commands are unavailable","The backend reports outletControl: false. No on/off, cycle or timed shutdown command will be sent.","warning"));
    const outlets=summary.outlets?.outlets;
    if(!outlets)add(container,notice("No verified outlet state","The API did not provide a valid outlet inventory.","error"));
    else {
      const rows=outlets.map(o=>[o.label||String(o.id),o.state||"Unknown",o.switchable?"Yes":"No",o.controlEnabled?"Capability available":"No power-control backend"]);
      add(container,table(["Outlet","Current state","Switchable","Operations"],rows));
    }
    add(container,section("Power operations","Each operation requires device-side authorization, command validation, and qualified hardware control."));
    add(container,commands([["Turn output on","Energize a selected load segment."],["Turn output off","Disconnect a selected load segment; must require explicit confirmation."],
      ["Power cycle","Disconnect and restore a selected segment after a controlled delay."],["Timed shutdown","Graceful delayed shutdown and restart policy."]]));
    return container;
  }
  function renderEvents(summary,records,paging,which) {
    const system=which==="system-log";
    const container=element("div");
    add(container,title(system?"System Log":"Event Log",system?
      "Read-only system and service events with monotonic timestamps and boot-generation context.":
      "Recorded UPS events, severity, sequence numbers and transition history."));
    if(!records){add(container,notice("Event API unavailable","The native event store did not return a valid response.","error"));return container;}
    if(records.simulated)add(container,notice("Development event stream","These records come from the simulator, not an attached UPS.","warning"));
    if(records.persistent===false)add(container,notice("Volatile event retention","Records currently remain in RAM. Persistent checkpoints require verified storage; reboot can lose uncommitted events.","info"));
    const all=Array.isArray(records.events)?records.events:[];
    const filtered=system?all.filter(e=>!e.code || ["SERVICE_START","REPORT_REJECTED","TELEMETRY_OFFLINE","TELEMETRY_ONLINE"].includes(e.code)):all;
    const rows=filtered.map(e=>[
      valueOrDash(e.id),
      e.bootGeneration!==undefined?"Boot "+e.bootGeneration:"Current session",
      e.monotonicMs!==undefined?valueOrDash(e.monotonicMs," ms"):"—",
      statusTag(e.severity||"info",e.severity==="error"?"error":e.severity==="warning"?"warning":""),
      e.code||e.message||"Event",
      e.message||""
    ]);
    add(container,table(["ID","Epoch","Time from boot","Severity","Code","Description"],rows));
    if(Number.isInteger(records.pages)&&records.pages>1){
      const pager=element("div","pager");
      const previous=button("Previous",()=>{state.eventPage=Math.max(0,state.eventPage-1);load(true);},state.eventPage===0);
      const next=button("Next",()=>{state.eventPage++;load(true);},state.eventPage>=records.pages-1);
      add(pager,previous,node("span","Page "+(state.eventPage+1)+" of "+records.pages),next);
      add(container,pager);
    }
    if(Number.isInteger(records.overwritten))add(container,node("p","Older events overwritten in the fixed RAM ring: "+records.overwritten,"label"));
    return container;
  }
  const pages={
    schedule:["Weekly Schedule","Configure recurring maintenance and shutdown windows.",[
      ["Add weekly schedule","Specify weekdays, start time, restart policy and affected loads."],["Edit scheduled operation","Modify a stored weekly entry."],["Disable schedule","Safely suspend a scheduled test or shutdown."]]],
    shutdown:["Shutdown Parameters","Runtime and delay thresholds for connected shutdown clients.",[
      ["Set runtime threshold","Set remaining-runtime shutdown conditions."],["Configure shutdown delay","Set device and outlet shutdown timing."],["Configure restart timing","Coordinate restore behavior after utility recovery."]]],
    email:["Email Notification","Recipients, notification rules and SMTP transport.",[
      ["SMTP settings","Server, transport security, authentication and sender identity."],["Recipients","Manage recipients and alert subscriptions."],["Send test notification","Verify actual authenticated delivery."]]],
    applications:["Notified Applications","Registered UPS shutdown agents and their notification state.",[
      ["Register client","Provision an authorized shutdown application."],["Edit client","Update client permissions and notification policy."],["Remove client","Revoke an existing shutdown application."]]],
    access:["Access Control","Management identities, roles, sessions and certificates.",[
      ["Users and roles","Manage authorized accounts and permissions."],["Password policy","Configure credential rules and session expiry."],["HTTPS certificates","Install and validate a device certificate and key."],["Session management","Revoke authenticated sessions."]]],
    firmware:["Firmware Upload","Independent firmware updates from approved project releases.",[
      ["Check GitHub releases","Find a verified and compatible release in the project repository."],["Verify firmware package","Validate signing, hardware compatibility and version."],["Install firmware","Stage the new image with safe interruption recovery."],["Rollback","Restore the previous proven firmware image."]]],
    network:["Network","Management interface addressing and DNS configuration.",[
      ["Configure IP settings","Configure static and automatic addressing."],["Configure DNS","Hostname and resolver settings."],["Apply network changes","Validate and safely activate management connectivity."]]],
    system:["System","Management card metadata, event retention and diagnostics.",[
      ["Device identity","Contact, location and system description."],["Event retention","Manage persistent journal and retention policy."],["Factory reset","Require confirmation and backup of preserved settings."]]],
    snmp:["SNMP","Monitoring agent, communities, users and traps.",[
      ["SNMPv3 users","Configure authenticated monitoring credentials."],["SNMP communities","Configure legacy access with access restrictions."],["Trap destinations","Define notification subscriptions and delivery."]]],
    time:["Time","Clock synchronization, time zone and daylight savings behavior.",[
      ["NTP synchronization","Configure upstream time servers."],["Time zone","Set local time and DST policy."],["Set time","Update the device clock securely."]]]
  };
  const searchTerms={
    schedule:["schedule","sched","weekly"],shutdown:["shutdown","runtime","delay"],
    email:["mail","smtp","notification"],applications:["application","client","notify"],
    access:["access","security","auth","password","certificate","ssl"],
    network:["network","ipv","dhcp","dns","tcp","host","domain"],
    system:["system","general","log","environment","snmp","config"],
    snmp:["snmp","community","trap"],time:["time","clock","ntp","timezone"]
  };
  async function loadFields(signal) {
    if(state.fields)return state.fields;
    const meta=await request("configuration/fields",signal);
    const count=Number(meta.pages);
    if(!Number.isInteger(count)||count<1||count>64)throw new Error("Invalid configuration inventory");
    const pages=await Promise.all(Array.from({length:count},(_,i)=>request("configuration/fields/"+i,signal)));
    const items=pages.flatMap(p=>Array.isArray(p.items)?p.items:[]);
    if(items.length!==Number(meta.total))throw new Error("Configuration inventory incomplete");
    state.fields=items;
    return items;
  }
  function configurationPreview(items,key) {
    const terms=searchTerms[key]||[];
    const matched=key==="system"?items:items.filter(item=>{
      const label=(String(item.group||"")+" "+String(item.name||"")).toLowerCase();
      return terms.some(term=>label.includes(term));
    });
    const content=element("div");
    add(content,section("Configuration inventory","Read-only metadata from the native firmware schema. Values are not available or inferred."));
    const controls=element("div","catalog-controls");
    const filter=element("input");
    filter.type="search";filter.setAttribute("aria-label","Search configuration fields");
    filter.placeholder="Search field name, group or type";
    filter.value=state.search;
    const count=node("span",matched.length+" related fields · "+items.length+" total cataloged","label");
    add(controls,filter,count);content.appendChild(controls);
    const mount=element("div");
    content.appendChild(mount);
    const update=()=>{
      const term=filter.value.trim().toLowerCase();
      const filtered=matched.filter(item=>
        [item.name,item.group,item.type,item.access].some(value=>String(value||"").toLowerCase().includes(term)));
      count.textContent=filtered.length+" visible · "+items.length+" cataloged";
      mount.replaceChildren(table(["Field","Group","Type","Access","Restart"],filtered.slice(0,150).map(item=>[
        item.name,item.group,item.type,item.access,item.requiresRestart?"Yes":"No"
      ])));
      if(filtered.length>150)mount.appendChild(notice("Display limit","Showing the first 150 matching schema fields. Narrow the search to see other results.","info"));
    };
    filter.addEventListener("input",()=>{state.search=filter.value;update();});
    update();
    return content;
  }
  function renderGeneral(summary,key,items) {
    const spec=pages[key];const container=element("div");
    add(container,title(spec[0],spec[1]));
    add(container,readOnlyMessage());
    add(container,section("Management operations","Disabled until the device exposes a verified and authenticated implementation."));
    add(container,commands(spec[2]));
    if(key==="firmware"){
      const info=element("p");
      info.textContent="Approved update source: bradjtrammell/dell-nmc-firmware GitHub Releases. No download or installation is performed by this page.";
      add(container,section("Release source"),info);
    }
    if(key==="system"&&summary.device){
      add(container,section("Device information"));
      add(container,properties([
        ["Firmware",summary.device.firmwareVersion||"Unknown"],
        ["Platform",summary.device.platform||"Unknown"],
        ["Hardware connection",summary.device.hardwareConnection||"Unknown"],
        ["Interface","Development management console"]
      ]));
    }
    if(items && key!=="firmware")add(container,configurationPreview(items,key));
    return container;
  }
  function renderFatal(message){
    view.replaceChildren(title("Management service unavailable","The browser could not fetch the management API."),
      notice("Unable to establish connection",message,"error"),
      button("Retry connection",()=>load(true)));
  }
  function renderNavigation() {
    nav.replaceChildren();
    groups.forEach(group=>{
      const heading=node("p",group[0],"nav-heading");
      nav.appendChild(heading);
      group[1].forEach(entry=>{
        pageMap.set(entry[0],entry[1]);
        const link=node("a",entry[1]);
        link.href="#"+entry[0];
        link.dataset.page=entry[0];
        nav.appendChild(link);
      });
    });
  }
  function selectRoute() {
    const key=location.hash.slice(1);
    return pageMap.has(key)?key:"properties";
  }
  function syncNav() {
    state.page=selectRoute();
    document.getElementById("breadcrumb-name").textContent=pageMap.get(state.page);
    nav.querySelectorAll("a").forEach(anchor=>{
      if(anchor.dataset.page===state.page)anchor.setAttribute("aria-current","page");
      else anchor.removeAttribute("aria-current");
    });
    sidebar.classList.remove("open");
    menuButton.setAttribute("aria-expanded","false");
  }
  let controller=null;
  async function load(force) {
    if(state.busy && !force)return;
    if(controller)controller.abort();
    controller=new AbortController();
    const signal=controller.signal;
    const generation=++state.generation;
    state.busy=true;
    refreshButton.disabled=true;
    const page=state.page;
    view.replaceChildren(node("div","Loading "+pageMap.get(page)+"…","loading"));
    try {
      const summary=await getSummary(signal);
      if(signal.aborted || generation!==state.generation)return;
      state.summary=summary;
      sourceInfo(summary);
      if(summary.fatal){renderFatal("The /api/v1/health request did not succeed.");return;}
      let content;
      if(page==="properties")content=renderOverview(summary);
      else if(page==="measurements")content=renderMeasurements(summary);
      else if(page==="control")content=renderControl(summary);
      else if(page==="events"||page==="system-log"){
        let events=null;
        try{
          events=await request("events"+(state.eventPage?"/"+state.eventPage:""),signal);
        }catch(error){if(error.httpStatus===503) events=null;else throw error;}
        if(signal.aborted||generation!==state.generation)return;
        content=renderEvents(summary,events,null,page);
      } else {
        let fields=null;
        if(page!=="firmware"&&page!=="applications"){
          try{fields=await loadFields(signal);}catch(error){
            if(signal.aborted||generation!==state.generation)return;
            fields=null;
          }
        }
        content=renderGeneral(summary,page,fields);
      }
      if(signal.aborted||generation!==state.generation)return;
      view.replaceChildren(content);
    } catch(error) {
      if(!signal.aborted && generation===state.generation) {
        setBanner("error","Unable to load management data","The API request failed. No fallback values were generated.");
        view.replaceChildren(failPage(error));
      }
    } finally {
      if(generation===state.generation){
        state.busy=false;
        refreshButton.disabled=false;
      }
    }
  }
  renderNavigation();
  syncNav();
  refreshButton.addEventListener("click",()=>{state.fields=null;load(true);});
  menuButton.addEventListener("click",()=>{
    const active=sidebar.classList.toggle("open");
    menuButton.setAttribute("aria-expanded",String(active));
  });
  document.addEventListener("keydown",event=>{
    if(event.key==="Escape"&&sidebar.classList.contains("open")){
      sidebar.classList.remove("open");
      menuButton.setAttribute("aria-expanded","false");
      menuButton.focus();
    }
  });
  window.addEventListener("hashchange",()=>{
    const previous=state.page;
    syncNav();
    if(state.page!==previous){
      state.eventPage=0;state.search="";
      load(true);
    }
  });
  document.addEventListener("visibilitychange",()=>{
    if(!document.hidden&&!state.busy)load(true);
  });
  window.setInterval(()=>{
    if(!document.hidden&&!state.busy)load(false);
  },20000);
  load(true);
})();