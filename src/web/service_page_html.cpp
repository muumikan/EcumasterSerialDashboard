#include <Arduino.h>

namespace ecu {

// The whole page: markup, styles and script in one PROGMEM string.
//
// Nothing is loaded from anywhere. This access point has no route to the
// internet, so a stylesheet or a font from a CDN would not arrive and a
// framework would have to be carried in flash for no gain. System fonts and a
// few hundred lines of plain script do everything this page needs.
extern const char kServicePageHtml[] PROGMEM;
extern const char kServicePageHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EcuDash service</title>
<style>
:root{
 --paper:#F1F1EE;--surface:#FBFBF9;--sunk:#E8E8E3;--ink:#16181C;--soft:#5C6067;
 --faint:#8C9098;--rule:#D3D3CD;--rule2:#E2E2DC;--stamp:#23479A;--stampbg:#E5EAF6;
 --on-stamp:#fff;--warn:#A9660A;--warnbg:#F7EBD8;--crit:#AE241C;--critbg:#F8E2E0;--ok:#2C6A50;
 --mono:ui-monospace,"Cascadia Mono",Consolas,"SF Mono","DejaVu Sans Mono",monospace;
 --ui:"Segoe UI",system-ui,-apple-system,Arial,sans-serif;color-scheme:light}
@media(prefers-color-scheme:dark){:root{
 --paper:#131519;--surface:#1A1D22;--sunk:#101216;--ink:#E7E8E6;--soft:#A0A5AD;
 --faint:#6E747D;--rule:#2C313A;--rule2:#23272E;--stamp:#7E9DEC;--stampbg:#1C2434;
 --on-stamp:#10131A;--warn:#DFA85C;--warnbg:#2A2317;--crit:#E5766B;--critbg:#2C1D1C;
 --ok:#63AE87;color-scheme:dark}}
*{box-sizing:border-box}
body{margin:0;background:var(--paper);color:var(--ink);font:14px/1.5 var(--ui)}
.wrap{max-width:1000px;margin:0 auto;padding:0 20px 64px}
header{background:var(--surface);border-bottom:1px solid var(--rule)}
.hin{max-width:1000px;margin:0 auto;padding:16px 20px 0;display:flex;flex-wrap:wrap;gap:16px 28px;align-items:flex-start}
h1{margin:0;font-size:18px;font-weight:650}
.sub{font:12px var(--mono);color:var(--soft)}
.ident{margin-right:auto}
.facts{display:flex;gap:24px;flex-wrap:wrap}
.fact .k{font-size:10px;text-transform:uppercase;letter-spacing:.09em;color:var(--faint);font-weight:600}
.fact .v{font:13px var(--mono);font-variant-numeric:tabular-nums}
nav{max-width:1000px;margin:0 auto;padding:14px 20px 0;display:flex;gap:2px;overflow-x:auto}
nav button{background:none;border:0;border-bottom:2px solid transparent;font:12px var(--mono);
 letter-spacing:.07em;text-transform:uppercase;color:var(--soft);padding:9px 14px;cursor:pointer}
nav button[aria-selected=true]{color:var(--ink);border-bottom-color:var(--stamp);font-weight:600}
nav button:focus-visible,.btn:focus-visible,input:focus-visible{outline:2px solid var(--stamp);outline-offset:1px}
.notice{display:flex;gap:10px;border-left:3px solid var(--warn);background:var(--warnbg);padding:10px 14px;margin-top:20px;font-size:13px}
section[hidden]{display:none}
section{padding-top:24px}
h2{margin:0 0 2px;font:700 12px/1.5 var(--mono);text-transform:uppercase;letter-spacing:.1em;color:var(--soft)}
.lede{margin:0 0 14px;color:var(--soft);font-size:13px;max-width:64ch}
.block+.block{margin-top:32px}
.scroll{overflow-x:auto}
table{border-collapse:collapse;width:100%;font-size:13px}
th{text-align:left;font:600 10px var(--mono);text-transform:uppercase;letter-spacing:.08em;
 color:var(--faint);padding:0 12px 7px 0;border-bottom:1px solid var(--rule);white-space:nowrap}
td{padding:8px 12px 8px 0;border-bottom:1px solid var(--rule2);vertical-align:baseline}
.num{font-family:var(--mono);font-variant-numeric:tabular-nums;white-space:nowrap}
.name{font-family:var(--mono)}
.muted{color:var(--soft)}
.btn{font:550 13px var(--ui);padding:7px 14px;border-radius:3px;cursor:pointer;
 border:1px solid var(--rule);background:var(--surface);color:var(--ink)}
.btn:hover:not(:disabled){border-color:var(--faint)}
.btn.primary{background:var(--stamp);border-color:var(--stamp);color:var(--on-stamp)}
.btn.danger{color:var(--crit);border-color:var(--crit)}
.btn.small{padding:4px 10px;font-size:12px}
.btn:disabled{opacity:.45;cursor:default}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.row.pad{margin-top:14px}
input[type=text],input[type=number]{font:13px var(--mono);font-variant-numeric:tabular-nums;
 padding:5px 8px;width:92px;text-align:right;border:1px solid var(--rule);border-radius:3px;
 background:var(--surface);color:var(--ink)}
input[type=text]{text-align:left;width:220px}
.unit{font:12px var(--mono);color:var(--faint)}
.range{font:11px var(--mono);color:var(--faint);white-space:nowrap}
.chg{position:relative}
.chg::before{content:"";position:absolute;left:-11px;top:50%;transform:translateY(-50%);
 width:3px;height:15px;background:var(--stamp);border-radius:1px}
.kv{display:grid;grid-template-columns:repeat(auto-fill,minmax(214px,1fr));gap:1px;
 background:var(--rule2);border:1px solid var(--rule2)}
.kv>div{background:var(--surface);padding:10px 13px;display:flex;flex-direction:column;gap:2px}
.kv .k{font-size:10px;text-transform:uppercase;letter-spacing:.09em;color:var(--faint);font-weight:600}
.kv .v{font:14px var(--mono);font-variant-numeric:tabular-nums}
.kv .v.warnv{color:var(--warn)}.kv .v.critv{color:var(--crit)}.kv .v.okv{color:var(--ok)}
.kv .n{font-size:11px;color:var(--faint)}
.ev{display:grid;grid-template-columns:88px 92px 1fr 80px 90px;gap:0 14px;padding:9px 12px;
 border-bottom:1px solid var(--rule2);border-left:3px solid transparent;align-items:baseline}
.ev.crit{border-left-color:var(--crit);background:var(--critbg)}
.ev.warn{border-left-color:var(--warn)}
.ev .t,.ev .rpm,.ev .dur{font:12px var(--mono);color:var(--soft);font-variant-numeric:tabular-nums}
.ev .lbl{font:650 13px var(--mono)}.ev .val{font:13px var(--mono)}
.confirm{border-left:3px solid var(--crit);background:var(--critbg);padding:12px 14px;
 margin-top:14px;display:flex;gap:16px;flex-wrap:wrap;justify-content:space-between;align-items:flex-start}
.foot{margin-top:40px;padding-top:14px;border-top:1px solid var(--rule);font-size:12px;
 color:var(--faint);display:flex;gap:18px;flex-wrap:wrap}
.toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%);background:var(--ink);
 color:var(--paper);font-size:13px;padding:9px 16px;border-radius:3px;opacity:0;
 pointer-events:none;transition:opacity .18s}
.toast.on{opacity:1}
@media(prefers-reduced-motion:reduce){*{transition:none!important}}
@media(max-width:640px){.ev{grid-template-columns:78px 1fr}}
</style></head><body>

<header>
 <div class="hin">
  <div class="ident"><h1>EcuDash &mdash; service</h1><div class="sub" id="build">&nbsp;</div></div>
  <div class="facts">
   <div class="fact"><div class="k">Engine</div><div class="v" id="fEngine">&mdash;</div></div>
   <div class="fact"><div class="k">Battery</div><div class="v" id="fBatt">&mdash;</div></div>
   <div class="fact"><div class="k">Clock</div><div class="v" id="fClock">&mdash;</div></div>
   <div class="fact"><div class="k">AP timeout</div><div class="v" id="fIdle">&mdash;</div></div>
  </div>
 </div>
 <nav role="tablist">
  <button role="tab" aria-selected="true" data-tab="logs">Logs</button>
  <button role="tab" aria-selected="false" data-tab="settings">Settings</button>
  <button role="tab" aria-selected="false" data-tab="diag">Diagnostics</button>
  <button role="tab" aria-selected="false" data-tab="alarms">Alarms</button>
 </nav>
</header>

<div class="wrap">
 <div class="notice" id="compat" hidden style="border-left-color:var(--crit);background:var(--critbg)">
  <b>This browser cannot run this page.</b><span id="compatWhy"></span></div>

 <div class="notice"><b>The access point stays up only while the engine is stopped.</b>
  <span>Starting the engine drops this network and resumes logging. Finish any download first.</span></div>

 <section id="logs">
  <div class="block">
   <h2>Recorded drives</h2>
   <p class="lede">Files close when the engine stops, so the drive you just finished is already
    complete. Download them and open them in EMU Classic Client.</p>
   <div class="scroll"><table>
    <thead><tr><th style="width:30px"><input type="checkbox" id="all" aria-label="Select all"></th>
     <th style="width:46%">File</th><th class="num">Size</th><th></th></tr></thead>
    <tbody id="logRows"><tr><td colspan="4" class="muted">Reading the card&hellip;</td></tr></tbody>
   </table></div>
   <div class="row pad">
    <button class="btn small" id="selAll">Select all</button>
    <button class="btn small" id="selNone">Clear selection</button>
    <button class="btn primary" id="dl" disabled>Download</button>
    <button class="btn danger" id="del" disabled>Delete</button>
    <span class="muted" id="sel">Nothing selected</span>
   </div>
   <div class="confirm" id="confirm" hidden>
    <div><b id="cTitle">Delete?</b>
     <div class="muted name" id="cList"></div>
     <div class="muted">This cannot be undone. Download anything you still want first.</div></div>
    <div class="row"><button class="btn danger" id="cGo">Delete permanently</button>
     <button class="btn" id="cNo">Cancel</button></div>
   </div>
   <div class="row pad"><span class="muted" id="card">&nbsp;</span></div>
  </div>

  <div class="block">
   <h2>Name the next drive</h2>
   <p class="lede">Appended to the next log file, for telling a tuning day's runs apart. The
    dashboard's own panel cannot enter text at all.</p>
   <div class="row">
    <input type="text" id="session" maxlength="24" spellcheck="false" placeholder="dyno-run-1">
    <button class="btn" id="saveSession">Save</button>
    <span class="muted name" id="preview"></span>
   </div>
  </div>
 </section>

 <section id="settings" hidden>
  <div class="block">
   <h2>Settings</h2>
   <p class="lede">The same values the panel holds, and only those. A bar beside a field means it
    differs from the built-in default; out-of-range entries are clamped exactly as a button press is.</p>
   <div id="setForm" class="muted">Loading&hellip;</div>
  </div>
 </section>

 <section id="diag" hidden>
  <div class="block">
   <h2>Clock</h2>
   <p class="lede">The dashboard has no network and no GPS, so its clock is seeded from the build
    stamp and drifts from there. This browser knows the right time.</p>
   <div class="kv" id="clockKv"></div>
   <div class="row pad"><button class="btn primary" id="setClock">Set dashboard clock from this computer</button></div>
  </div>
  <div class="block"><h2>Logging</h2><div class="kv" id="logKv"></div></div>
  <div class="block"><h2>Link and system</h2><div class="kv" id="sysKv"></div>
   <div class="row pad"><button class="btn" id="reboot">Restart dashboard</button>
    <span class="muted">The log file is closed first.</span></div></div>
 </section>

 <section id="alarms" hidden>
  <div class="block">
   <h2>Events this run</h2>
   <p class="lede">Every condition that tripped, newest first, with the moment and the engine speed
    at onset.</p>
   <div id="events" class="muted">Loading&hellip;</div>
   <div class="row pad"><span class="muted" id="evFoot"></span></div>
  </div>
 </section>

 <div class="foot"><span>Served from the dashboard</span><span>Read-only towards the ECU</span>
  <span>No internet on this network &mdash; the page carries its own styles</span></div>
</div>
<div class="toast" id="toast" role="status" aria-live="polite"></div>

<script>
var $=function(id){return document.getElementById(id)};

/* A page that dies quietly reads as a page that is empty, which is exactly how
   the first old browser to fail here was diagnosed - by reading the source
   rather than by anything the page said. It says it now. */
function bail(why){
 var box=document.getElementById("compat");
 if(!box){return}
 document.getElementById("compatWhy").textContent=" "+why;
 box.hidden=false;
}
window.onerror=function(msg){bail("It stopped with: "+msg+".");return false};
/* querySelectorAll returns a NodeList, and NodeList.forEach does not exist in
   Edge Legacy or IE - calling it threw before the first fetch ever ran, which
   left the page a shell with no data in it. Arrays keep their own forEach;
   this is only for the ones that come back from the DOM. */
function each(list,fn){for(var i=0;i<list.length;i++){fn(list[i],i)}}
var toastT=null;
function toast(m){var t=$("toast");t.textContent=m;t.classList.add("on");
 clearTimeout(toastT);toastT=setTimeout(function(){t.classList.remove("on")},2400)}
function esc(s){return String(s).replace(/[&<>"]/g,function(c){
 return {"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]})}
function mb(b){return b<1024*1024?(b/1024).toFixed(0)+" kB":(b/1048576).toFixed(1)+" MB"}
function two(n){return (n<10?"0":"")+n}
function post(url,body){return fetch(url,{method:"POST",
 headers:{"Content-Type":"application/x-www-form-urlencoded"},body:body})}

/* ---- tabs ---- */
var tabs=document.querySelectorAll("nav button");
each(tabs,function(b){b.onclick=function(){
 var tab=b.getAttribute("data-tab");
 each(tabs,function(o){o.setAttribute("aria-selected",o===b?"true":"false")});
 each(["logs","settings","diag","alarms"],function(id){
  $(id).hidden=(id!==tab)});
 if(tab==="settings")loadSettings();
 if(tab==="alarms")loadEvents();
}});

/* ---- status ---- */
var status={};
function loadStatus(){fetch("/api/status").then(function(r){return r.json()}).then(function(s){
 status=s;
 $("build").textContent=s.linkName+" · "+s.baud+" baud · build "+s.build;
 $("fEngine").textContent=s.rpm>0?s.rpm+" rpm":"stopped";
 $("fBatt").textContent=s.batteryV.toFixed(1)+" V";
 $("fClock").textContent=s.clock;
 $("fIdle").textContent=s.idleLeft>0?Math.floor(s.idleLeft/60)+":"+two(s.idleLeft%60):"—";
 if($("session").value==="")$("session").value=s.session;
 updatePreview();
 drawDiag(s);
}).catch(function(){})}

function kv(k,v,n,cls){return "<div><span class=k>"+k+"</span><span class='v "+(cls||"")+"'>"+
 esc(v)+"</span><span class=n>"+esc(n||"")+"</span></div>"}

function drawDiag(s){
 var dev=s.clock, pc=new Date();
 var pcs=two(pc.getHours())+":"+two(pc.getMinutes())+":"+two(pc.getSeconds());
 var drift="unknown", cls="";
 if(s.rtc&&dev!=="--:--:--"){
  var p=dev.split(":"),d=(pc.getHours()*3600+pc.getMinutes()*60+pc.getSeconds())
   -(+p[0]*3600 + +p[1]*60 + +p[2]);
  var a=Math.abs(d);
  drift=a<2?"in step":(d>0?"+":"-")+Math.floor(a/60)+" min "+two(a%60)+" s";
  cls=a<2?"okv":(a<120?"warnv":"critv");
 }
 $("clockKv").innerHTML=kv("Dashboard clock",dev,s.rtc?"BM8563, battery-backed":"no RTC")+
  kv("This computer",pcs,"from the browser")+kv("Difference",drift,"",cls);

 $("logKv").innerHTML=
  kv("Logging",s.logging?"running":"off",s.logging?"":s.logFailure,s.logging?"okv":"warnv")+
  kv("Current file",s.logFile||"none",s.logging?"":"closed")+
  kv("Frames written",s.frames.toLocaleString(),"this file")+
  kv("Frames dropped",s.dropped,"this file",s.dropped>0?"critv":"okv")+
  kv("Bytes on card",s.logBytes.toLocaleString(),"compressed");

 var up=s.uptime,ups=Math.floor(up/3600)+" h "+two(Math.floor(up/60)%60)+" min";
 $("sysKv").innerHTML=
  kv("ECU link",s.link,s.linkName+", "+s.baud+" baud")+
  kv("Frames decoded",s.updates.toLocaleString(),"since boot")+
  kv("Bad frames",s.badFrames,"since boot",s.badFrames>0?"warnv":"okv")+
  kv("Free heap",(s.heap/1024).toFixed(0)+" kB","")+
  kv("Free PSRAM",(s.psram/1048576).toFixed(1)+" MB","")+
  /* The pool the screens are built from, and the one that runs out first.
     Exhausting it used to reboot the dashboard without a word - see
     lv_conf.h - so it is worth a line here. */
  kv("Screen memory",(s.lvFree/1024).toFixed(0)+" kB free of "+
     (s.lvTotal/1024).toFixed(0)+" kB",s.lvUsedPct+"% used",
     s.lvUsedPct>85?"critv":(s.lvUsedPct>70?"warnv":"okv"))+
  kv("Uptime",ups,"since power-on")+
  kv("Clients",s.clients,"joined to the access point")+
  kv("Firmware",s.build,"");
}

/* ---- logs ---- */
var files=[],active="";
function loadLogs(){fetch("/api/logs").then(function(r){return r.json()}).then(function(d){
 files=d.files;active=d.active;
 files.sort(function(a,b){return a.name<b.name?1:-1});
 var h="";
 if(files.length===0)h='<tr><td colspan="4" class="muted">No logs on the card yet.</td></tr>';
 files.forEach(function(f,i){
  var isActive=(f.name===active);
  h+='<tr><td>'+(isActive?'':'<input type="checkbox" class="cb" data-i="'+i+'">')+'</td>'+
   '<td class="name">'+esc(f.name)+(isActive?' <span class="muted">(recording)</span>':'')+'</td>'+
   '<td class="num">'+mb(f.size)+'</td>'+
   '<td>'+(isActive?'':'<a class="btn small" href="/log?name='+encodeURIComponent(f.name)+'">Download</a>')+'</td></tr>';
 });
 $("logRows").innerHTML=h;
 $("card").textContent=files.length+" files · "+mb(d.totalBytes)+
  " · card "+((d.cardBytes-d.usedBytes)/1048576).toFixed(1)+" GB free of "+
  (d.cardBytes/1048576).toFixed(1)+" GB";
 each(document.querySelectorAll(".cb"),function(c){c.onchange=refreshSel});
 refreshSel();
}).catch(function(){$("logRows").innerHTML=
 '<tr><td colspan="4" class="muted">The card could not be read.</td></tr>'})}

function selected(){var out=[];each(document.querySelectorAll(".cb"),function(c){
 if(c.checked)out.push(files[+c.getAttribute("data-i")])});return out}

function refreshSel(){
 var s=selected(),n=s.length,bytes=0;
 s.forEach(function(f){bytes+=f.size});
 $("dl").disabled=n===0;$("del").disabled=n===0;
 $("dl").textContent=n>1?"Download "+n+" as .tar ("+mb(bytes)+")":
  (n===1?"Download ("+mb(bytes)+")":"Download");
 $("del").textContent=n?"Delete "+n:"Delete";
 $("sel").textContent=n?n+" of "+files.length+" selected":"Nothing selected";
 var boxes=document.querySelectorAll(".cb");
 $("all").checked=n>0&&n===boxes.length;
 $("all").indeterminate=n>0&&n<boxes.length;
 $("confirm").hidden=true;
}
function setAll(v){each(document.querySelectorAll(".cb"),function(c){c.checked=v});refreshSel()}
$("all").onchange=function(){setAll($("all").checked)};
$("selAll").onclick=function(){setAll(true)};
$("selNone").onclick=function(){setAll(false)};

$("dl").onclick=function(){
 var s=selected();if(!s.length)return;
 var names=s.map(function(f){return f.name});
 location.href = names.length===1
  ? "/log?name="+encodeURIComponent(names[0])
  : "/all.tar?names="+encodeURIComponent(names.join(","));
};

$("del").onclick=function(){
 var s=selected();if(!s.length)return;
 $("cTitle").textContent="Delete "+s.length+" file"+(s.length>1?"s":"")+" from the card?";
 $("cList").textContent=s.map(function(f){return f.name}).join(", ");
 $("confirm").hidden=false;
};
$("cNo").onclick=function(){$("confirm").hidden=true};
$("cGo").onclick=function(){
 var names=selected().map(function(f){return f.name});
 post("/api/delete","names="+encodeURIComponent(names.join(",")))
  .then(function(r){return r.json()}).then(function(d){
   toast("Deleted "+d.deleted+(d.refused?", refused "+d.refused:""));
   loadLogs();
  }).catch(function(){toast("The dashboard refused the request")});
};

function updatePreview(){
 var v=$("session").value.replace(/[^A-Za-z0-9_-]/g,"-");
 $("preview").textContent=v?"→ <timestamp>_"+v+".emulog":"→ <timestamp>.emulog";
}
$("session").oninput=updatePreview;
$("saveSession").onclick=function(){
 post("/api/session","name="+encodeURIComponent($("session").value))
  .then(function(){toast("Saved — the next drive will use this name")})
  .catch(function(){toast("The dashboard refused the request")});
};

/* ---- settings ---- */
/* The form is built once and then patched in place. Rebuilding it after every
   edit - which is what this did first - threw away the focus, jumped the
   scroll back to the top and replaced the field under the cursor, so a page
   that worked read as a page that was broken. */
var setData=null;

function markChanged(it,cell){
 var diff=Math.abs(it.value-it["default"])>1e-6;
 cell.className=diff?"chg":"";
 var n=0;
 setData.categories.forEach(function(c){c.items.forEach(function(x){
  if(Math.abs(x.value-x["default"])>1e-6)n++})});
 $("setCount").textContent=n+" setting"+(n===1?"":"s")+" differ from defaults";
}

function sendSetting(el){
 var ci=+el.getAttribute("data-c"), ii=+el.getAttribute("data-i");
 var it=setData.categories[ci].items[ii];
 var v=el.type==="checkbox"?(el.checked?1:0):el.value;
 el.disabled=true;
 post("/api/settings","category="+ci+"&index="+ii+"&value="+encodeURIComponent(v))
  .then(function(r){if(!r.ok)throw 0;return r.json()})
  .then(function(d){
   /* The server clamps, so what comes back is the truth and may differ from
      what was typed. */
   it.value=d.value;
   if(el.type==="checkbox"){el.checked=d.value>0.5}
   else{el.value=d.value.toFixed(it.decimals)}
   markChanged(it,el.parentNode);
   toast("Applied \u2014 stored a few seconds after the last change");
  })
  .catch(function(){toast("The dashboard refused that value");loadSettings()})
  .then(function(){el.disabled=false;el.focus()});
}

function loadSettings(){fetch("/api/settings").then(function(r){return r.json()}).then(function(d){
 setData=d;
 var h="",changed=0;
 d.categories.forEach(function(cat,ci){
  h+='<div class="block"><h2>'+esc(cat.name)+'</h2><div class="scroll"><table><thead><tr>'+
   '<th style="width:34%">Setting</th><th>Value</th><th>Unit</th><th>Allowed</th></tr></thead><tbody>';
  cat.items.forEach(function(it,ii){
   var diff=Math.abs(it.value-it["default"])>1e-6; if(diff)changed++;
   var ctl=it["bool"]
    ?'<input type="checkbox" data-c="'+ci+'" data-i="'+ii+'" class="sv"'+(it.value>0.5?" checked":"")+'>'
    /* step="any" on purpose: the real step is in the Allowed column, and a
       browser validating 0.88 against step 0.01 in binary floating point
       marks a perfectly good value invalid. The server clamps regardless. */
    :'<input type="number" class="sv" data-c="'+ci+'" data-i="'+ii+'" value="'+
      it.value.toFixed(it.decimals)+'" min="'+it.min+'" max="'+it.max+'" step="any">';
   h+='<tr><td>'+esc(it.key)+'</td><td'+(diff?' class="chg"':'')+'>'+ctl+'</td>'+
    '<td class="unit">'+esc(it.unit)+'</td><td class="range">'+
    (it["bool"]?"off / on":it.min+" \u2026 "+it.max+(it.step?" step "+it.step:""))+'</td></tr>';
  });
  h+="</tbody></table></div></div>";
 });
 h+='<div class="row pad"><span class="muted" id="setCount">'+changed+
  ' setting'+(changed===1?"":"s")+' differ from defaults</span></div>';
 $("setForm").innerHTML=h;
 each(document.querySelectorAll(".sv"),function(el){
  el.onchange=function(){sendSetting(el)};
  /* Enter commits without having to click away first. */
  el.onkeydown=function(e){if(e.key==="Enter"){e.preventDefault();el.blur()}};
 });
}).catch(function(){$("setForm").textContent="The settings could not be read."})}

/* ---- alarms ---- */
function loadEvents(){fetch("/api/events").then(function(r){return r.json()}).then(function(d){
 if(!d.available){$("events").textContent=
  "The alarm log lives with the display, which did not come up on this boot.";return}
 if(!d.events.length){$("events").textContent="Nothing has tripped this run.";
  $("evFoot").textContent="";return}
 var h="";
 d.events.forEach(function(e){
  h+='<div class="ev '+e.severity+'"><span class="t">'+esc(e.time)+'</span>'+
   '<span class="lbl">'+esc(e.label)+'</span><span class="val">'+esc(e.value)+'</span>'+
   '<span class="rpm">'+e.rpm+' rpm</span><span class="dur">'+
   (e.active?"still open":e.seconds+" s")+'</span></div>';
 });
 $("events").innerHTML=h;
 $("evFoot").textContent=d.events.length+" of "+d.capacity+
  " slots used · "+d.total+" events this run";
}).catch(function(){$("events").textContent="The alarm log could not be read."})}

/* ---- actions ---- */
$("setClock").onclick=function(){
 var n=new Date();
 post("/api/clock","year="+n.getFullYear()+"&month="+(n.getMonth()+1)+"&day="+n.getDate()+
  "&hour="+n.getHours()+"&minute="+n.getMinutes()+"&second="+n.getSeconds())
 .then(function(r){if(!r.ok)throw 0;
  toast("Clock set — the next log file will be named correctly");loadStatus()})
 .catch(function(){toast("The clock did not accept the time")});
};
$("reboot").onclick=function(){
 if(!confirm("Restart the dashboard now?"))return;
 post("/api/reboot","").catch(function(){});
 toast("Restarting — this page will stop responding");
};

/* Everything on this page is fetched, so without these there is nothing to
   show and no point starting. Both arrived in Edge 14 and neither is in IE. */
if(typeof window.fetch!=="function"||typeof window.Promise!=="function"){
 bail("It has no fetch or Promise support, which everything on this page is "+
      "built on. Use a current browser - any Chromium-based one, or Firefox.");
}else{
 loadStatus();loadLogs();
 setInterval(loadStatus,5000);
}
</script></body></html>
)HTML";

}  // namespace ecu
