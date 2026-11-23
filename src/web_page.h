#pragma once
#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Solo RTK Base</title>
<style>
:root{--bg:#111418;--card:#1b2027;--fg:#e6e9ee;--mut:#8a94a3;--ok:#3fb950;--warn:#d29922;--bad:#f85149;--acc:#58a6ff}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.4 system-ui,sans-serif}
header{padding:12px 16px;border-bottom:1px solid #2a313b;display:flex;gap:12px;align-items:center;flex-wrap:wrap}
h1{font-size:17px;margin:0}main{padding:12px 16px;display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(300px,1fr))}
.card{background:var(--card);border-radius:8px;padding:12px 14px}.card h2{font-size:13px;margin:0 0 8px;color:var(--mut);text-transform:uppercase;letter-spacing:.05em}
table{width:100%;border-collapse:collapse}td{padding:2px 0;vertical-align:top}td:last-child{text-align:right;font-variant-numeric:tabular-nums}
.pill{display:inline-block;padding:1px 8px;border-radius:10px;font-size:12px;font-weight:600}
.ok{background:#1f3d27;color:var(--ok)}.warn{background:#3d321a;color:var(--warn)}.bad{background:#40201f;color:var(--bad)}
label{display:block;margin:8px 0 2px;color:var(--mut);font-size:12px}
input,select{width:100%;padding:7px 8px;background:#0d1014;color:var(--fg);border:1px solid #2f3742;border-radius:6px;font:inherit}
.row{display:grid;grid-template-columns:1fr 1fr;gap:8px}
button{margin-top:12px;padding:8px 14px;border:0;border-radius:6px;background:var(--acc);color:#04111f;font-weight:600;cursor:pointer}
button.sec{background:#2f3742;color:var(--fg)}button.danger{background:#5a1f1d;color:#ffd7d5}
#msg{margin-top:8px;font-size:13px}.mut{color:var(--mut)}
</style></head><body>
<header><h1>Solo RTK Base</h1><span id="hdr" class="pill warn">loading</span><span class="mut" id="up"></span></header>
<main>
<section class="card"><h2>RTCM from base GPS</h2><table id="rtcm"></table><table id="types" style="margin-top:8px"></table></section>
<section class="card"><h2>MAVLink link</h2><table id="mav"></table></section>
<section class="card"><h2>Base position (1005/1006)</h2><table id="base"></table></section>
<section class="card"><h2>Wi-Fi</h2><table id="wifi"></table></section>
<section class="card"><h2>Configuration</h2>
<form id="cfg">
<label>SoloLink SSID</label><input name="ssid" maxlength="32" placeholder="SoloLink_XXXXXX">
<label>SoloLink password <span class="mut">(blank keeps current)</span></label><input name="pass" type="password" maxlength="63" autocomplete="new-password">
<div class="row"><div><label>Fallback IP</label><input name="fallback_ip"></div><div><label>Fallback port</label><input name="fallback_port" type="number"></div></div>
<div class="row"><div><label>Local UDP port</label><input name="local_port" type="number"></div><div><label>Lock timeout (s)</label><input name="lock_timeout_s" type="number"></div></div>
<div class="row"><div><label>MAVLink sysid</label><input name="sysid" type="number" min="1" max="255"></div><div><label>compid</label><input name="compid" type="number" min="0" max="255"></div></div>
<div class="row"><div><label>GPS baud</label><select name="gps_baud">
<option>9600</option><option>19200</option><option>38400</option><option>57600</option><option>115200</option><option>230400</option><option>460800</option><option>921600</option></select></div>
<div><label>Setup AP</label><select name="ap_always"><option value="0">Auto</option><option value="1">Always on</option></select></div></div>
<label>Setup AP password <span class="mut">(blank keeps current)</span></label><input name="ap_pass" type="password" maxlength="63" autocomplete="new-password">
<button type="submit">Save &amp; reboot</button> <button type="button" class="sec" id="reboot">Reboot</button> <button type="button" class="danger" id="factory">Factory reset</button>
<div id="msg"></div>
</form></section>
</main>
<script>
const $=id=>document.getElementById(id);
const FIX=["No GPS","No fix","2D","3D","DGPS","RTK float","RTK fixed","Static","PPP"];
const pill=(t,c)=>`<span class="pill ${c}">${t}</span>`;
const rows=(el,r)=>{$(el).innerHTML=r.map(([k,v])=>`<tr><td>${k}</td><td>${v}</td></tr>`).join("")};
const ago=ms=>ms<0?"never":ms<1500?"now":(ms/1000).toFixed(ms<10000?1:0)+" s ago";
const esc=s=>String(s).replace(/[&<>"]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]));
async function tick(){
 try{
  const s=await (await fetch("/api/status")).json();
  const r=s.rtcm,m=s.mav,w=s.wifi,b=s.base;
  const good=!r.stale&&w.connected&&m.tx>0;
  $("hdr").className="pill "+(good?"ok":r.stale?"bad":"warn");
  $("hdr").textContent=!w.connected?"Wi-Fi down":r.stale?"No RTCM":m.locked?"Injecting":"Injecting (fallback)";
  $("up").textContent=`${s.version} · up ${Math.floor(s.uptime_s/3600)}h${Math.floor(s.uptime_s/60)%60}m · last reset: ${s.reset_reason} · heap ${s.heap>>10}k (min ${s.heap_min>>10}k)`;
  rows("rtcm",[["Feed",r.stale?pill("stale","bad"):pill("live","ok")],["Last frame",ago(r.last_frame_ms)],["Rate",r.bps+" B/s"],
   ["Frames OK",r.frames],["CRC errors",r.crc_err],["Header errors",r.hdr_err],["Bytes discarded",r.discarded],
   [">720 B dropped",r.oversize?pill(r.oversize,"bad"):0],["Send failures",r.send_fail]]);
  $("types").innerHTML="<tr><td class=mut>Msg</td><td class=mut>count · last len · age</td></tr>"+
   r.types.sort((a,c)=>a.t-c.t).map(t=>`<tr><td>${t.t}</td><td>${t.n} · ${t.len} B · ${ago(t.ago_ms)}</td></tr>`).join("");
  const fx=m.veh_fix<0?"unknown":FIX[m.veh_fix]||m.veh_fix;
  rows("mav",[["Target",(m.locked?pill("locked","ok"):pill("fallback","warn"))+" "+esc(m.target)],
   ["Last telemetry",ago(m.last_rx_ms)],["Vehicle sysid",m.veh_sysid||"-"],
   ["Vehicle GPS",m.veh_fix>=6?pill(fx,"ok"):m.veh_fix==5?pill(fx,"warn"):fx],["Vehicle sats / HDOP",m.veh_sats+" / "+(m.veh_hdop_cm/100).toFixed(2)],
   ["GPS_RTCM_DATA sent",m.tx],["Send errors",m.tx_err],["Skipped (no Wi-Fi)",m.tx_no_wifi],
   ["Telemetry msgs in",m.rx_msgs],["MAVLink CRC errors",m.rx_crc_err],["Lock changes",m.lock_changes]]);
  rows("base",b.valid?[["Message / station",b.msg+" / "+b.station],["Latitude",b.lat.toFixed(8)],["Longitude",b.lon.toFixed(8)],
   ["Height (ellipsoid)",b.h.toFixed(3)+" m"],["Last seen",ago(b.ago_ms)]]:[["Status",pill("no 1005/1006 yet","warn")],["","Survey-in still running, or 1005 not enabled"]]);
  rows("wifi",[["SoloLink",w.connected?pill("connected","ok"):pill("down","bad")],["SSID",esc(w.ssid||"(not set)")],["IP",w.ip],
   ["RSSI / channel",w.rssi+" dBm / "+w.channel],["Up for",w.up_s+" s"],["Attempts / drops",w.attempts+" / "+w.disconnects],
   ["Last drop reason",w.last_reason],["Setup AP",w.ap?`${esc(w.ap_ssid)} @ ${w.ap_ip} (${w.ap_clients} client)`:"off"]]);
 }catch(e){$("hdr").className="pill bad";$("hdr").textContent="offline"}
}
async function loadCfg(){
 const c=await (await fetch("/api/config")).json();const f=$("cfg");
 for(const k of["ssid","fallback_ip","fallback_port","local_port","lock_timeout_s","sysid","compid","gps_baud"])f[k].value=c[k];
 f.ap_always.value=c.ap_always?"1":"0";
}
async function post(url,body){const r=await fetch(url,{method:"POST",body});return [r.ok,await r.json()]}
$("cfg").onsubmit=async e=>{e.preventDefault();
 if(+e.target.sysid.value===255&&!confirm("sysid 255 is the default SYSID_MYGCS. Use it anyway?"))return;
 const [ok,j]=await post("/api/config",new URLSearchParams(new FormData(e.target)));
 $("msg").textContent=ok?"Saved. Rebooting, reload in ~10 s.":"Error: "+j.error;$("msg").className=ok?"":"bad"};
$("reboot").onclick=()=>post("/api/reboot").then(()=>$("msg").textContent="Rebooting…");
$("factory").onclick=()=>{if(confirm("Erase all settings?"))post("/api/factory").then(()=>$("msg").textContent="Reset. Join the SoloRTK setup AP.")};
loadCfg();tick();setInterval(tick,1000);
</script></body></html>)HTML";
