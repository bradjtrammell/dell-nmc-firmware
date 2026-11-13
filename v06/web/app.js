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
  const state = {page:"properties", generation:0, busy:false, summary:null, fields:null, eventPage:0, eventSeverity:"all", eventQuery:"", search:"", configDraft:Object.create(null),notifyDraft:[],agentDraft:[],warningDraftSeconds:120,timeDraft:{server:'',timezone:'America/New_York',dst:'automatic'},shutdownDraft:null,backupNotice:""};
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
  function renderMeasurements(summary,history) {
    const container=element("div");
    const status=summary.status,measurements=summary.measurements;
    add(container,title("Measurements","Current UPS electrical measurements and retained, independently verified measurement history."));
    if(!status||!measurements)add(container,notice("No current UPS sample","Live voltage, load and battery readings are unavailable. Historical samples, if previously collected, remain visible.","error"));
    add(container,table(["Measurement","Value","Unit","Source"],[
      ["Input voltage",measurements?valueOrDash(measurements.inputVoltage?.valueDecivolts/10):"Unavailable","V",measurements?"UPS API":"Unavailable"],
      ["Output voltage",measurements?valueOrDash(measurements.outputVoltage?.valueDecivolts/10):"Unavailable","V",measurements?"UPS API":"Unavailable"],
      ["Output load",status?status.loadPercent:"Unavailable","% of rated capacity",status?"UPS API":"Unavailable"],
      ["Battery charge",status?status.batteryPercent:"Unavailable","%",status?"UPS API":"Unavailable"],
      ["Estimated runtime",status?status.estimatedRuntimeSeconds:"Unavailable","seconds",status?"UPS API":"Unavailable"],
      ["Utility supply",status?status.powerSource:"Unavailable","state",status?"UPS API":"Unavailable"]
    ]));
    add(container,section("Measurement history","At most 64 retained validated samples; scheduled one minute apart, plus significant power transitions."));
    if(!history){
      add(container,notice("No native measurement history","This source has not exposed a verified history provider. Simulator measurements are never saved or presented as genuine history.","info"));
      return container;
    }
    const {meta,samples}=history;
    if(!meta.live)add(container,notice("UPS history available while offline","The stored readings below are from earlier verified samples. The UPS is not currently supplying live data.","warning"));
    if(!meta.persistent)add(container,notice("Volatile history","Samples are in RAM only. Power loss or restart clears them; there is no flash storage or fabricated long-term trend.","info"));
    const counters=element("div","metrics");
    const battery=samples.length?samples[0].batteryPercent:null;
    const minBattery=samples.length?Math.min(...samples.map(row=>row.batteryPercent)):null;
    const maxLoad=samples.length?Math.max(...samples.map(row=>row.loadPercent)):null;
    add(counters,metric("Recorded samples",samples.length,"Maximum 64"),
      metric("Last battery reading",valueOrDash(battery,"%"),"Historical sample"),
      metric("Lowest battery",valueOrDash(minBattery,"%"),"Retained history"),
      metric("Peak load",valueOrDash(maxLoad,"%"),"Retained history"));
    container.appendChild(counters);
    if(samples.length){
      const note=element("p","label","Blue = battery capacity; amber = UPS load. Horizontal order is oldest to newest. A vertical separator marks a reconnection.");
      container.appendChild(note);
      const chart=element("div","history-chart");
      chart.setAttribute("aria-hidden","true");
      for(const row of samples.slice().reverse()){
        const group=element("div","history-chart-column"+(row.gapBefore?" history-gap":""));
        group.title="Boot time "+row.monotonicMs+" ms, battery "+row.batteryPercent+
          "%, load "+row.loadPercent+"%, utility "+(row.acPresent?"on":"off");
        const batteryBar=element("span","history-bar history-battery");
        const loadBar=element("span","history-bar history-load");
        batteryBar.style.height=String(Math.max(1,row.batteryPercent))+"%";
        loadBar.style.height=String(Math.max(1,row.loadPercent))+"%";
        group.appendChild(batteryBar);group.appendChild(loadBar);
        chart.appendChild(group);
      }
      container.appendChild(chart);
      const controls=element("div","event-controls");
      const csv=button("Export measurements CSV",()=>saveEventFile(
        "openh910p-measurements.csv",
        "\ufeff"+[["sequence","monotonicMs","batteryPercent","loadPercent",
          "runtimeSeconds","inputDecivolts","outputDecivolts","acPresent","gapBefore"],
          ...samples.map(row=>[row.sequence,row.monotonicMs,row.batteryPercent,
            row.loadPercent,row.runtimeSeconds,row.inputDecivolts,row.outputDecivolts,
            row.acPresent,row.gapBefore])].map(row=>row.map(csvCell).join(",")).join("\r\n")+"\r\n",
        "text/csv;charset=utf-8"));
      const json=button("Export measurements JSON",()=>saveEventFile(
        "openh910p-measurements.json",JSON.stringify({
          schemaVersion:1,sampleSource:meta.sampleSource,
          live:meta.live,persistent:false,retained:samples.length,
          overwritten:meta.overwritten,latestRecordSequence:meta.latestRecordSequence,
          samples
        },null,2)+"\n","application/json;charset=utf-8"));
      add(controls,csv,json);
      container.appendChild(controls);
      add(container,table(["Sequence","Boot uptime","Battery","Load","Input","Output","Runtime","Utility / Link"],samples.map(row=>[
        row.sequence,valueOrDash(row.monotonicMs," ms"),
        valueOrDash(row.batteryPercent,"%"),valueOrDash(row.loadPercent,"%"),
        valueOrDash(row.inputDecivolts/10," V"),valueOrDash(row.outputDecivolts/10," V"),
        valueOrDash(Math.round(row.runtimeSeconds/60)," min"),
        (row.acPresent?"Utility":"Battery")+(row.gapBefore?" · after reconnect":"")
      ])));
    } else add(container,notice("No historical samples yet","The native driver has not submitted a verified reading. History remains empty until a qualified physical measurement is received.","info"));
    add(container,node("p","Records overwritten by the bounded history ring: "+String(meta.overwritten||0),"label"));
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
  /* Return one consistent retained-history snapshot. Every page must share
   * the same newest event ID and boot generation. If a new event arrives while
   * paging, retry once rather than export duplicates or silently omit rows. */
  async function loadMeasurementHistory(signal) {
    for(let attempt=0;attempt<2;attempt++){
      const first=await request("ups/history",signal);
      if(first.simulated || first.sampleSource!=="verified-native-only")
        throw new Error("Unverified measurement history source");
      if(!Array.isArray(first.samples)||!Number.isInteger(first.stored)||
         first.stored<0||first.stored>64||first.pageSize!==8||
         !Number.isInteger(first.pages)||first.pages<0||first.pages>8)
        throw new Error("Malformed history metadata");
      const remaining=await Promise.all(Array.from(
        {length:Math.max(0,first.pages-1)},(_,index)=>
          request("ups/history/"+(index+1),signal)));
      const pages=[first,...remaining];
      const all=pages.flatMap(page=>page.samples||[]);
      const valid=pages.every((page,index)=>page.page===index&&
        page.stored===first.stored&&page.overwritten===first.overwritten&&
        page.latestRecordSequence===first.latestRecordSequence&&
        page.sampleSource==="verified-native-only"&&Array.isArray(page.samples))&&
        all.length===first.stored &&
        new Set(all.map(row=>row.sequence)).size===all.length &&
        all.every(row=>Number.isInteger(row.sequence)&&row.sequence>0&&
          Number.isInteger(row.monotonicMs)&&row.monotonicMs>=0&&
          Number.isInteger(row.batteryPercent)&&row.batteryPercent>=0&&row.batteryPercent<=100&&
          Number.isInteger(row.loadPercent)&&row.loadPercent>=0&&row.loadPercent<=100&&
          Number.isInteger(row.inputDecivolts)&&row.inputDecivolts>=0&&
          Number.isInteger(row.outputDecivolts)&&row.outputDecivolts>=0&&
          Number.isInteger(row.runtimeSeconds)&&row.runtimeSeconds>=0&&
          typeof row.acPresent==="boolean"&&typeof row.gapBefore==="boolean");
      if(valid)return {meta:first,samples:all};
      if(attempt===1)throw new Error("History changed while reading; try refreshing.");
    }
    throw new Error("History could not be retrieved consistently.");
  }
  async function loadEventHistory(signal) {
    for(let attempt=0;attempt<2;attempt++) {
      const first=await request("events",signal);
      if(!Array.isArray(first.events))throw new Error("Invalid event API response");
      if(!Number.isInteger(first.pages)) {
        return {meta:first,stats:null,all:first.events};
      }
      if(!Number.isInteger(first.stored)||first.stored<0||first.stored>32 ||
         first.pages<0||first.pages>4||first.pageSize!==8)
        throw new Error("Invalid event retention metadata");
      const additional=await Promise.all(
        Array.from({length:Math.max(first.pages-1,0)},(_,i)=>request("events/"+(i+1),signal))
      );
      const pages=[first,...additional];
      const consistent=pages.every((page,i)=>
        page.page===i&&page.stored===first.stored&&
        page.overwritten===first.overwritten&&
        page.latestEventId===first.latestEventId&&
        page.currentBootGeneration===first.currentBootGeneration&&
        Array.isArray(page.events)
      );
      const all=pages.flatMap(p=>p.events);
      const ids=new Set(all.map(e=>e.id));
      if(!consistent||all.length!==first.stored||ids.size!==all.length){
        if(attempt===0)continue;
        throw new Error("Event history changed while reading; refresh to retry.");
      }
      let stats=null;
      try {
        stats=await request("events/stats",signal);
      } catch(error) {
        /* An older host simulator exposes sample events without native
         * statistics. Real native errors must not be hidden as success. */
        if(error.httpStatus!==404||!first.simulated)throw error;
      }
      if(stats && (
        stats.latestEventId!==first.latestEventId||
        stats.stored!==first.stored||
        stats.currentBootGeneration!==first.currentBootGeneration
      )){
        if(attempt===0)continue;
        throw new Error("Event history changed while computing totals; retry.");
      }
      return {meta:first,stats,all};
    }
    throw new Error("Unable to obtain a consistent event snapshot.");
  }
  function saveEventFile(filename, content, type) {
    const blob=new Blob([content],{type});
    const url=URL.createObjectURL(blob);
    const link=document.createElement("a");
    link.href=url;
    link.download=filename;
    document.body.appendChild(link);
    link.click();
    link.remove();
    window.setTimeout(()=>URL.revokeObjectURL(url),500);
  }
  function csvCell(value) {
    let raw=String(value===undefined||value===null?"":value).replace(/[\r\n]+/g," ");
    /* Escaping alone is insufficient for spreadsheet formula injection. */
    if(/^\s*[=+\-@]/.test(raw))raw="'"+raw;
    return '"'+raw.replace(/"/g,'""')+'"';
  }
  function renderEvents(summary, snapshot, which) {
    const system=which==="system-log";
    const container=element("div");
    add(container,title(system?"System Log":"Event Log",system?
      "Device and service events, searchable across all retained pages and boot generations.":
      "UPS power, battery and link history, searchable and exportable without losing page boundaries."));
    if(!snapshot) {
      add(container,notice("Event history unavailable","No readable event log was returned. The backend did not supply replacement records.","error"));
      return container;
    }
    const meta=snapshot.meta;
    const all=snapshot.all;
    const stats=snapshot.stats;
    if(meta.simulated)add(container,notice("Development event stream","This history is simulated and is not connected to a physical UPS.","warning"));
    if(meta.persistent===false)add(container,notice("Event retention is not yet automatic","The backend retains up to 32 records in RAM. Its journal checkpoint is not enabled on unqualified flash, so uncommitted history may be lost after reboot.","info"));
    const counters=element("div","metrics");
    const levels={error:0,warning:0,info:0};
    all.forEach(e=>{if(Object.prototype.hasOwnProperty.call(levels,e.severity))levels[e.severity]++;});
    add(counters,metric("Retained records",String(all.length),"Maximum 32 in current firmware"),
      metric("Critical / error",String(stats?stats.error:levels.error),"Retained events"),
      metric("Warnings",String(stats?stats.warning:levels.warning),"Retained events"),
      metric("Older records lost",meta.overwritten===undefined?"—":String(meta.overwritten),"Ring buffer overwrite count"));
    container.appendChild(counters);
    const controls=element("div","event-controls");
    const selection=element("select","event-select");
    selection.setAttribute("aria-label","Filter event severity");
    [["all","All severities"],["error","Errors"],["warning","Warnings"],["info","Information"]].forEach(([key,label])=>{
      const option=element("option","",label);
      option.value=key;
      selection.appendChild(option);
    });
    selection.value=state.eventSeverity;
    const search=element("input","event-search");
    search.type="search";
    search.setAttribute("aria-label","Search retained events");
    search.placeholder="Search event code, description or ID";
    search.value=state.eventQuery;
    const exportCsv=button("Export CSV",null,false);
    const exportJson=button("Export JSON",null,false);
    const count=node("span","","label");
    add(controls,selection,search,count,exportCsv,exportJson);
    container.appendChild(controls);
    const results=element("div");
    container.appendChild(results);
    const systemCodes=new Set(["SERVICE_START","REPORT_REJECTED","TELEMETRY_OFFLINE","TELEMETRY_ONLINE"]);
    function selected() {
      const query=state.eventQuery.trim().toLowerCase();
      return all.filter(e=>(!system||!e.code||systemCodes.has(e.code)) &&
        (state.eventSeverity==="all"||e.severity===state.eventSeverity) &&
        (!query||[e.id,e.code,e.message,e.severity,e.bootGeneration]
         .some(value=>String(value===undefined?"":value).toLowerCase().includes(query))));
    }
    function csv(rows) {
      const data=[["id","bootGeneration","monotonicMs","severity","code","message"],
        ...rows.map(e=>[e.id,e.bootGeneration,e.monotonicMs,e.severity,e.code,e.message])];
      return "\ufeff"+data.map(line=>line.map(csvCell).join(",")).join("\r\n")+"\r\n";
    }
    function exportName(ext) {
      return "openh910p-"+(system?"system-log":"event-log")+"-boot"+
        String(meta.currentBootGeneration||1)+"."+ext;
    }
    exportCsv.addEventListener("click",()=>{
      const rows=selected();
      if(rows.length)saveEventFile(exportName("csv"),csv(rows),"text/csv;charset=utf-8");
    });
    exportJson.addEventListener("click",()=>{
      const rows=selected();
      if(rows.length)saveEventFile(exportName("json"),JSON.stringify({
        schemaVersion:1,source:meta.source,simulated:meta.simulated,
        exportedRecords:rows.length,stored:meta.stored,
        overwritten:meta.overwritten||0,
        currentBootGeneration:meta.currentBootGeneration,
        persistent:meta.persistent===true,
        events:rows
      },null,2)+"\n","application/json;charset=utf-8");
    });
    function update() {
      const filtered=selected();
      const pages=Math.max(1,Math.ceil(filtered.length/8));
      state.eventPage=Math.min(state.eventPage,pages-1);
      count.textContent=filtered.length+" matching · "+all.length+" retained";
      exportCsv.disabled=exportJson.disabled=!filtered.length;
      const slice=filtered.slice(state.eventPage*8,(state.eventPage+1)*8);
      const rows=slice.map(e=>[
        valueOrDash(e.id),
        e.bootGeneration===undefined?"Current session":"Boot "+e.bootGeneration,
        e.monotonicMs===undefined?"—":valueOrDash(e.monotonicMs," ms"),
        statusTag(e.severity||"info",e.severity==="error"?"error":e.severity==="warning"?"warning":""),
        e.code||"EVENT",
        e.message||""
      ]);
      results.replaceChildren(table(["ID","Epoch","Time from boot","Severity","Code","Description"],rows));
      if(pages>1){
        const pager=element("div","pager");
        const previous=button("Previous",()=>{state.eventPage--;update();},state.eventPage===0);
        const next=button("Next",()=>{state.eventPage++;update();},state.eventPage>=pages-1);
        add(pager,previous,node("span","Page "+(state.eventPage+1)+" of "+pages),next);
        results.appendChild(pager);
      }
    }
    selection.addEventListener("change",()=>{state.eventSeverity=selection.value;state.eventPage=0;update();});
    search.addEventListener("input",()=>{state.eventQuery=search.value;state.eventPage=0;update();});
    update();
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
  /* In-browser review draft only. A real authenticated, two-phase apply and
   * network rollback protocol must exist before any POST is enabled. */
  function configIpv4(value) {
    const chunks=value.split(".");
    if(chunks.length!==4)return null;
    let n=0;
    for(const chunk of chunks){
      if(!/^(0|[1-9][0-9]{0,2})$/.test(chunk))return null;
      const octet=Number(chunk);
      if(octet>255)return null;
      n=(n*256+octet)>>>0;
    }
    return n;
  }
  function validConfigField(field,value) {
    if(typeof value!=="string"||value.length===0||value.length>=64)return false;
    const ip=field.type==="ipv4"||field.type==="netmask"?configIpv4(value):null;
    switch(field.type){
      case "hostname":
        return value.length<=49&&/^[a-z0-9](?:[a-z0-9-]*[a-z0-9])?$/i.test(value);
      case "domain":
        return value.length<=49&&value.split(".").every(label=>
          /^[a-z0-9](?:[a-z0-9-]*[a-z0-9])?$/i.test(label));
      case "dhcp":return value==="true"||value==="false";
      case "ipv4":
        return ip!==null && (ip>>>24)>0&&(ip>>>24)<224&&(ip>>>24)!==127;
      case "netmask": {
        if(ip===null||ip===0)return false;
        const inverse=(~ip)>>>0;
        return (inverse&(inverse+1))===0;
      }
      case "language":return ["AUTO","FRE","ENG","SPA","GER","ITA"].includes(value);
      case "string":
        return value.length<=field.max&&/^[\x20-\x7e]+$/.test(value);
      default:return false;
    }
  }
  function configDraftErrors(fields,draft=state.configDraft) {
    const errors=[];
    for(const field of fields) {
      const value=draft[field.name];
      if(value!==undefined&&value!==""&&!validConfigField(field,value))
        errors.push(field.label+" is not valid for "+field.type+".");
    }
    const d=Boolean(draft["System.Network.DHCP"]==="false");
    const dhcp=draft["System.Network.DHCP"];
    const ip=draft["System.Network.IPAddress"];
    const mask=draft["System.Network.IPMask"];
    const gateway=draft["System.Network.IPGateway"];
    if(d&&!ip)errors.push("A static IPv4 profile requires an IP address.");
    if(d&&!mask)errors.push("A static IPv4 profile requires a subnet mask.");
    if((ip||mask||gateway)&&(!ip||!mask||!dhcp))
      errors.push("Static IPv4 values require assignment mode, address and subnet mask.");
    if(ip&&mask){
      const addr=configIpv4(ip),netmask=configIpv4(mask);
      if(addr!==null&&netmask!==null && validConfigField({type:"netmask"},mask)){
        const hosts=(~netmask)>>>0;
        if(hosts!==0&&(((addr&hosts)>>>0)===0||((addr&hosts)>>>0)===hosts))
          errors.push("The IPv4 address cannot be the subnet network or broadcast address.");
        if(gateway){
          const gw=configIpv4(gateway);
          if(gw!==null && (((addr&netmask)>>>0)!==((gw&netmask)>>>0)||
              gw===addr|| (hosts!==0&&(((gw&hosts)>>>0)===0||((gw&hosts)>>>0)===hosts))))
            errors.push("The gateway must be a separate usable address in the same subnet.");
        }
      }
    }
    return errors;
  }
  function renderConfigDraft(staging,key) {
    const panel=element("div","config-draft");
    if(!staging||!Array.isArray(staging.fields)||staging.configurationWritable!==false||
       staging.authorizationAvailable!==false||staging.activeValuesAvailable!==false)
      return add(panel,notice("Configuration editor unavailable","The server has not advertised a safe, unauthenticated-write-disabled staging contract.","error"));
    const allowed=new Set([
      "System.Network.HostName","System.Network.DomainName",
      "System.Network.DHCP","System.Network.IPAddress",
      "System.Network.IPMask","System.Network.IPGateway",
      "System.Contact","System.Location","System.Language"]);
    const all=staging.fields.filter(field=>allowed.has(field.name));
    if(all.length!==9||new Set(all.map(field=>field.name)).size!==9)
      return add(panel,notice("Unrecognized configuration schema","The server did not return the exact supported field inventory.","error"));
    const group=key==="network"?all.filter(f=>f.name.startsWith("System.Network.")):
      all.filter(f=>!f.name.startsWith("System.Network."));
    add(panel,section("Local configuration draft","Validated values are exportable for review only. Nothing is applied, sent to the card, or saved to device flash."));
    const editor=element("form","config-form");
    editor.addEventListener("submit",event=>event.preventDefault());
    const errors=element("div","config-errors");
    errors.setAttribute("role","status");
    errors.setAttribute("aria-live","polite");
    for(const field of group){
      const item=element("div","config-field");
      const label=element("label","",field.label);
      const id="stage-"+field.name.replace(/[^a-zA-Z0-9]/g,"-");
      label.htmlFor=id;
      let input;
      if(field.type==="dhcp"||field.type==="language"){
        input=element("select","config-input");
        const options=field.type==="dhcp"?[
          ["","Not specified"],["true","Automatic (DHCP)"],["false","Static IPv4"]]:
          [["","Not specified"],...["AUTO","FRE","ENG","SPA","GER","ITA"].map(v=>[v,v])];
        options.forEach(([value,name])=>{
          const o=element("option","",name);o.value=value;input.appendChild(o);
        });
      }else{
        input=element("input","config-input");
        input.type="text";
        input.maxLength=field.max||49;
        input.autocomplete="off";
        input.spellcheck=false;
      }
      input.id=id;
      input.value=state.configDraft[field.name]||"";
      input.setAttribute("aria-label",field.label);
      const hint=element("small","",field.type==="ipv4"||field.type==="netmask"?
         "Four decimal octets; canonical dotted format":"Optional until used in a complete profile");
      add(item,label,input,hint);editor.appendChild(item);
      const updateValue=()=>{
        if(input.value==="")delete state.configDraft[field.name];
        else state.configDraft[field.name]=input.value;
        update();
      };
      input.addEventListener("input",updateValue);
      input.addEventListener("change",updateValue);
    }
    panel.appendChild(editor);
    const actions=element("div","config-actions");
    const exportButton=button("Export validated review draft",null,true);
    const summary=element("div","label");
    add(actions,exportButton,summary);
    add(panel,errors,actions);
    function update(){
      const issues=configDraftErrors(all);
      const count=Object.keys(state.configDraft).filter(name=>allowed.has(name)).length;
      const empty=count===0;
      errors.replaceChildren();
      for(const issue of issues)errors.appendChild(node("p",issue));
      exportButton.disabled=empty||issues.length!==0;
      summary.textContent=issues.length?
        issues.length+" validation issue(s) · no export":
        empty?"No local changes entered":
        count+" field(s) staged locally; not applied";
    }
    exportButton.addEventListener("click",()=>{
      const issues=configDraftErrors(all);
      if(issues.length||!Object.keys(state.configDraft).length)return;
      const fields={};
      for(const f of all) {
        const value=state.configDraft[f.name];
        if(value!==undefined&&value!=="")fields[f.name]=value;
      }
      if(!Object.keys(fields).length)return;
      saveEventFile("openh910p-configuration-review.json",JSON.stringify({
        schemaVersion:1,kind:"openh910p-local-configuration-review",
        applied:false,installable:false,origin:"operator-entered-local-draft",
        fields
      },null,2)+"\n","application/json;charset=utf-8");
    });
    update();
    return panel;
  }

  function renderWeeklyDraft(schema){
    const panel=element("div","weekly-draft");
    if(!schema||schema.weeklyPlannerCompiled!==true ||
       schema.scheduleWritable!==false||schema.activeScheduleAvailable!==false ||
       schema.slotSeconds!==1800||schema.dayCount!==7 ||
       schema.slotsPerDay!==48){
      add(panel,notice("Weekly schedule metadata unavailable",
        "The backend has not advertised the validated, read-only seven-day schedule contract.","error"));
      return panel;
    }
    add(panel,section("Weekly power schedule draft",
      "Seven days, half-hour increments. The final UPS schedule is not read from this preview and no power command is sent."));
    add(panel,notice("Schedule is not active",
      "These are local planning fields only. The device clock, authenticated control path and storage partition are not qualified. Saving a review file cannot switch outlets or activate a schedule.","info"));
    const days=["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"];
    if(!state.weeklyDraft){
      state.weeklyDraft=days.map(()=>({offSeconds:-1,onSeconds:-1}));
    }
    const form=element("form","weekly-form");
    form.addEventListener("submit",e=>e.preventDefault());
    const summary=element("div","label");
    const errors=element("div","config-errors");
    errors.setAttribute("role","status");
    errors.setAttribute("aria-live","polite");
    const actions=element("div","config-actions");
    const save=button("Export weekly schedule review",null,true);
    function update(){
      const errorsList=[];
      let total=0;
      state.weeklyDraft.forEach((entry,day)=>{
        for(const value of [entry.offSeconds,entry.onSeconds]){
          if(!Number.isInteger(value)||!(value===-1 ||
             (value>=0&&value<=84600&&value%1800===0)))
            errorsList.push(days[day]+" has an invalid time slot.");
          if(value>=0)total++;
        }
        if(entry.offSeconds>=0&&entry.offSeconds===entry.onSeconds)
          errorsList.push(days[day]+" cannot turn power off and on at the same time.");
      });
      errors.replaceChildren(...errorsList.map(e=>node("p",e)));
      save.disabled=total===0||errorsList.length>0;
      summary.textContent=errorsList.length?
        errorsList.length+" invalid schedule entry/entries":
        total===0?"No weekly actions selected":
        total+" planned action(s); no changes applied";
    }
    function formatTime(seconds) {
      const hours=Math.floor(seconds/3600);
      const minute=(seconds%3600)/60;
      return String(hours).padStart(2,"0")+":"+
        String(minute).padStart(2,"0");
    }
    days.forEach((label,day)=>{
      const row=element("fieldset","weekly-day");
      const legend=element("legend","",label);
      row.appendChild(legend);
      [["offSeconds","Turn output off"],["onSeconds","Turn output on"]].forEach(([key,description])=>{
        const field=element("div","config-field");
        const text=element("label","",description);
        const id="schedule-"+day+"-"+key;
        text.htmlFor=id;
        const select=element("select","config-input");
        select.id=id;
        select.setAttribute("aria-label",label+" "+description);
        const disabled=element("option","","Not scheduled");
        disabled.value="-1";select.appendChild(disabled);
        for(let slot=0;slot<48;slot++){
          const option=element("option","",formatTime(slot*1800));
          option.value=String(slot*1800);
          select.appendChild(option);
        }
        select.value=String(state.weeklyDraft[day][key]);
        select.addEventListener("change",()=>{
          state.weeklyDraft[day][key]=Number(select.value);
          update();
        });
        add(field,text,select);
        row.appendChild(field);
      });
      form.appendChild(row);
    });
    panel.appendChild(form);
    add(actions,save,summary);
    add(panel,errors,actions);
    save.addEventListener("click",()=>{
      update();
      if(save.disabled)return;
      saveEventFile("openh910p-weekly-schedule-review.json",JSON.stringify({
        schemaVersion:1,kind:"openh910p-weekly-schedule-review",
        applied:false,installable:false,origin:"operator-entered-local-draft",
        timezone:"device-time-not-qualified",slotSeconds:1800,
        days:state.weeklyDraft.map((entry,day)=>({
          weekday:day,offSeconds:entry.offSeconds,onSeconds:entry.onSeconds
        }))
      },null,2)+"\n","application/json;charset=utf-8");
    });
    update();
    return panel;
  }

  function renderAccessSecurity(security) {
    const panel=element("div","security-status");
    add(panel,section("Authentication and session enforcement","Actual backend qualification status. This preview does not accept account passwords."));
    if(!security || security.sessionEngineCompiled!==true) {
      add(panel,notice("Security status unavailable",
        "No trustworthy authentication status was returned. Management writes remain disabled.","error"));
      return panel;
    }
    if(!security.authenticationAvailable) {
      add(panel,notice("Authentication not yet connected to the physical card",
        "The independent C session core is compiled, but hardware TLS, secure entropy, password verification, and persistent credential storage are not qualified. Login, session cookies, and all mutations are disabled.","warning"));
    }
    const rows=[
      ["Native session core",security.sessionEngineCompiled?"Compiled":"Unavailable"],
      ["Account login",security.loginEnabled?"Enabled":"Disabled"],
      ["Qualified TLS",security.tlsQualified?"Ready":"Not qualified"],
      ["Secure random generator",security.entropyQualified?"Ready":"Not qualified"],
      ["Credential verifier",security.credentialStoreQualified?"Ready":"Not qualified"],
      ["Authenticated configuration writes",security.mutationsAuthorized?"Enabled":"Disabled"],
      ["Concurrent sessions",valueOrDash(security.maximumSessions)],
      ["Session idle timeout",valueOrDash(security.idleTimeoutSeconds," seconds")],
      ["Maximum session lifetime",valueOrDash(security.absoluteTimeoutSeconds," seconds")],
      ["Failed attempts before lockout",valueOrDash(security.maximumFailedAttempts)],
      ["Lockout duration",valueOrDash(security.lockoutSeconds," seconds")],
      ["CSRF verification for mutations",security.csrfRequiredForWrites?"Required":"Not configured"],
      ["Replay protection",security.increasingRequestIdRequired?"Increasing request ID required":"Not configured"]
    ];
    add(panel,properties(rows));
    add(panel,notice("Operator accounts remain locked",
      "No default user is created and no browser password is transmitted. Configure users, roles, certificates and sessions only after the real TLS/credential providers and device-side storage are accepted.","info"));
    return panel;
  }
  function renderNotificationRules(policy,delivery,key){
    const panel=element("div","notification-rules");
    add(panel,section("Event notification routing",
      "Eight typed events, destination slots, cooldown protection, and bounded retry delivery."));
    if(!policy||policy.routerCompiled!==true||
       policy.maximumRules!==8||policy.queueCapacity!==24 ||
       policy.deliveryAvailable!==false||
       !Array.isArray(policy.events)||policy.events.length!==8){
      add(panel,notice("Notification policy unavailable",
        "The native server did not return a supported notification policy. No routing changes can be prepared.","error"));
      return panel;
    }
    add(panel,notice("Delivery is not connected to the card",
      "The native queue/router and retry engine are implemented, but outbound transports and recipient credentials are not configured or qualified. These local drafts do not change delivery settings.","info"));
    if(delivery){
      add(panel,properties([
        ["Queued notifications",valueOrDash(delivery.pending)],
        ["Delivered",valueOrDash(delivery.delivered)],
        ["Retries",valueOrDash(delivery.retried)],
        ["Dropped",valueOrDash(delivery.dropped)],
        ["Failed",valueOrDash(delivery.failed)]
      ]));
    }
    const controls=element("div","notification-controls");
    const rulesMount=element("div","notification-rule-list");
    const footer=element("div","config-actions");
    const addRule=button("Add rule",()=>{
      if(state.notifyDraft.length>=8)return;
      state.notifyDraft.push({event:"UTILITY_LOST",
        transport:key==="snmp"?"snmp-trap":"email",destination:0,cooldownSeconds:60});
      draw();
    },false);
    const exportRule=button("Export routing review",null,true);
    const summary=element("span","label");
    add(controls,addRule);
    add(footer,exportRule,summary);
    add(panel,controls,rulesMount,footer);
    const events=policy.events;
    const transports=["email","webhook","snmp-trap"];
    function selector(options,value,label,change){
      const select=element("select","config-input");
      select.setAttribute("aria-label",label);
      for(const option of options){
        const entry=element("option","",String(option));
        entry.value=String(option);
        select.appendChild(entry);
      }
      select.value=String(value);
      select.addEventListener("change",()=>change(select.value));
      return select;
    }
    function valid(){
      const errors=[];
      const signatures=new Set();
      state.notifyDraft.forEach((rule,i)=>{
        if(!events.includes(rule.event)||!transports.includes(rule.transport)||
           !Number.isInteger(rule.destination)||rule.destination<0||rule.destination>7||
           !Number.isInteger(rule.cooldownSeconds)||
           rule.cooldownSeconds<0||rule.cooldownSeconds>3600)
          errors.push("Rule "+(i+1)+" has invalid values.");
        const signature=[rule.event,rule.transport,rule.destination].join(":");
        if(signatures.has(signature))errors.push("Rule "+(i+1)+" duplicates another destination and event.");
        signatures.add(signature);
      });
      return errors;
    }
    function draw(){
      rulesMount.replaceChildren();
      state.notifyDraft.forEach((rule,i)=>{
        const row=element("div","notification-rule");
        const heading=element("strong","", "Rule "+(i+1));
        const fields=element("div","notification-fields");
        const event=selector(events,rule.event,"Rule "+(i+1)+" event",value=>{
          rule.event=value;update();
        });
        const transport=selector(transports,rule.transport,"Rule "+(i+1)+" transport",value=>{
          rule.transport=value;update();
        });
        const slots=Array.from({length:8},(_,index)=>index);
        const destination=selector(slots,rule.destination,"Rule "+(i+1)+" destination slot",value=>{
          rule.destination=Number(value);update();
        });
        const cooldown=element("input","config-input");
        cooldown.type="number";cooldown.min="0";cooldown.max="3600";cooldown.step="1";
        cooldown.value=String(rule.cooldownSeconds);
        cooldown.setAttribute("aria-label","Rule "+(i+1)+" cooldown in seconds");
        cooldown.addEventListener("input",()=>{
          rule.cooldownSeconds=cooldown.value.trim()===""?NaN:Number(cooldown.value);
          update();
        });
        const remove=button("Remove",()=>{
          state.notifyDraft.splice(i,1);draw();
        });
        add(fields,event,transport,destination,cooldown);
        add(row,heading,fields,remove);
        rulesMount.appendChild(row);
      });
      addRule.disabled=state.notifyDraft.length>=8;
      update();
    }
    function update(){
      const issues=valid();
      exportRule.disabled=state.notifyDraft.length===0||issues.length>0;
      summary.textContent=issues.length?issues.join(" "):
        state.notifyDraft.length+" local rule(s) staged · none applied";
    }
    exportRule.addEventListener("click",()=>{
      update();
      if(exportRule.disabled)return;
      saveEventFile("openh910p-notification-rules-review.json",JSON.stringify({
        schemaVersion:1,kind:"openh910p-notification-routing-review",
        applied:false,installable:false,destinationsProvisioned:false,
        deliveryEnabled:false,
        rules:state.notifyDraft.map(r=>({
          event:r.event,transport:r.transport,
          destinationSlot:r.destination,cooldownSeconds:r.cooldownSeconds
        }))
      },null,2)+"\n","application/json;charset=utf-8");
    });
    draw();
    return panel;
  }

  function renderShutdownPolicy(schema,advisory){
    const panel=element("div","shutdown-policy");
    if(!schema||schema.policyModelCompiled!==true||
       schema.minimumShutdownDurationSeconds!==120 ||
       schema.maximumShutdownDurationSeconds!==9999 ||
       schema.outlets!==3 || schema.policyWritable!==false){
      add(panel,notice("UPS shutdown settings unavailable",
        "The server did not provide the complete validated three-outlet shutdown policy schema.","error"));
      return panel;
    }
    add(panel,section("UPS and outlet shutdown thresholds",
      "Shutdown duration, battery limits, restart level, and independent outlet timing for all three UPS outputs."));
    if(advisory&&advisory.resource==="shutdown.advisory"){
      add(panel,properties([
        ["Battery or runtime threshold reached",advisory.recommendAgentWarning?"Yes":"No"],
        ["Power commands",advisory.powerCommandsEnabled?"Enabled":"Disabled"],
        ["Physical shutdown executed",advisory.physicalShutdownExecuted?"Yes":"No"]
      ]));
    }else{
      add(panel,notice("No validated active shutdown policy",
        "The native card has not supplied a protected policy with verified UPS telemetry. The settings below are a local review only.","info"));
    }
    if(!state.shutdownDraft){
      state.shutdownDraft={
        master:{duration:120,timerSelected:false,forcedReboot:false,
          timerSeconds:0,runtimeSeconds:0,capacityPercent:0,restartPercent:0},
        segments:[{duration:120,shutdownDelay:0,startupDelay:0,capacityPercent:0},
                  {duration:120,shutdownDelay:0,startupDelay:0,capacityPercent:0}]
      };
    }
    const draft=state.shutdownDraft;
    const fields=element("div","shutdown-fields");
    const warnings=element("div","config-errors");
    warnings.setAttribute("role","status");warnings.setAttribute("aria-live","polite");
    const buttonExport=button("Export shutdown-policy review",null,true);
    const footer=element("div","config-actions");
    const summary=node("span","Local draft only; no changes applied.","label");
    add(footer,buttonExport,summary);
    function entry(host,key,label,min,max,step){
      const wrap=element("div","config-field");
      const id="shutdown-"+(host===draft.master?"master":host===draft.segments[0]?"outlet-2":"outlet-3")+"-"+key;
      const text=element("label","",label);text.htmlFor=id;
      const input=element("input","config-input");
      input.type="number";input.id=id;input.min=String(min);
      input.max=String(max);input.step=String(step);
      input.value=String(host[key]);
      input.addEventListener("input",()=>{
        host[key]=input.value.trim()===""?NaN:Number(input.value);
        update();
      });
      add(wrap,text,input);return wrap;
    }
    function toggle(host,key,label){
      const wrap=element("div","config-field");
      const text=element("label","",label);
      const id="shutdown-"+key;text.htmlFor=id;
      const select=element("select","config-input");select.id=id;
      [["false","Disabled"],["true","Enabled"]].forEach(([value,name])=>{
        const option=element("option","",name);option.value=value;
        select.appendChild(option);
      });
      select.value=String(host[key]);
      select.addEventListener("change",()=>{host[key]=select.value==="true";update();});
      add(wrap,text,select);return wrap;
    }
    const sectionMaster=element("fieldset","shutdown-outlet");
    sectionMaster.appendChild(element("legend","","Main UPS"));
    const masterFields=element("div","shutdown-grid");
    add(masterFields,
      entry(draft.master,"duration","Shutdown duration (seconds)",120,9999,1),
      toggle(draft.master,"timerSelected","Use timed shutdown"),
      entry(draft.master,"timerSeconds","Shutdown timer (seconds, 60-second steps)",0,5999940,60),
      entry(draft.master,"runtimeSeconds","Minimum remaining runtime (seconds)",0,99999,1),
      entry(draft.master,"capacityPercent","Minimum battery charge (%)",0,100,1),
      entry(draft.master,"restartPercent","Battery charge required for restart (%)",0,100,1),
      toggle(draft.master,"forcedReboot","Forced reboot after shutdown"));
    sectionMaster.appendChild(masterFields);
    fields.appendChild(sectionMaster);
    draft.segments.forEach((segment,index)=>{
      const outlet=element("fieldset","shutdown-outlet");
      outlet.appendChild(element("legend","","Outlet "+(index+2)));
      const grid=element("div","shutdown-grid");
      add(grid,
        entry(segment,"duration","Shutdown duration (seconds)",120,9999,1),
        entry(segment,"shutdownDelay","Shutdown delay (seconds)",0,99999,1),
        entry(segment,"startupDelay","Startup delay (seconds)",0,65535,1),
        entry(segment,"capacityPercent","Minimum battery charge (%)",0,100,1));
      outlet.appendChild(grid);fields.appendChild(outlet);
    });
    function validNumber(value,min,max,step){
      return Number.isInteger(value)&&value>=min&&value<=max&&value%step===0;
    }
    function validate(){
      const err=[],m=draft.master;
      const check=(value,min,max,step,label)=>{
        if(!validNumber(value,min,max,step))err.push(label+" must be within the supported range.");
      };
      check(m.duration,120,9999,1,"Main shutdown duration");
      check(m.timerSeconds,0,5999940,60,"Main shutdown timer");
      check(m.runtimeSeconds,0,99999,1,"Runtime limit");
      check(m.capacityPercent,0,100,1,"Battery charge threshold");
      check(m.restartPercent,0,100,1,"Restart battery threshold");
      if(typeof m.timerSelected!=="boolean"||typeof m.forcedReboot!=="boolean")
        err.push("The main UPS switches must be enabled or disabled.");
      draft.segments.forEach((o,i)=>{
        const name="Outlet "+(i+2);
        check(o.duration,120,9999,1,name+" shutdown duration");
        check(o.shutdownDelay,0,99999,1,name+" shutdown delay");
        check(o.startupDelay,0,65535,1,name+" startup delay");
        check(o.capacityPercent,0,100,1,name+" battery threshold");
      });
      return err;
    }
    function update(){
      const issues=validate();
      warnings.replaceChildren(...issues.map(message=>node("p",message)));
      buttonExport.disabled=issues.length>0;
      summary.textContent=issues.length?
        issues.length+" invalid setting(s)":
        "All 3 outlets validated; local draft only";
    }
    buttonExport.addEventListener("click",()=>{
      update();if(buttonExport.disabled)return;
      saveEventFile("openh910p-shutdown-policy-review.json",JSON.stringify({
        schemaVersion:1,kind:"openh910p-shutdown-policy-review",
        applied:false,installable:false,physicalPowerControl:false,
        master:{...draft.master},
        outlets:draft.segments.map((s,i)=>({outlet:i+2,...s}))
      },null,2)+"\n","application/json;charset=utf-8");
    });
    add(panel,fields,warnings,footer);
    update();return panel;
  }

  function renderTimeService(policy,status){
    const panel=element("div","time-service");
    add(panel,section("Clock and time synchronization",
      "Trusted network time, device time zone, daylight saving behavior and synchronization health."));
    if(!policy||policy.ntpWireClientCompiled!==true||
       policy.maximumResponseDelayMs!==5000||
       policy.maximumSampleAgeSeconds!==3600){
      add(panel,notice("Time service capabilities unavailable",
        "The management service returned an unrecognized time synchronization contract.","error"));
      return panel;
    }
    if(status&&status.resource==="time.status"&&status.sourceVerified===true){
      add(panel,properties([
        ["UTC timestamp",valueOrDash(status.utcUnixSeconds)+" seconds since 1970"],
        ["Milliseconds",valueOrDash(status.utcMilliseconds)],
        ["Network response delay",valueOrDash(status.roundTripMs," ms")],
        ["Estimated time uncertainty",valueOrDash(status.uncertaintyMs," ms")],
        ["Hardware real-time clock",status.rtcSet?"Set":"Not set"]
      ]));
    }else add(panel,notice("No qualified clock synchronization",
      "The card has not received a validated time response. Timestamps and scheduling cannot rely on a synchronized wall clock yet.","info"));
    add(panel,notice("Network time is not yet enabled on this card",
      "The native NTP client validates request nonces, response timing, trust boundaries and clock rollover. An isolated trusted NTP source, verified network adapter, device RTC and time-zone rules are still required.","info"));
    add(panel,section("Local time settings review",
      "Prepare an NTP server and time-zone policy for later authenticated provisioning. This does not set the clock."));
    const editor=element("form","time-form");
    editor.addEventListener("submit",event=>event.preventDefault());
    const makeField=(label,id,control)=>{
      const wrap=element("div","config-field");
      const l=element("label","",label);l.htmlFor=id;control.id=id;
      add(wrap,l,control);editor.appendChild(wrap);
      return control;
    };
    const server=element("input","config-input");
    server.type="text";server.maxLength=253;server.autocomplete="off";
    server.spellcheck=false;
    server.value=state.timeDraft.server;
    makeField("Trusted NTP server name or IPv4 address","time-ntp-server",server);
    const zone=element("input","config-input");
    zone.type="text";zone.maxLength=64;zone.autocomplete="off";
    zone.spellcheck=false;
    zone.value=state.timeDraft.timezone;
    makeField("IANA time zone (for example America/New_York)","time-zone",zone);
    const dst=element("select","config-input");
    [["automatic","Use regional daylight saving rules"],
     ["standard","Standard time only"],["utc","UTC without DST"]].forEach(([value,label])=>{
      const option=element("option","",label);option.value=value;dst.appendChild(option);
    });
    dst.value=state.timeDraft.dst;
    makeField("Daylight saving behavior","time-dst",dst);
    const hint=node("p","Time zone and daylight saving rules require a qualified device timezone database before they can be activated.","label");
    const errors=element("div","config-errors");
    errors.setAttribute("role","status");errors.setAttribute("aria-live","polite");
    const actions=element("div","config-actions");
    const exportButton=button("Export time-settings review",null,true);
    add(actions,exportButton);
    add(panel,editor,hint,errors,actions);
    function validate(){
      const issues=[];
      const name=state.timeDraft.server;
      if(!name||name.length>253||name.includes("://")||
         !name.split(".").every(segment=>/^[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$/.test(segment)))
        issues.push("Enter a valid NTP server hostname or IPv4 address.");
      const timezone=state.timeDraft.timezone;
      if(!/^(UTC|[A-Za-z_+-]+(?:\/[A-Za-z_+-]+){1,2})$/.test(timezone))
        issues.push("Enter UTC or a valid IANA-style region/city time zone.");
      if(!["automatic","standard","utc"].includes(state.timeDraft.dst))
        issues.push("Choose a daylight saving policy.");
      if(state.timeDraft.dst==="utc"&&timezone!=="UTC")
        issues.push("Select UTC as the time zone when disabling regional time offsets.");
      return issues;
    }
    function update(){
      state.timeDraft.server=server.value.trim();
      state.timeDraft.timezone=zone.value.trim();
      state.timeDraft.dst=dst.value;
      const issues=validate();
      errors.replaceChildren(...issues.map(issue=>node("p",issue)));
      exportButton.disabled=issues.length>0;
    }
    server.addEventListener("input",update);
    zone.addEventListener("input",update);
    dst.addEventListener("change",update);
    exportButton.addEventListener("click",()=>{
      update();if(exportButton.disabled)return;
      saveEventFile("openh910p-time-settings-review.json",JSON.stringify({
        schemaVersion:1,kind:"openh910p-time-settings-review",
        applied:false,installable:false,
        server:state.timeDraft.server,
        timezone:state.timeDraft.timezone,
        dst:state.timeDraft.dst,
        rtcQualified:false,networkTimeSecurityQualified:false
      },null,2)+"\n","application/json;charset=utf-8");
    });
    update();
    return panel;
  }

  function renderShutdownAgents(policy,status,page){
    const panel=element("div","agents-panel");
    if(!policy||policy.coordinatorCompiled!==true||
       policy.maximumClients!==12||policy.registrationWritable!==false){
      add(panel,notice("Shutdown client management unavailable",
        "The native service has not returned the expected protected client-management contract.","error"));
      return panel;
    }
    add(panel,section(page==="applications"?"Shutdown client coordination":"Shutdown warning coordination",
      "Manage registered shutdown applications, their connection health and readiness before a UPS shutdown."));
    add(panel,notice("Live client control not yet enabled",
      "The native registry supports verified heartbeats, advance warnings and client acknowledgements. Physical client connections and authenticated registration are not available on this card yet. Review drafts never send a shutdown instruction.","info"));
    if(status&&status.resource==="agents.status"){
      add(panel,properties([
        ["Registered clients",valueOrDash(status.enrolled)],
        ["Connected clients",valueOrDash(status.online)],
        ["Waiting for acknowledgement",valueOrDash(status.waiting)],
        ["Acknowledged",valueOrDash(status.acknowledged)],
        ["Unreachable",valueOrDash(status.unreachable)],
        ["Warning in progress",status.warningActive?"Yes":"No"]
      ]));
    }else{
      add(panel,node("p","No live client inventory is available from this development host.","label"));
    }
    if(page==="shutdown"){
      add(panel,section("Advance warning window",
        "Choose how much warning registered systems must receive before an operator-authorized shutdown."));
      const field=element("div","agent-warning-field");
      const label=element("label","","Warning duration in seconds (30–1800)");
      const input=element("input","config-input");
      input.type="number";input.id="agent-warning-duration";label.htmlFor=input.id;
      input.min="30";input.max="1800";input.step="1";
      input.value=String(state.warningDraftSeconds);
      const buttonExport=button("Export warning-policy review",null);
      const stateText=element("span","label");
      function update(){
        const v=Number(input.value);
        if(input.value.trim()!==""&&Number.isInteger(v)&&v>=30&&v<=1800){
          state.warningDraftSeconds=v;buttonExport.disabled=false;
          stateText.textContent="Advance warning: "+v+" seconds; not activated";
        }else{buttonExport.disabled=true;stateText.textContent="Enter 30–1800 seconds.";}
      }
      input.addEventListener("input",update);
      buttonExport.addEventListener("click",()=>{
        update();if(buttonExport.disabled)return;
        saveEventFile("openh910p-shutdown-warning-review.json",JSON.stringify({
          schemaVersion:1,kind:"openh910p-shutdown-warning-review",
          applied:false,installable:false,
          warningSeconds:state.warningDraftSeconds,
          physicalPowerControl:false,
          authenticatedAgentNetworkAvailable:false
        },null,2)+"\n","application/json;charset=utf-8");
      });
      add(field,label,input,buttonExport,stateText);
      add(panel,field);update();
      return panel;
    }
    add(panel,section("Local shutdown client registration draft",
      "Define client names, IDs and heartbeat intervals. Credentials and live registrations will be provisioned separately through authenticated management."));
    const list=element("div","agent-draft-list");
    const controls=element("div","config-actions");
    const addClient=button("Add shutdown client",()=>{
      if(state.agentDraft.length>=12)return;
      let id=1;
      const used=new Set(state.agentDraft.map(x=>x.id));
      while(id<=12&&used.has(id))++id;
      state.agentDraft.push({id,label:"",leaseSeconds:60});
      draw();
    });
    const save=button("Export client registration review",null,true);
    const errors=element("div","config-errors");
    errors.setAttribute("role","status");
    errors.setAttribute("aria-live","polite");
    add(controls,addClient,save);
    add(panel,list,errors,controls);
    function validate(){
      const issues=[],ids=new Set();
      state.agentDraft.forEach((client,index)=>{
        const n=index+1;
        if(!Number.isSafeInteger(client.id)||client.id<1||client.id>4294967295)
          issues.push("Client "+n+" needs a positive numeric ID.");
        if(ids.has(client.id))issues.push("Client "+n+" duplicates an ID.");
        ids.add(client.id);
        if(typeof client.label!=="string"||client.label.length===0||
           client.label.length>31||! /^[A-Za-z0-9 _.,()/-]+$/.test(client.label)||
           client.label.trim().length===0)
          issues.push("Client "+n+" needs a valid name (up to 31 characters).");
        if(!Number.isInteger(client.leaseSeconds)||client.leaseSeconds<10||
           client.leaseSeconds>300)
          issues.push("Client "+n+" needs a heartbeat lease between 10 and 300 seconds.");
      });
      return issues;
    }
    function update(){
      const issues=validate();
      errors.replaceChildren(...issues.map(e=>node("p",e)));
      save.disabled=state.agentDraft.length===0||issues.length>0;
      addClient.disabled=state.agentDraft.length>=12;
    }
    function draw(){
      list.replaceChildren();
      state.agentDraft.forEach((client,index)=>{
        const row=element("div","agent-draft-row");
        const heading=node("strong","Client "+(index+1));
        const idInput=element("input","config-input");
        idInput.type="number";idInput.min="1";idInput.max="4294967295";idInput.step="1";
        idInput.value=String(client.id);
        idInput.setAttribute("aria-label","Client "+(index+1)+" ID");
        idInput.addEventListener("input",()=>{
          client.id=idInput.value.trim()?Number(idInput.value):NaN;update();
        });
        const name=element("input","config-input");
        name.type="text";name.maxLength=31;name.autocomplete="off";
        name.value=client.label;
        name.setAttribute("aria-label","Client "+(index+1)+" name");
        name.addEventListener("input",()=>{client.label=name.value;update();});
        const lease=element("input","config-input");
        lease.type="number";lease.min="10";lease.max="300";lease.step="1";
        lease.value=String(client.leaseSeconds);
        lease.setAttribute("aria-label","Client "+(index+1)+" heartbeat lease (seconds)");
        lease.addEventListener("input",()=>{
          client.leaseSeconds=lease.value.trim()?Number(lease.value):NaN;update();
        });
        const remove=button("Remove",()=>{state.agentDraft.splice(index,1);draw();});
        add(row,heading,idInput,name,lease,remove);list.appendChild(row);
      });
      update();
    }
    save.addEventListener("click",()=>{
      update();if(save.disabled)return;
      saveEventFile("openh910p-shutdown-clients-review.json",JSON.stringify({
        schemaVersion:1,kind:"openh910p-shutdown-clients-review",
        applied:false,installable:false,authenticated:false,
        clients:state.agentDraft.map(client=>({
          id:client.id,label:client.label,leaseSeconds:client.leaseSeconds
        }))
      },null,2)+"\n","application/json;charset=utf-8");
    });
    draw();
    return panel;
  }

  function renderSnmpAgent(policy){
    const panel=element("div","snmp-monitor");
    add(panel,section("UPS-MIB monitoring","Supported read-only SNMP object identifiers for verified UPS measurements."));
    if(!policy||policy.readOnlyAgentCompiled!==true||policy.supportedOids!==7||
       !Array.isArray(policy.oids)||policy.oids.length!==7){
      add(panel,notice("SNMP monitoring capabilities unavailable",
        "The management service has not advertised a compatible UPS-MIB mapping.","error"));
      return panel;
    }
    add(panel,notice("SNMP listener not enabled",
      "Native SNMPv2c GET/GETNEXT encoding is implemented, but access restrictions, a provisioned community and a qualified UDP listener are not connected. SNMPv3 authPriv remains required for secure remote monitoring.","info"));
    add(panel,properties([
      ["Read-only object support","Seven UPS-MIB objects"],
      ["Community access",policy.communityProvisioned?"Provisioned":"Not provisioned"],
      ["Network source restrictions",policy.sourceAclQualified?"Qualified":"Not qualified"],
      ["SNMPv3 security",policy.v3UsmAuthPrivQualified?"Qualified":"Not qualified"],
      ["Write operations",policy.setOperationsAllowed?"Allowed":"Always disabled"]
    ]));
    const rows=policy.oids.map(entry=>[entry.label,entry.oid]);
    panel.appendChild(table(["Measurement","UPS-MIB OID"],rows));
    return panel;
  }
  function renderFirmwareUpdate(policy){
    const panel=element("div","firmware-update");
    add(panel,section("Project firmware releases",
      "Release discovery is read-only and uses this project's GitHub repository, never the OEM firmware channel."));
    if(!policy||policy.repository!=="bradjtrammell/dell-nmc-firmware"||
       policy.releaseProvider!=="github"||
       policy.releaseCheckEnabled!==true ||
       policy.installEnabled!==false){
      add(panel,notice("Update source could not be verified",
        "The device did not return the pinned project release contract. No update lookup or installation will be attempted.","error"));
      return panel;
    }
    add(panel,properties([
      ["Release source","GitHub · "+policy.repository],
      ["Running development version",policy.runningVersion||"Unknown"],
      ["Hardware target",policy.hardwareTarget||"Unknown"],
      ["Native SHA-256 verification",policy.nativeSha256Compiled?"Compiled":"Unavailable"],
      ["Signed package-validation gate",policy.signedManifestGateCompiled?"Compiled":"Unavailable"],
      ["Trusted release signing keys",policy.signatureKeysQualified?"Qualified":"Not qualified"],
      ["Verified flash and recovery",policy.flashLayoutQualified&&policy.recoveryQualified?"Qualified":"Not qualified"],
      ["Firmware installation",policy.installEnabled?"Enabled":"Disabled"],
      ["Rollback",policy.rollbackEnabled?"Enabled":"Disabled"]
    ]));
    add(panel,notice("Installation intentionally blocked",
      "Release discovery cannot install firmware. Each package must pass project signing-key verification, SHA-256 image integrity, H910P compatibility, anti-rollback and recovery checks before any future device-side activation.","warning"));
    const controls=element("div","config-actions");
    const check=button("Check GitHub releases",null,false);
    const status=element("span","label","Not checked");
    const results=element("div","firmware-results");
    add(controls,check,status);
    add(panel,controls,results);
    check.addEventListener("click",async()=>{
      if(check.disabled)return;
      check.disabled=true;
      status.textContent="Checking GitHub…";
      results.replaceChildren();
      const generation=state.generation;
      try{
        /* Pinned public release endpoint. No GitHub PAT, browser credentials,
         * arbitrary user URL or Dell/OEM fallback. */
        const response=await fetch(
          "https://api.github.com/repos/bradjtrammell/dell-nmc-firmware/releases?per_page=12",
          {method:"GET",credentials:"omit",cache:"no-store",
           headers:{"Accept":"application/vnd.github+json"}});
        if(!response.ok)throw new Error("GitHub returned HTTP "+response.status);
        const releases=await response.json();
        if(!Array.isArray(releases))
          throw new Error("GitHub did not return a release list");
        if(generation!==state.generation)return;
        const accepted=releases.filter(release=>
          release&&release.draft===false&&typeof release.tag_name==="string"&&
          /^[A-Za-z0-9._-]{1,80}$/.test(release.tag_name)).slice(0,12);
        status.textContent=accepted.length+" published release(s)";
        if(!accepted.length){
          add(results,notice("No published project releases",
            "The project GitHub Releases feed currently contains no published versions. No update is available to inspect or install.","info"));
        } else {
          const rows=accepted.map(release=>{
            const link=element("a","","View "+release.tag_name);
            link.href="https://github.com/bradjtrammell/dell-nmc-firmware/releases/tag/"+
              encodeURIComponent(release.tag_name);
            link.rel="noopener noreferrer";
            link.target="_blank";
            const assets=Array.isArray(release.assets)?release.assets:[];
            const assetNames=assets.filter(a=>a&&typeof a.name==="string")
              .slice(0,8).map(a=>a.name.slice(0,100)).join(", ");
            return [link,release.prerelease?"Pre-release":"Release",
              release.published_at?String(release.published_at).slice(0,10):"Date unavailable",
              assetNames||"No downloadable assets","Not verified for installation"];
          });
          results.appendChild(table(["Version","Channel","Published","Assets","Card status"],rows));
        }
      }catch(error){
        if(generation!==state.generation)return;
        status.textContent="Release check failed";
        add(results,notice("Unable to retrieve project releases",
          String(error.message||error)+". Existing device firmware has not been changed.","error"));
      }finally{
        if(generation===state.generation)check.disabled=false;
      }
    });
    return panel;
  }

  /* Local draft workspace, separate from the native O6BK binary backup.
   * Neither representation is an installable/on-device backup. */
  function backupWorkspaceFields(){
    return [
      ["System.Network.HostName","hostname",49],
      ["System.Network.DomainName","domain",49],
      ["System.Network.DHCP","dhcp",0],
      ["System.Network.IPAddress","ipv4",0],
      ["System.Network.IPMask","netmask",0],
      ["System.Network.IPGateway","ipv4",0],
      ["System.Contact","string",49],
      ["System.Location","string",31],
      ["System.Language","language",0]
    ].map(([name,type,max])=>({name,label:name,type,max}));
  }
  function defaultShutdownDraft(){
    return {master:{duration:120,timerSelected:false,forcedReboot:false,
      timerSeconds:0,runtimeSeconds:0,capacityPercent:0,restartPercent:0},
      segments:[{duration:120,shutdownDelay:0,startupDelay:0,capacityPercent:0},
        {duration:120,shutdownDelay:0,startupDelay:0,capacityPercent:0}]};
  }
  function workspaceValidation(data){
    if(!data||typeof data!=="object"||Array.isArray(data)||
       data.schemaVersion!==1||
       data.kind!=="openh910p-local-workspace-review"||
       data.applied!==false||data.installable!==false||
       Object.keys(data).some(k=>!["schemaVersion","kind","applied","installable",
         "sections"].includes(k)))
      throw new Error("Not a supported non-installable workspace review");
    const p=data.sections;
    if(!p||typeof p!=="object"||Array.isArray(p)||
       Object.keys(p).length!==3||
       Object.keys(p).some(k=>!["configuration","weekly","shutdown"].includes(k)))
      throw new Error("Workspace sections are incomplete or unknown");
    const cfg=p.configuration;
    const allowed=backupWorkspaceFields();
    const fieldMap=new Map(allowed.map(f=>[f.name,f]));
    if(!cfg||typeof cfg!=="object"||Array.isArray(cfg))
      throw new Error("Configuration section is invalid");
    const config=Object.create(null);
    for(const [key,value] of Object.entries(cfg)){
      if(!fieldMap.has(key)||typeof value!=="string"||
         !validConfigField(fieldMap.get(key),value))
        throw new Error("Invalid or unsupported setting: "+key);
      config[key]=value;
    }
    const configErrors=configDraftErrors(allowed,config);
    if(configErrors.length)throw new Error(configErrors[0]);
    const days=p.weekly;
    if(!Array.isArray(days)||days.length!==7)
      throw new Error("Weekly schedule requires seven days");
    const weekly=days.map((day,index)=>{
      if(!day||typeof day!=="object"||Array.isArray(day)||
         Object.keys(day).length!==2||
         Object.keys(day).some(k=>!["offSeconds","onSeconds"].includes(k)))
        throw new Error("Unknown weekday fields at "+index);
      const pair={offSeconds:day.offSeconds,onSeconds:day.onSeconds};
      for(const value of [pair.offSeconds,pair.onSeconds])
        if(!Number.isInteger(value)||
           !(value===-1||(value>=0&&value<=84600&&value%1800===0)))
          throw new Error("Invalid half-hour slot at "+index);
      if(pair.offSeconds>=0&&pair.offSeconds===pair.onSeconds)
        throw new Error("Conflicting on/off slot at "+index);
      return pair;
    });
    const shutdown=p.shutdown;
    if(!shutdown||typeof shutdown!=="object"||Array.isArray(shutdown)||
       Object.keys(shutdown).length!==2 ||
       Object.keys(shutdown).some(k=>!["master","segments"].includes(k)) ||
       !shutdown.master||!Array.isArray(shutdown.segments)||
       shutdown.segments.length!==2)
      throw new Error("Shutdown policy requires all three outputs");
    const master=shutdown.master;
    const masterRanges={
      duration:[120,9999,1],timerSeconds:[0,5999940,60],
      runtimeSeconds:[0,99999,1],capacityPercent:[0,100,1],
      restartPercent:[0,100,1]
    };
    const segmentRanges={
      duration:[120,9999,1],shutdownDelay:[0,99999,1],
      startupDelay:[0,65535,1],capacityPercent:[0,100,1]
    };
    function checkRanges(object,ranges,additional=[]){
      if(!object||typeof object!=="object"||Array.isArray(object)||
         Object.keys(object).length!==Object.keys(ranges).length+additional.length||
         Object.keys(object).some(key=>!(key in ranges)&&!additional.includes(key)))
        throw new Error("Shutdown policy contains unsupported settings");
      for(const [key,[low,high,step]] of Object.entries(ranges)){
        const value=object[key];
        if(!Number.isInteger(value)||value<low||value>high||value%step!==0)
          throw new Error("Invalid shutdown setting "+key);
      }
      for(const key of additional)if(typeof object[key]!=="boolean")
        throw new Error("Invalid shutdown switch "+key);
      return {...object};
    }
    const normalizedShutdown={
      master:checkRanges(master,masterRanges,["timerSelected","forcedReboot"]),
      segments:shutdown.segments.map(entry=>checkRanges(entry,segmentRanges))
    };
    return {configuration:config,weekly,shutdown:normalizedShutdown};
  }
  function currentWorkspace(){
    const cfg=Object.fromEntries(Object.entries(state.configDraft)
      .filter(([k,v])=>typeof v==="string"&&v.length>0));
    return {schemaVersion:1,kind:"openh910p-local-workspace-review",
      applied:false,installable:false,sections:{
        configuration:cfg,
        weekly:state.weeklyDraft||Array.from({length:7},
          ()=>({offSeconds:-1,onSeconds:-1})),
        shutdown:state.shutdownDraft||defaultShutdownDraft()
      }};
  }
  function renderBackupWorkspace(capabilities){
    const panel=element("div","backup-workspace");
    add(panel,section("Configuration backup and recovery",
      "Review your local Network, System, Weekly Schedule and Shutdown Parameters drafts together."));
    if(!capabilities||capabilities.nativePortableCodecCompiled!==true||
       capabilities.restoreAvailable!==false||
       capabilities.liveDeviceBackupAvailable!==false ||
       capabilities.credentialsIncluded!==false){
      add(panel,notice("Backup capabilities unavailable",
        "The device has not advertised the non-destructive portable backup contract.","error"));
      return panel;
    }
    add(panel,notice("Not an active device backup",
      "Native binary integrity verification is implemented. This browser workspace contains only drafts entered here, not live H910P settings, accounts or passwords. Import never writes to the card.","info"));
    const actions=element("div","config-actions");
    const exportButton=button("Export local workspace",null,false);
    const importLabel=element("label","","Import local workspace JSON");
    const importInput=element("input","config-input");
    importInput.type="file";importInput.accept=".json,application/json";
    importInput.setAttribute("aria-label","Import local workspace JSON");
    const status=element("p","label",state.backupNotice||"No device backup has been applied.");
    add(importLabel,importInput);
    add(actions,exportButton,importLabel);
    add(panel,actions,status);
    exportButton.addEventListener("click",()=>{
      try{
        const candidate=currentWorkspace();
        workspaceValidation(candidate);
        saveEventFile("openh910p-local-workspace-review.json",
          JSON.stringify(candidate,null,2)+"\n","application/json;charset=utf-8");
        status.textContent="Local draft review exported. No device settings changed.";
      }catch(error){
        status.textContent="Export blocked: "+String(error.message||error);
      }
    });
    importInput.addEventListener("change",async()=>{
      const file=importInput.files&&importInput.files[0];
      if(!file)return;
      if(!Number.isInteger(file.size)||file.size<1||file.size>16384 ||
         typeof file.text!=="function"){
        status.textContent="Import blocked: file must be valid JSON under 16 KB.";
        return;
      }
      try{
        const raw=await file.text();
        if(raw.length>16384)throw new Error("File exceeds size limit");
        const normalized=workspaceValidation(JSON.parse(raw));
        /* Atomic local replacement AFTER all three models validate. */
        state.configDraft=normalized.configuration;
        state.weeklyDraft=normalized.weekly;
        state.shutdownDraft=normalized.shutdown;
        state.backupNotice="Validated workspace restored in browser memory. No card writes.";
        status.textContent=state.backupNotice;
      }catch(error){
        status.textContent="Import rejected: "+String(error.message||error);
      }
    });
    return panel;
  }

  function renderGeneral(summary,key,items,staging,security,weekly,notifyPolicy,notifyStatus,updatePolicy,snmpPolicy,agentsPolicy,agentsStatus,timePolicy,timeStatus,shutdownSchema,shutdownAdvisory,backupCapabilities) {
    const spec=pages[key];const container=element("div");
    add(container,title(spec[0],spec[1]));
    if(key==="access")container.appendChild(renderAccessSecurity(security));
    if(key==="system")container.appendChild(renderBackupWorkspace(backupCapabilities));
    if(key==="schedule")container.appendChild(renderWeeklyDraft(weekly));
    if(key==="firmware")container.appendChild(renderFirmwareUpdate(updatePolicy));
    if(key==="snmp")container.appendChild(renderSnmpAgent(snmpPolicy));
    if(key==="time")container.appendChild(renderTimeService(timePolicy,timeStatus));
    if(key==="shutdown")container.appendChild(renderShutdownPolicy(shutdownSchema,shutdownAdvisory));
    if(key==="applications"||key==="shutdown")
      container.appendChild(renderShutdownAgents(agentsPolicy,agentsStatus,key));
    if(key==="email"||key==="snmp")
      container.appendChild(renderNotificationRules(notifyPolicy,notifyStatus,key));
    if(key==="network"||key==="system")
      container.appendChild(renderConfigDraft(staging,key));
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
      else if(page==="measurements"){
        let history=null;
        try{
          history=await loadMeasurementHistory(signal);
        }catch(error){
          if(error.httpStatus!==503)throw error;
        }
        if(signal.aborted||generation!==state.generation)return;
        content=renderMeasurements(summary,history);
      }
      else if(page==="control")content=renderControl(summary);
      else if(page==="events"||page==="system-log"){
        let history=null;
        try{
          history=await loadEventHistory(signal);
        }catch(error){if(error.httpStatus===503) history=null;else throw error;}
        if(signal.aborted||generation!==state.generation)return;
        content=renderEvents(summary,history,page);
      } else {
        let fields=null;
        if(page!=="firmware"&&page!=="applications"){
          try{fields=await loadFields(signal);}catch(error){
            if(signal.aborted||generation!==state.generation)return;
            fields=null;
          }
        }
        let staging=null;
        if(page==="network"||page==="system"){
          try{staging=await request("configuration/staging",signal);}
          catch(error){if(error.httpStatus!==404)throw error;}
        }
        let security=null;
        if(page==="access"){
          try{security=await request("security/capabilities",signal);}
          catch(error){if(error.httpStatus!==404)throw error;}
        }
        let weekly=null;
        if(page==="schedule"){
          try{weekly=await request("schedule/schema",signal);}
          catch(error){if(error.httpStatus!==404)throw error;}
        }
        let notifyPolicy=null,notifyStatus=null;
        if(page==="email"||page==="snmp"){
          try{notifyPolicy=await request("notifications/policy",signal);}
          catch(error){if(error.httpStatus!==404)throw error;}
          try{notifyStatus=await request("notifications/status",signal);}
          catch(error){if(error.httpStatus!==503)throw error;}
        }
        let updatePolicy=null;
        if(page==="firmware"){
          try{updatePolicy=await request("firmware/update-policy",signal);}
          catch(error){if(error.httpStatus!==404)throw error;}
        }
        let snmpPolicy=null;
        if(page==="snmp"){
          try{snmpPolicy=await request("snmp/capabilities",signal);}
          catch(error){if(error.httpStatus!==404)throw error;}
        }
        let agentsPolicy=null,agentsStatus=null;
        if(page==="applications"||page==="shutdown"){
          try{agentsPolicy=await request("agents/capabilities",signal);}
          catch(error){if(error.httpStatus!==404)throw error;}
          try{agentsStatus=await request("agents/status",signal);}
          catch(error){if(error.httpStatus!==503)throw error;}
        }
        let timePolicy=null,timeStatus=null;
        if(page==="time"){
          try{timePolicy=await request("time/capabilities",signal);}
          catch(error){if(error.httpStatus!==404)throw error;}
          try{timeStatus=await request("time/status",signal);}
          catch(error){if(error.httpStatus!==503)throw error;}
        }
        let shutdownSchema=null,shutdownAdvisory=null;
        if(page==="shutdown"){
          try{shutdownSchema=await request("shutdown/schema",signal);}
          catch(error){if(error.httpStatus!==404)throw error;}
          try{shutdownAdvisory=await request("shutdown/advisory",signal);}
          catch(error){if(error.httpStatus!==503)throw error;}
        }
        let backupCapabilities=null;
        if(page==="system"){
          try{backupCapabilities=await request("backup/capabilities",signal);}
          catch(error){if(error.httpStatus!==404)throw error;}
        }
        if(signal.aborted||generation!==state.generation)return;
        content=renderGeneral(summary,page,fields,staging,security,weekly,notifyPolicy,notifyStatus,updatePolicy,snmpPolicy,agentsPolicy,agentsStatus,timePolicy,timeStatus,shutdownSchema,shutdownAdvisory,backupCapabilities);
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
      state.eventPage=0;state.eventSeverity="all";state.eventQuery="";state.search="";
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