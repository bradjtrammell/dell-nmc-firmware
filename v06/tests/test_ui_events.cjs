"use strict";
/* Dependency-free Node 22 browser-behavior test. No NMC, flash, or network.
 * Exercises the real production app.js against deterministic DOM/API fakes.
 * Does not claim visual or on-card browser acceptance. */
const assert=require("node:assert/strict");
const {readFileSync}=require("node:fs");
const path=require("node:path");
const vm=require("node:vm");
const {test}=require("node:test");

class Element {
  constructor(tag) {
    this.tagName=tag.toLowerCase();
    this.nodeType=1;
    this.children=[];
    this.events={};
    this.dataset={};
    this.attributes={};
    this.className="";
    this.textContent="";
    this.disabled=false;
    this.value="";
    this.type="";
    this.style={};
    this.parent=null;
    const names=new Set();
    this.classList={
      add(name){names.add(name);},
      remove(name){names.delete(name);},
      contains(name){return names.has(name);},
      toggle(name){if(names.has(name)){names.delete(name);return false;}
        names.add(name);return true;}
    };
  }
  appendChild(node){node.parent=this;this.children.push(node);return node;}
  replaceChildren(...nodes){this.children.forEach(n=>n.parent=null);this.children=[];
    nodes.forEach(n=>this.appendChild(n));}
  setAttribute(name,value){this.attributes[name]=String(value);}
  getAttribute(name){return this.attributes[name]??null;}
  removeAttribute(name){delete this.attributes[name];}
  addEventListener(name,fn){(this.events[name]??=[]).push(fn);}
  dispatch(name){(this.events[name]||[]).forEach(fn=>fn({target:this}));}
  click(){this.dispatch("click");}
  focus(){this.focused=true;}
  remove(){if(this.parent){const p=this.parent;
    p.children=p.children.filter(n=>n!==this);this.parent=null;}}
  querySelectorAll(selector){
    const result=[];function descend(element){
      element.children.forEach(child=>{
        if(child.tagName===selector.toLowerCase())result.push(child);
        descend(child);
      });
    }descend(this);return result;
  }
}
function descendants(root){
  const arr=[root];for(const child of root.children)arr.push(...descendants(child));
  return arr;
}
function allText(root) {
  return descendants(root).map(n=>n.textContent).filter(Boolean).join(" | ");
}
function find(root,tag,matching){
  return descendants(root).find(n=>n.tagName===tag&&matching(n));
}
function createEnvironment({inconsistent=false,historyInconsistent=false,historyMissing=false,githubReleases=[],githubError=false}={}){
  const names=["navigation","view","banner","refresh","health-indicator",
    "sidebar","menu","device-status","breadcrumb-name"];
  const named=Object.fromEntries(names.map(name=>[name,new Element("div")]));
  named.view.id="view";
  named.navigation.id="navigation";
  named.refresh.tagName="button";
  named.menu.tagName="button";
  const document={
    body:new Element("body"),hidden:false,
    createElement(tag){return new Element(tag);},
    getElementById(name){assert.ok(named[name],"unknown DOM id "+name);return named[name];},
    addEventListener(name,fn){(this.events??={})[name]=fn;}
  };
  const location={hash:"#events"};
  const listeners={};
  const downloads=[],blobs=[],calls=[];
  const window={
    addEventListener(name,fn){listeners[name]=fn;},
    setInterval(){return 1;},
    setTimeout(){return 1;}
  };
  const urlApi={
    createObjectURL(blob){blobs.push(blob);return "blob:fake-"+blobs.length;},
    revokeObjectURL(){}
  };
  document.body.appendChild=function(child){
    Element.prototype.appendChild.call(this,child);
    if(child.tagName==="a"){
      child.click=function(){downloads.push({filename:child.download,blob:blobs.at(-1)});};
    }
    return child;
  };
  const events=[
    {id:20,bootGeneration:2,monotonicMs:10000,severity:"info",code:"SERVICE_START",message:"Service initialized"},
    {id:19,bootGeneration:2,monotonicMs:9000,severity:"error",code:"TELEMETRY_OFFLINE",message:"UPS data unavailable"},
    {id:18,bootGeneration:2,monotonicMs:8000,severity:"info",code:"TELEMETRY_ONLINE",message:"Measurements available"},
    {id:17,bootGeneration:2,monotonicMs:7000,severity:"info",code:"UTILITY_RESTORED",message:"Power returned"},
    {id:16,bootGeneration:2,monotonicMs:6000,severity:"warning",code:"BATTERY_LOW",message:"Low battery"},
    {id:15,bootGeneration:2,monotonicMs:5000,severity:"info",code:"BATTERY_RECOVERED",message:"Battery recovered"},
    {id:14,bootGeneration:2,monotonicMs:4000,severity:"error",code:"REPORT_REJECTED",message:"Checksum mismatch"},
    {id:13,bootGeneration:1,monotonicMs:3000,severity:"info",code:"SERVICE_START",message:"Previous boot"},
    {id:12,bootGeneration:1,monotonicMs:2000,severity:"warning",code:"UTILITY_LOST",message:"Utility lost before restart"}
  ];
  const respond=(route)=>{
    if(route==="health")return {schemaVersion:1,source:"native",simulated:false,status:"ok"};
    if(route==="device")return {schemaVersion:1,source:"native",simulated:false,firmwareVersion:"0.6.0-alpha.1"};
    if(route==="capabilities")return {schemaVersion:1,source:"native",simulated:false,
      outletControl:false,configurationWritable:false,firmwareInstall:false,transport:"native-json-c"};
    if(route==="ups/status"||route==="ups/measurements"||route==="ups/outlets")
      return {error:"TELEMETRY_UNAVAILABLE",status:503};
    if(route==="ups/history"||route==="ups/history/1") {
      if(historyMissing)return {error:"HISTORY_UNAVAILABLE",status:503};
      const history=Array.from({length:9},(_,i)=>({
        sequence:20-i,monotonicMs:90000-i*10000,
        batteryPercent:90-i,loadPercent:25+i,runtimeSeconds:2000-i*20,
        inputDecivolts:1190,outputDecivolts:1200,
        acPresent:i!==4,gapBefore:i===4
      }));
      return {schemaVersion:1,source:"native",simulated:false,
        page:route==="ups/history"?0:1,pageSize:8,pages:2,
        capacity:64,stored:9,overwritten:3,persistent:false,live:false,
        latestRecordSequence:historyInconsistent&&route==="ups/history/1"?21:20,
        sampleSource:"verified-native-only",
        samples:route==="ups/history"?history.slice(0,8):history.slice(8)};
    }
    if(route==="shutdown/schema")return {
      schemaVersion:1,source:"native",simulated:false,
      policyModelCompiled:true,policyWritable:false,
      activePolicyAvailable:false,outputControlQualified:false,
      outlets:3,minimumShutdownDurationSeconds:120,
      maximumShutdownDurationSeconds:9999,
      maximumMasterTimerSeconds:5999940,masterTimerStepSeconds:60,
      maximumRuntimeThresholdSeconds:99999,
      maximumOutletTimerSeconds:99999,
      maximumOutletStartupSeconds:65535,maximumCapacityPercent:100,
      mode:"local-review-only"
    };
    if(route==="shutdown/advisory")return {
      schemaVersion:1,status:503,code:"SHUTDOWN_POLICY_OR_TELEMETRY_UNAVAILABLE"
    };
    if(route==="time/capabilities")return {
      schemaVersion:1,source:"native",simulated:false,
      ntpWireClientCompiled:true,trustedLocalNtpSourceConfigured:false,
      secureNonceProviderQualified:false,monotonicTimerQualified:false,
      ntpSourceAclQualified:false,networkTimeSecurityQualified:false,
      rtcWritable:false,timezoneConfigured:false,dstPolicyQualified:false,
      maximumResponseDelayMs:5000,maximumSampleAgeSeconds:3600,
      timeBasis:"UTC",mode:"trusted-lan-ntp-bootstrap-only"
    };
    if(route==="time/status")return {
      schemaVersion:1,status:503,code:"TIME_NOT_SYNCHRONIZED"
    };
    if(route==="agents/capabilities")return {
      schemaVersion:1,source:"native",simulated:false,
      coordinatorCompiled:true,registrationWritable:false,
      notificationTransportQualified:false,mutualTlsQualified:false,
      trustedShutdownIntentAvailable:false,physicalPowerControl:false,
      maximumClients:12,minimumLeaseSeconds:10,maximumLeaseSeconds:300,
      minimumWarningSeconds:30,maximumWarningSeconds:1800,
      mode:"local-review-only"
    };
    if(route==="agents/status")return {
      schemaVersion:1,source:"native",simulated:false,
      status:503,error:"AGENT_COORDINATOR_UNAVAILABLE"
    };
    if(route==="snmp/capabilities")return {
      schemaVersion:1,source:"native",simulated:false,
      readOnlyAgentCompiled:true,listenerEnabled:false,
      v2cPacketCodec:true,v3UsmAuthPrivQualified:false,
      communityProvisioned:false,sourceAclQualified:false,
      verifiedNativeTelemetryRequired:true,setOperationsAllowed:false,
      maximumVarbinds:8,supportedOids:7,
      oids:[
        ["1.3.6.1.2.1.33.1.2.1.0","Battery status"],
        ["1.3.6.1.2.1.33.1.2.3.0","Runtime remaining (minutes)"],
        ["1.3.6.1.2.1.33.1.2.4.0","Battery charge (%)"],
        ["1.3.6.1.2.1.33.1.3.3.1.3.1","Input voltage (V)"],
        ["1.3.6.1.2.1.33.1.4.1.0","Output power source"],
        ["1.3.6.1.2.1.33.1.4.4.1.2.1","Output voltage (V)"],
        ["1.3.6.1.2.1.33.1.4.4.1.5.1","Output load (%)"]
      ].map(([oid,label])=>({oid,label}))
    };
    if(route==="backup/capabilities")return {
      schemaVersion:1,resource:"backup.capabilities",source:"native",
      nativePortableCodecCompiled:true,readOnlyPreviewAvailable:true,
      liveDeviceBackupAvailable:false,restoreAvailable:false,
      multiPartitionCommitQualified:false,credentialsIncluded:false,
      secretsIncluded:false,checksumIsSignature:false,
      formatVersion:1,maximumBytes:1536,
      sections:["configuration","shutdown","weekly"],
      mode:"local-workspace-export-import-only"
    };
    if(route==="firmware/update-policy")return {
      schemaVersion:1,source:"native",simulated:false,
      releaseProvider:"github",repository:"bradjtrammell/dell-nmc-firmware",
      runningVersion:"0.6.0-alpha.1",hardwareTarget:"H910P",
      releaseCheckEnabled:true,nativeSha256Compiled:true,
      signedManifestGateCompiled:true,signatureKeysQualified:false,
      flashLayoutQualified:false,recoveryQualified:false,
      deviceHttpsQualified:false,installEnabled:false,
      rollbackEnabled:false,automaticUpdateEnabled:false,
      mode:"release-check-only"
    };
    if(route==="notifications/policy")return {
      schemaVersion:1,source:"native",simulated:false,
      routerCompiled:true,deliveryAvailable:false,rulesWritable:false,
      destinationsProvisioned:false,transportQualified:false,
      credentialsConfigured:false,maximumRules:8,queueCapacity:24,
      maximumAttempts:5,maximumCooldownSeconds:3600,
      mode:"local-draft-export-only",
      transports:["email","webhook","snmp-trap"],
      events:["SERVICE_START","TELEMETRY_ONLINE","TELEMETRY_OFFLINE",
        "REPORT_REJECTED","UTILITY_LOST","UTILITY_RESTORED",
        "BATTERY_LOW","BATTERY_RECOVERED"]
    };
    if(route==="notifications/status")return {
      error:"NOTIFICATION_ENGINE_UNAVAILABLE",status:503
    };
    if(route==="schedule/schema")return {
      schemaVersion:1,source:"native",simulated:false,
      weeklyPlannerCompiled:true,scheduleWritable:false,
      activeScheduleAvailable:false,rtcQualified:false,
      powerControlQualified:false,persistent:false,
      dayCount:7,slotSeconds:1800,slotsPerDay:48,
      disabledValue:"-1",mode:"local-draft-export-only"
    };
    if(route==="security/capabilities")return {
      schemaVersion:1,source:"native",simulated:false,
      sessionEngineCompiled:true,authenticationAvailable:false,
      loginEnabled:false,tlsQualified:false,entropyQualified:false,
      credentialStoreQualified:false,mutationsAuthorized:false,
      maximumSessions:4,idleTimeoutSeconds:900,
      absoluteTimeoutSeconds:28800,
      maximumFailedAttempts:5,lockoutSeconds:60,
      csrfRequiredForWrites:true,increasingRequestIdRequired:true,
      status:"provider-unqualified"
    };
    if(route==="configuration/fields")
      return {schemaVersion:1,source:"native",simulated:false,pages:1,total:0};
    if(route==="configuration/fields/0")
      return {schemaVersion:1,source:"native",simulated:false,items:[]};
    if(route==="configuration/staging"){
      const keys=[
        ["System.Network.HostName","Host name","hostname",49],
        ["System.Network.DomainName","Domain name","domain",49],
        ["System.Network.DHCP","IPv4 assignment","dhcp",0],
        ["System.Network.IPAddress","Static IPv4 address","ipv4",0],
        ["System.Network.IPMask","IPv4 subnet mask","netmask",0],
        ["System.Network.IPGateway","IPv4 default gateway","ipv4",0],
        ["System.Contact","System contact","string",49],
        ["System.Location","Physical location","string",31],
        ["System.Language","Interface language","language",0]
      ];
      return {schemaVersion:1,source:"native",simulated:false,
        configurationWritable:false,authorizationAvailable:false,
        activeValuesAvailable:false,storageQualified:false,supportedFields:9,
        fields:keys.map(([name,label,type,max])=>({name,label,type,max}))};
    }
    if(route==="events/stats")return {schemaVersion:1,source:"native",simulated:false,
      stored:9,info:6,warning:2,error:2,latestEventId:20,
      currentBootGeneration:2,byCode:{SERVICE_START:2}};
    if(route==="events"||route==="events/1")return {
      schemaVersion:1,source:"native",simulated:false,
      persistent:false,stored:9,pages:2,pageSize:8,overwritten:3,
      page:route==="events"?0:1,currentBootGeneration:2,
      latestEventId:inconsistent&&route==="events/1"?21:20,
      events:route==="events"?events.slice(0,8):events.slice(8)
    };
    throw Error("Unexpected API route "+route);
  };
  async function fetch(url,opts){
    if(String(url).startsWith("https://api.github.com/")){
      assert.equal(String(url),"https://api.github.com/repos/bradjtrammell/dell-nmc-firmware/releases?per_page=12");
      assert.equal(opts.credentials,"omit");
      assert.equal(opts.method,"GET");
      calls.push("github-releases");
      return {ok:!githubError,status:githubError?403:200,
        async json(){return githubReleases;}};
    }
    const route=String(url).replace(/^\/api\/v1\//,"");
    calls.push(route);
    assert.equal(opts.method,"GET");
    assert.equal(opts.credentials,"same-origin");
    const info=respond(route);
    const httpStatus=typeof info.status==="number"?info.status:200;
    return {ok:httpStatus>=200&&httpStatus<300,status:httpStatus,
      async json(){return info;}};
  }
  const source=readFileSync(path.join(__dirname,"..","web","app.js"),"utf8");
  vm.runInNewContext(source,{
    document,location,window,fetch,
    AbortController,URL:urlApi,Blob,console
  },{filename:"v06/web/app.js",timeout:1000});
  return {named,document,location,listeners,calls,downloads,events};
}
async function settled(env){
  for(let i=0;i<20;i++){
    await new Promise(resolve=>setImmediate(resolve));
    if(!allText(env.named.view).includes("Loading"))break;
  }
  await new Promise(resolve=>setImmediate(resolve));
}
test("all management destinations and complete event history render",async()=>{
  const env=createEnvironment();
  await settled(env);
  const nav=env.named.navigation.querySelectorAll("a");
  assert.equal(nav.length,15);
  assert.equal(env.calls.filter(x=>x==="events").length,1);
  assert.ok(env.calls.includes("events/1"));
  assert.ok(env.calls.includes("events/stats"));
  assert.match(allText(env.named.view),/Retained records/);
  assert.match(allText(env.named.view),/9 matching/);
  assert.match(allText(env.named.view),/Stored|Retained/);
  assert.match(allText(env.named.view),/TELEMETRY_OFFLINE/);
  assert.equal(find(env.named.view,"button",n=>n.textContent==="Export CSV").disabled,false);
});
test("filter searches all pages and exports only matching rows",async()=>{
  const env=createEnvironment();
  await settled(env);
  const select=find(env.named.view,"select",n=>n.className.includes("event-select"));
  assert.ok(select);
  select.value="warning";select.dispatch("change");
  assert.match(allText(env.named.view),/2 matching/);
  assert.match(allText(env.named.view),/BATTERY_LOW/);
  assert.match(allText(env.named.view),/UTILITY_LOST/);
  const input=find(env.named.view,"input",n=>n.className.includes("event-search"));
  input.value="utility";input.dispatch("input");
  assert.match(allText(env.named.view),/1 matching/);
  const exportCsv=find(env.named.view,"button",n=>n.textContent==="Export CSV");
  exportCsv.click();
  assert.equal(env.downloads.length,1);
  assert.match(env.downloads[0].filename,/event-log-boot2\.csv$/);
  const content=await env.downloads[0].blob.text();
  assert.match(content,/UTILITY_LOST/);
  assert.doesNotMatch(content,/BATTERY_LOW/);
  const exportJson=find(env.named.view,"button",n=>n.textContent==="Export JSON");
  exportJson.click();
  const json=JSON.parse(await env.downloads[1].blob.text());
  assert.equal(json.events.length,1);
  assert.equal(json.events[0].id,12);
  assert.equal(json.exportedRecords,1);
  assert.equal(json.overwritten,3);
  input.value="no-match-value";input.dispatch("input");
  assert.equal(exportCsv.disabled,true);
  assert.equal(exportJson.disabled,true);
});
test("event pages changing during fetch fail safely without exporting",async()=>{
  const env=createEnvironment({inconsistent:true});
  await settled(env);
  assert.match(allText(env.named.view),/history changed|Unable to load/i);
  assert.equal(env.calls.filter(x=>x==="events").length,2);
  assert.equal(env.downloads.length,0);
});
test("system log filters across all event pages",async()=>{
  const env=createEnvironment();
  await settled(env);
  env.location.hash="#system-log";
  env.listeners.hashchange();
  await settled(env);
  assert.match(allText(env.named.view),/System Log/);
  assert.match(allText(env.named.view),/TELEMETRY_OFFLINE/);
  assert.doesNotMatch(allText(env.named.view),/BATTERY_LOW/);
});

test("Measurements shows retained native readings even with live UPS offline",async()=>{
  const env=createEnvironment();
  await settled(env);
  env.location.hash="#measurements";
  env.listeners.hashchange();
  await settled(env);
  assert.ok(env.calls.includes("ups/history"));
  assert.ok(env.calls.includes("ups/history/1"));
  assert.match(allText(env.named.view),/UPS history available while offline/);
  assert.match(allText(env.named.view),/Recorded samples/);
  assert.match(allText(env.named.view),/9/);
  assert.ok(find(env.named.view,"div",n=>n.className==="history-chart"));
  const csv=find(env.named.view,"button",n=>n.textContent==="Export measurements CSV");
  assert.ok(csv&&!csv.disabled);
  csv.click();
  assert.equal(env.downloads.length,1);
  const data=await env.downloads[0].blob.text();
  assert.match(data,/sequence,?/);
  assert.match(data,/gapBefore/);
  assert.match(data,/20/);
  const json=find(env.named.view,"button",n=>n.textContent==="Export measurements JSON");
  json.click();
  const record=JSON.parse(await env.downloads[1].blob.text());
  assert.equal(record.samples.length,9);
  assert.equal(record.persistent,false);
  assert.equal(record.live,false);
  assert.equal(record.samples[4].gapBefore,true);
});
test("History snapshot conflict never silently exports torn readings",async()=>{
  const env=createEnvironment({historyInconsistent:true});
  await settled(env);
  env.location.hash="#measurements";
  env.listeners.hashchange();
  await settled(env);
  assert.match(allText(env.named.view),/History changed while reading/);
  assert.equal(env.calls.filter(x=>x==="ups/history").length,2);
  assert.equal(env.downloads.length,0);
});
test("Unavailable native history does not fabricate a trend",async()=>{
  const env=createEnvironment({historyMissing:true});
  await settled(env);
  env.location.hash="#measurements";
  env.listeners.hashchange();
  await settled(env);
  assert.match(allText(env.named.view),/No native measurement history/);
  assert.equal(find(env.named.view,"div",n=>n.className==="history-chart"),undefined);
  assert.equal(env.downloads.length,0);
});

test("local network draft validates static IPv4 and exports without any HTTP writes",async()=>{
  const env=createEnvironment();
  await settled(env);
  env.location.hash="#network";
  env.listeners.hashchange();
  await settled(env);
  assert.ok(env.calls.includes("configuration/staging"));
  assert.match(allText(env.named.view),/Local configuration draft/);
  const inputs=descendants(env.named.view);
  const set=(name,value)=>{
    const field=inputs.find(n=>n.id==="stage-"+name.replace(/[^a-zA-Z0-9]/g,"-"));
    assert.ok(field,"Missing form control "+name);
    field.value=value;
    field.dispatch("change");
  };
  const exportButton=find(env.named.view,"button",n=>n.textContent==="Export validated review draft");
  assert.ok(exportButton&&exportButton.disabled);
  set("System.Network.DHCP","false");
  assert.equal(exportButton.disabled,true);
  assert.match(allText(env.named.view),/requires an IP address/);
  set("System.Network.IPAddress","192.168.10.24");
  set("System.Network.IPMask","255.255.255.0");
  assert.equal(exportButton.disabled,false);
  set("System.Network.IPGateway","192.168.20.1");
  assert.equal(exportButton.disabled,true);
  assert.match(allText(env.named.view),/same subnet/);
  set("System.Network.IPGateway","192.168.10.1");
  set("System.Network.HostName","h910p-lab");
  assert.equal(exportButton.disabled,false);
  exportButton.click();
  assert.equal(env.downloads.length,1);
  assert.equal(env.downloads[0].filename,"openh910p-configuration-review.json");
  const config=JSON.parse(await env.downloads[0].blob.text());
  assert.equal(config.applied,false);
  assert.equal(config.installable,false);
  assert.equal(config.fields["System.Network.IPAddress"],"192.168.10.24");
  assert.equal(config.fields["System.Network.IPGateway"],"192.168.10.1");
  assert.ok(env.calls.every(route=>typeof route==="string"));
  assert.equal(env.downloads.length,1);
});
test("System identity drafts reject control characters and preserve other pages",async()=>{
  const env=createEnvironment();
  await settled(env);
  env.location.hash="#system";
  env.listeners.hashchange();
  await settled(env);
  const input=find(env.named.view,"input",n=>n.id==="stage-System-Contact");
  const button=find(env.named.view,"button",n=>n.textContent==="Export validated review draft");
  assert.ok(input&&button);
  input.value="Invalid\\nContact".replace("\\n","\n");
  input.dispatch("input");
  assert.equal(button.disabled,true);
  input.value="Rack manager";input.dispatch("input");
  assert.equal(button.disabled,false);
  button.click();
  const result=JSON.parse(await env.downloads[0].blob.text());
  assert.deepEqual(Object.keys(result.fields),["System.Contact"]);
  assert.equal(result.fields["System.Contact"],"Rack manager");
  env.location.hash="#network";env.listeners.hashchange();
  await settled(env);
  assert.match(allText(env.named.view),/Local configuration draft/);
  assert.equal(env.downloads.length,1);
});

test("Access Control shows real security-provider state, not a fake login",async()=>{
  const env=createEnvironment();
  await settled(env);
  env.location.hash="#access";
  env.listeners.hashchange();
  await settled(env);
  assert.ok(env.calls.includes("security/capabilities"));
  const text=allText(env.named.view);
  assert.match(text,/Authentication and session enforcement/);
  assert.match(text,/Account login/);
  assert.match(text,/Disabled/);
  assert.match(text,/Qualified TLS/);
  assert.match(text,/Not qualified/);
  assert.match(text,/CSRF verification for mutations/);
  assert.match(text,/No default user is created/);
  assert.equal(find(env.named.view,"input",n=>n.type==="password"),undefined);
});

test("Weekly Schedule enforces seven days, 30-minute slots and safe draft export",async()=>{
  const env=createEnvironment();
  await settled(env);
  env.location.hash="#schedule";
  env.listeners.hashchange();
  await settled(env);
  assert.ok(env.calls.includes("schedule/schema"));
  assert.match(allText(env.named.view),/Weekly power schedule draft/);
  assert.match(allText(env.named.view),/Schedule is not active/);
  const all=descendants(env.named.view);
  const selects=all.filter(x=>x.tagName==="select"&&x.id&&x.id.startsWith("schedule-"));
  assert.equal(selects.length,14);
  const off=selects.find(x=>x.id==="schedule-0-offSeconds");
  const on=selects.find(x=>x.id==="schedule-0-onSeconds");
  assert.ok(off&&on);
  const exportButton=find(env.named.view,"button",
    n=>n.textContent==="Export weekly schedule review");
  assert.ok(exportButton&&exportButton.disabled);
  off.value="28800";off.dispatch("change");
  assert.equal(exportButton.disabled,false);
  on.value="28800";on.dispatch("change");
  assert.equal(exportButton.disabled,true);
  assert.match(allText(env.named.view),/cannot turn power off and on at the same time/);
  on.value="64800";on.dispatch("change");
  assert.equal(exportButton.disabled,false);
  exportButton.click();
  assert.equal(env.downloads.length,1);
  assert.equal(env.downloads[0].filename,"openh910p-weekly-schedule-review.json");
  const json=JSON.parse(await env.downloads[0].blob.text());
  assert.equal(json.applied,false);
  assert.equal(json.installable,false);
  assert.equal(json.slotSeconds,1800);
  assert.equal(json.days.length,7);
  assert.equal(json.days[0].offSeconds,28800);
  assert.equal(json.days[0].onSeconds,64800);
  assert.equal(json.days[1].offSeconds,-1);
  assert.equal(env.calls.filter(r=>r==="schedule/schema").length,1);
  env.location.hash="#events";
  env.listeners.hashchange();
  await settled(env);
  assert.match(allText(env.named.view),/Event Log/);
  env.location.hash="#schedule";
  env.listeners.hashchange();
  await settled(env);
  const storedOff=find(env.named.view,"select",x=>x.id==="schedule-0-offSeconds");
  assert.equal(storedOff.value,"28800");
});

test("Notification rules editor validates unique event/destination and exports review only",async()=>{
  const env=createEnvironment();
  await settled(env);
  env.location.hash="#email";env.listeners.hashchange();
  await settled(env);
  assert.ok(env.calls.includes("notifications/policy"));
  assert.ok(env.calls.includes("notifications/status"));
  assert.match(allText(env.named.view),/Event notification routing/);
  assert.match(allText(env.named.view),/Delivery is not connected/);
  const add=find(env.named.view,"button",n=>n.textContent==="Add rule");
  const save=find(env.named.view,"button",n=>n.textContent==="Export routing review");
  assert.ok(add&&save&&save.disabled);
  add.click();
  assert.equal(save.disabled,false);
  add.click();
  assert.equal(save.disabled,true);
  assert.match(allText(env.named.view),/duplicates another destination/);
  const selects=descendants(env.named.view).filter(n=>n.tagName==="select"&&
      String(n.attributes["aria-label"]||"").includes("Rule 2 event"));
  assert.equal(selects.length,1);
  selects[0].value="BATTERY_LOW";selects[0].dispatch("change");
  assert.equal(save.disabled,false);
  save.click();
  assert.equal(env.downloads.length,1);
  const data=JSON.parse(await env.downloads[0].blob.text());
  assert.equal(data.applied,false);
  assert.equal(data.installable,false);
  assert.equal(data.deliveryEnabled,false);
  assert.equal(data.destinationsProvisioned,false);
  assert.equal(data.rules.length,2);
  assert.equal(data.rules[0].event,"UTILITY_LOST");
  assert.equal(data.rules[1].event,"BATTERY_LOW");
  assert.equal(data.rules[0].transport,"email");
  assert.equal(data.rules[0].destinationSlot,0);
  env.location.hash="#snmp";env.listeners.hashchange();
  await settled(env);
  assert.match(allText(env.named.view),/Event notification routing/);
  assert.equal(env.downloads.length,1);
});

test("Firmware page checks only project GitHub releases and handles empty feed",async()=>{
  const env=createEnvironment();
  await settled(env);
  env.location.hash="#firmware";
  env.listeners.hashchange();
  await settled(env);
  assert.ok(env.calls.includes("firmware/update-policy"));
  assert.match(allText(env.named.view),/Project firmware releases/);
  assert.match(allText(env.named.view),/Signing|signing/);
  const button=find(env.named.view,"button",n=>n.textContent==="Check GitHub releases");
  assert.ok(button&&!button.disabled);
  button.click();
  await settled(env);
  assert.ok(env.calls.includes("github-releases"));
  assert.match(allText(env.named.view),/No published project releases/);
  assert.match(allText(env.named.view),/Installation intentionally blocked/);
  assert.equal(env.downloads.length,0);
});
test("Project release results are text-only and never permit installation",async()=>{
  const env=createEnvironment({githubReleases:[
    {tag_name:"v0.6.1",draft:false,prerelease:true,
     published_at:"2026-11-13T16:25:00Z",assets:[{name:"h910p-research.bin"}]},
    {tag_name:"evil<script>",draft:false,prerelease:false,
     assets:[{name:"ignore-this"}]}
  ]});
  await settled(env);
  env.location.hash="#firmware";env.listeners.hashchange();
  await settled(env);
  const button=find(env.named.view,"button",n=>n.textContent==="Check GitHub releases");
  button.click();await settled(env);
  assert.match(allText(env.named.view),/View v0.6.1/);
  assert.match(allText(env.named.view),/Not verified for installation/);
  assert.doesNotMatch(allText(env.named.view),/evil<script>/);
  const link=find(env.named.view,"a",n=>n.textContent==="View v0.6.1");
  assert.ok(link);
  assert.equal(link.href,"https://github.com/bradjtrammell/dell-nmc-firmware/releases/tag/v0.6.1");
  assert.equal(link.rel,"noopener noreferrer");
  assert.equal(env.downloads.length,0);
});
test("GitHub release check fails visibly and leaves installed firmware unchanged",async()=>{
  const env=createEnvironment({githubError:true});
  await settled(env);
  env.location.hash="#firmware";env.listeners.hashchange();
  await settled(env);
  const button=find(env.named.view,"button",n=>n.textContent==="Check GitHub releases");
  button.click();await settled(env);
  assert.match(allText(env.named.view),/Release check failed/);
  assert.match(allText(env.named.view),/GitHub returned HTTP 403/);
  assert.equal(button.disabled,false);
  assert.equal(env.downloads.length,0);
});

test("SNMP monitoring page shows exact supported OIDs and disabled listener",async()=>{
  const env=createEnvironment();
  await settled(env);
  env.location.hash="#snmp";env.listeners.hashchange();
  await settled(env);
  assert.ok(env.calls.includes("snmp/capabilities"));
  const view=allText(env.named.view);
  assert.match(view,/UPS-MIB monitoring/);
  assert.match(view,/SNMP listener not enabled/);
  assert.match(view,/1\.3\.6\.1\.2\.1\.33\.1\.4\.4\.1\.5\.1/);
  assert.match(view,/Not provisioned/);
  assert.match(view,/Not qualified/);
  assert.match(view,/Event notification routing/);
});

test("Notified Applications provides validated registration review without sending",async()=>{
  const env=createEnvironment();
  await settled(env);
  env.location.hash="#applications";env.listeners.hashchange();
  await settled(env);
  assert.ok(env.calls.includes("agents/capabilities"));
  assert.ok(env.calls.includes("agents/status"));
  assert.match(allText(env.named.view),/Shutdown client coordination/);
  assert.match(allText(env.named.view),/No live client inventory/);
  const add=find(env.named.view,"button",n=>n.textContent==="Add shutdown client");
  const save=find(env.named.view,"button",n=>n.textContent==="Export client registration review");
  assert.ok(add&&save&&save.disabled);
  add.click();
  assert.equal(save.disabled,true);
  const name=find(env.named.view,"input",n=>n.attributes["aria-label"]==="Client 1 name");
  assert.ok(name);
  name.value="Media server";name.dispatch("input");
  assert.equal(save.disabled,false);
  save.click();
  assert.equal(env.downloads.length,1);
  const artifact=JSON.parse(await env.downloads[0].blob.text());
  assert.equal(artifact.applied,false);
  assert.equal(artifact.authenticated,false);
  assert.equal(artifact.installable,false);
  assert.equal(artifact.clients.length,1);
  assert.equal(artifact.clients[0].label,"Media server");
  assert.equal(artifact.clients[0].leaseSeconds,60);
  env.location.hash="#shutdown";env.listeners.hashchange();
  await settled(env);
  assert.match(allText(env.named.view),/Shutdown warning coordination/);
  const warning=find(env.named.view,"input",n=>n.id==="agent-warning-duration");
  const exportWarning=find(env.named.view,"button",n=>n.textContent==="Export warning-policy review");
  assert.ok(warning&&exportWarning);
  warning.value="10";warning.dispatch("input");
  assert.equal(exportWarning.disabled,true);
  warning.value="120";warning.dispatch("input");
  assert.equal(exportWarning.disabled,false);
  exportWarning.click();
  assert.equal(env.downloads.length,2);
  const policy=JSON.parse(await env.downloads[1].blob.text());
  assert.equal(policy.warningSeconds,120);
  assert.equal(policy.physicalPowerControl,false);
  assert.equal(policy.authenticatedAgentNetworkAvailable,false);
});

test("Time page reviews trusted NTP, timezone and DST without device writes",async()=>{
  const env=createEnvironment();
  await settled(env);
  env.location.hash="#time";env.listeners.hashchange();
  await settled(env);
  assert.ok(env.calls.includes("time/capabilities"));
  assert.ok(env.calls.includes("time/status"));
  assert.match(allText(env.named.view),/Clock and time synchronization/);
  assert.match(allText(env.named.view),/No qualified clock synchronization/);
  const server=find(env.named.view,"input",n=>n.id==="time-ntp-server");
  const zone=find(env.named.view,"input",n=>n.id==="time-zone");
  const dst=find(env.named.view,"select",n=>n.id==="time-dst");
  const save=find(env.named.view,"button",n=>n.textContent==="Export time-settings review");
  assert.ok(server&&zone&&dst&&save);
  assert.equal(save.disabled,true);
  server.value="192.168.0.2";server.dispatch("input");
  assert.equal(save.disabled,false);
  zone.value="../etc/passwd";zone.dispatch("input");
  assert.equal(save.disabled,true);
  zone.value="America/New_York";zone.dispatch("input");
  assert.equal(save.disabled,false);
  dst.value="utc";dst.dispatch("change");
  assert.equal(save.disabled,true);
  dst.value="automatic";dst.dispatch("change");
  assert.equal(save.disabled,false);
  save.click();
  assert.equal(env.downloads.length,1);
  assert.equal(env.downloads[0].filename,"openh910p-time-settings-review.json");
  const json=JSON.parse(await env.downloads[0].blob.text());
  assert.equal(json.applied,false);
  assert.equal(json.installable,false);
  assert.equal(json.networkTimeSecurityQualified,false);
  assert.equal(json.server,"192.168.0.2");
  assert.equal(json.timezone,"America/New_York");
  assert.equal(json.dst,"automatic");
});

test("All three UPS shutdown and outlet threshold fields validate and export safely",async()=>{
  const env=createEnvironment();
  await settled(env);
  env.location.hash="#shutdown";env.listeners.hashchange();
  await settled(env);
  assert.ok(env.calls.includes("shutdown/schema"));
  assert.ok(env.calls.includes("shutdown/advisory"));
  assert.match(allText(env.named.view),/UPS and outlet shutdown thresholds/);
  assert.match(allText(env.named.view),/No validated active shutdown policy/);
  assert.match(allText(env.named.view),/Outlet 3/);
  const fields=descendants(env.named.view).filter(n=>n.tagName==="fieldset"&&n.className==="shutdown-outlet");
  assert.equal(fields.length,3);
  const save=find(env.named.view,"button",n=>n.textContent==="Export shutdown-policy review");
  const timer=find(env.named.view,"input",n=>n.id==="shutdown-master-timerSeconds");
  const runtime=find(env.named.view,"input",n=>n.id==="shutdown-master-runtimeSeconds");
  const outlet=find(env.named.view,"input",n=>n.id==="shutdown-outlet-3-startupDelay");
  const forced=find(env.named.view,"select",n=>n.id==="shutdown-forcedReboot");
  assert.ok(save&&timer&&runtime&&outlet&&forced);
  assert.equal(save.disabled,false);
  timer.value="61";timer.dispatch("input");
  assert.equal(save.disabled,true);
  timer.value="120";timer.dispatch("input");
  assert.equal(save.disabled,false);
  runtime.value="1200";runtime.dispatch("input");
  outlet.value="120";outlet.dispatch("input");
  forced.value="true";forced.dispatch("change");
  assert.equal(save.disabled,false);
  save.click();
  assert.equal(env.downloads.length,1);
  assert.equal(env.downloads[0].filename,"openh910p-shutdown-policy-review.json");
  const policy=JSON.parse(await env.downloads[0].blob.text());
  assert.equal(policy.applied,false);
  assert.equal(policy.installable,false);
  assert.equal(policy.physicalPowerControl,false);
  assert.equal(policy.master.timerSeconds,120);
  assert.equal(policy.master.runtimeSeconds,1200);
  assert.equal(policy.master.forcedReboot,true);
  assert.equal(policy.outlets.length,2);
  assert.equal(policy.outlets[1].outlet,3);
  assert.equal(policy.outlets[1].startupDelay,120);
});

test("System workspace exports only local drafts, imports atomically and rejects corrupt/secret data",async()=>{
  const env=createEnvironment();
  await settled(env);
  env.location.hash="#system";env.listeners.hashchange();
  await settled(env);
  assert.ok(env.calls.includes("backup/capabilities"));
  assert.match(allText(env.named.view),/Configuration backup and recovery/);
  assert.match(allText(env.named.view),/Not an active device backup/);
  const exportButton=find(env.named.view,"button",
    n=>n.textContent==="Export local workspace");
  const importInput=find(env.named.view,"input",
    n=>n.attributes["aria-label"]==="Import local workspace JSON");
  assert.ok(exportButton&&importInput);
  exportButton.click();
  assert.equal(env.downloads.length,1);
  assert.equal(env.downloads[0].filename,"openh910p-local-workspace-review.json");
  const original=JSON.parse(await env.downloads[0].blob.text());
  assert.equal(original.installable,false);
  assert.equal(original.applied,false);
  assert.deepEqual(Object.keys(original.sections).sort(),
    ["configuration","shutdown","weekly"]);
  assert.equal(original.sections.weekly.length,7);
  assert.equal(original.sections.shutdown.segments.length,2);
  const modified=JSON.parse(JSON.stringify(original));
  modified.sections.configuration["System.Network.HostName"]="laboratory-ups";
  modified.sections.weekly[0].offSeconds=28800;
  modified.sections.weekly[0].onSeconds=64800;
  modified.sections.shutdown.master.runtimeSeconds=600;
  importInput.files=[{size:JSON.stringify(modified).length,
    async text(){return JSON.stringify(modified);}}];
  importInput.dispatch("change");
  await settled(env);
  assert.match(allText(env.named.view),/Validated workspace restored in browser memory/);
  exportButton.click();
  assert.equal(env.downloads.length,2);
  const restored=JSON.parse(await env.downloads[1].blob.text());
  assert.equal(restored.sections.configuration["System.Network.HostName"],"laboratory-ups");
  assert.equal(restored.sections.weekly[0].offSeconds,28800);
  assert.equal(restored.sections.shutdown.master.runtimeSeconds,600);
  const poisoned=JSON.parse(JSON.stringify(modified));
  poisoned.sections.configuration["System.Network.Password"]="secret";
  importInput.files=[{size:JSON.stringify(poisoned).length,
    async text(){return JSON.stringify(poisoned);}}];
  importInput.dispatch("change");
  await settled(env);
  assert.match(allText(env.named.view),/Import rejected/);
  exportButton.click();
  const preserved=JSON.parse(await env.downloads[2].blob.text());
  assert.equal(preserved.sections.configuration["System.Network.HostName"],"laboratory-ups");
  assert.equal(preserved.sections.configuration["System.Network.Password"],undefined);
  const badShutdown=JSON.parse(JSON.stringify(modified));
  badShutdown.sections.shutdown.segments[1].startupDelay=999999;
  importInput.files=[{size:JSON.stringify(badShutdown).length,
    async text(){return JSON.stringify(badShutdown);}}];
  importInput.dispatch("change");
  await settled(env);
  assert.match(allText(env.named.view),/Import rejected/);
  exportButton.click();
  const still=JSON.parse(await env.downloads[3].blob.text());
  assert.equal(still.sections.shutdown.master.runtimeSeconds,600);
  assert.equal(still.sections.shutdown.segments[1].startupDelay,0);
  assert.ok(env.calls.every(x=>typeof x==="string"));
});
