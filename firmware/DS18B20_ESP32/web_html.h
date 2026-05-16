#pragma once
// ============================================================
// web_html.h  –  DS18B20 Temperature Monitor Web UI
// Stored in flash (PROGMEM).  Served as text/html.
//
// Features:
//   • Real-time temperature display via WebSocket
//   • Line chart with 60-point history per sensor
//   • Min / Max / Avg statistics
//   • Alarm threshold configuration
//   • WiFi AP / Station mode configuration
//   • Dark theme, responsive layout
// ============================================================
#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"HTMLEOF(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>DS18B20 Temperature Monitor</title>
<style>
:root{--bg:#07080e;--panel:#0d0f1a;--border:#1a2236;--acc:#00c8ff;--acc2:#00ff99;--warn:#ff5e3a;--txt:#a8bac8;--dim:#2a3548;--red:#ff3a3a;--blue:#3a8fff}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--txt);font-family:'Courier New',monospace;font-size:12px;min-height:100vh}
/* ── Header ── */
.hdr{display:flex;align-items:center;justify-content:space-between;padding:8px 14px;background:var(--panel);border-bottom:1px solid var(--border)}
.hdr h1{color:var(--acc);font-size:14px;letter-spacing:2px}
.dot{width:8px;height:8px;border-radius:50%;background:#222;display:inline-block;margin-right:5px;transition:background .4s}
.dot.on{background:var(--acc2);box-shadow:0 0 6px var(--acc2)}
/* ── Status bar ── */
.sbar{display:flex;flex-wrap:wrap;gap:10px;padding:5px 14px;background:var(--panel);border-bottom:1px solid var(--border);font-size:11px;color:var(--dim)}
.sbar b{color:var(--acc)}
/* ── Sensor cards ── */
.cards{display:flex;flex-wrap:wrap;gap:10px;padding:12px}
.card{flex:1;min-width:220px;background:var(--panel);border:1px solid var(--border);border-radius:6px;overflow:hidden;transition:border-color .3s}
.card.alarm{border-color:var(--warn);box-shadow:0 0 8px rgba(255,94,58,.3)}
.card-hdr{display:flex;align-items:center;justify-content:space-between;padding:8px 12px;border-bottom:1px solid var(--border)}
.card-name{font-size:12px;font-weight:bold;color:var(--acc)}
.card-rom{font-size:9px;color:var(--dim)}
.temp-big{text-align:center;padding:12px 0 6px;font-size:42px;font-weight:bold;letter-spacing:-1px}
.temp-big.hot{color:var(--warn)}
.temp-big.cold{color:var(--blue)}
.temp-big.ok{color:var(--acc2)}
.unit{font-size:20px;color:var(--dim)}
.stats{display:flex;justify-content:space-around;padding:6px 12px 10px;font-size:10px;color:var(--dim)}
.stats span b{color:var(--txt)}
.chart-wrap{padding:0 8px 8px;position:relative}
canvas.chart{display:block;width:100%;height:80px}
/* ── Alarm settings ── */
.alarm-row{display:flex;align-items:center;gap:6px;padding:4px 12px 8px;font-size:10px}
.alarm-row label{color:var(--dim);min-width:30px}
.alarm-row input[type=number]{width:60px;padding:2px 5px}
.alarm-row button{padding:2px 8px;font-size:10px}
/* ── Inputs global ── */
input,select{background:#06060f;border:1px solid var(--border);color:var(--txt);padding:4px 8px;border-radius:3px;font-family:inherit;font-size:11px}
input:focus,select:focus{outline:none;border-color:var(--acc)}
/* ── Bottom panel ── */
.bottom{display:flex;flex-wrap:wrap;gap:10px;padding:0 12px 12px}
/* ── Log ── */
.logbox{flex:2;min-width:260px;background:var(--panel);border:1px solid var(--border);border-radius:6px;overflow:hidden}
.logbox-hdr{display:flex;align-items:center;justify-content:space-between;padding:6px 12px;border-bottom:1px solid var(--border)}
.logbox-hdr h3{font-size:11px;color:var(--acc)}
.log{height:140px;overflow-y:auto;padding:6px 10px;font-size:10px;line-height:1.7}
.log .ts{color:var(--dim)}
.log .err{color:var(--warn)}
.log .ok{color:var(--acc2)}
/* ── WiFi panel ── */
.wp{flex:1;min-width:240px;background:var(--panel);border:1px solid var(--border);border-radius:6px;overflow:hidden}
.wphdr{display:flex;align-items:center;justify-content:space-between;padding:6px 12px;border-bottom:1px solid var(--border);cursor:pointer;user-select:none}
.wphdr h3{font-size:11px;color:var(--acc)}
.wpbody{padding:10px 12px}
.collapsed .wpbody{display:none}
.tabs{display:flex;gap:4px;margin-bottom:9px}
.tab{flex:1;padding:4px;text-align:center;border:1px solid var(--border);border-radius:3px;cursor:pointer;font-size:10px;transition:all .15s}
.tab.act{border-color:var(--acc);color:var(--acc);background:rgba(0,200,255,.08)}
.row{display:flex;align-items:center;gap:7px;margin-bottom:7px}
.row label{min-width:58px;font-size:10px;color:var(--dim)}
.row input,.row select{flex:1}
.netlist{max-height:110px;overflow-y:auto;margin:4px 0 8px}
.net{display:flex;align-items:center;padding:3px 6px;border:1px solid transparent;border-radius:2px;cursor:pointer;font-size:10px}
.net:hover,.net.sel{border-color:var(--border);background:rgba(255,255,255,.03)}
.net-s{flex:1}.net-r{color:var(--dim);font-size:9px;margin-right:4px}
.wmsg{padding:4px 7px;border-radius:2px;font-size:10px;margin-top:6px;display:none}
.wmsg.ok{background:rgba(0,255,136,.08);color:var(--acc2)}
.wmsg.err{background:rgba(255,94,58,.1);color:var(--warn)}
.wmsg.info{background:rgba(0,200,255,.08);color:var(--acc)}
.btn{padding:3px 11px;border:1px solid var(--border);background:transparent;color:var(--txt);cursor:pointer;border-radius:3px;font-size:10px;font-family:inherit;transition:all .15s}
.btn:hover{border-color:var(--acc);color:var(--acc)}
/* ── No sensor placeholder ── */
.nosensor{text-align:center;padding:40px;color:var(--dim);font-size:13px}
</style>
</head>
<body>

<div class="hdr">
  <div style="display:flex;align-items:center;gap:10px">
    <h1>🌡 DS18B20 TEMPERATURE MONITOR</h1>
  </div>
  <div style="display:flex;align-items:center;gap:5px">
    <span class="dot" id="dot"></span>
    <span id="connTxt" style="font-size:10px;color:var(--dim)">Connecting…</span>
  </div>
</div>

<div class="sbar">
  <span>Mode: <b id="bMode">--</b></span>
  <span>IP: <b id="bIP">--</b></span>
  <span>Sensors: <b id="bSens">0</b></span>
  <span>Power: <b id="bPwr">--</b></span>
  <span>Resolution: <b id="bRes">--</b> bit</span>
  <span>Interval: <b id="bInt">--</b> s</span>
  <span id="bTime" style="margin-left:auto;color:var(--dim)"></span>
</div>

<div class="cards" id="cards">
  <div class="nosensor" id="noSensor">⏳ Connecting to device…</div>
</div>

<div class="bottom">
  <!-- Log -->
  <div class="logbox">
    <div class="logbox-hdr">
      <h3>📋 Event Log</h3>
      <button class="btn" onclick="clearLog()" style="padding:1px 7px;font-size:9px">Clear</button>
    </div>
    <div class="log" id="logEl"></div>
  </div>

  <!-- WiFi config -->
  <div class="wp collapsed" id="wifiPanel">
    <div class="wphdr" onclick="toggleWP()">
      <h3>⚙ WiFi Configuration</h3>
      <span id="wpArrow" style="font-size:9px">▼</span>
    </div>
    <div class="wpbody">
      <div class="tabs">
        <div class="tab act" id="tabAP"  onclick="switchTab('ap')">📡 Access Point</div>
        <div class="tab"     id="tabSTA" onclick="switchTab('sta')">🌐 Station</div>
      </div>
      <div id="panelAP">
        <div class="row"><label>AP Name</label><input id="apSSID" type="text" placeholder="DS18B20_Monitor"></div>
        <div class="row"><label>Password</label><input id="apPass" type="password" placeholder="min 8 chars (blank=open)"></div>
        <button class="btn" onclick="applyAP()" style="width:100%">Apply &amp; Save</button>
      </div>
      <div id="panelSTA" style="display:none">
        <button class="btn" onclick="doScan()" style="margin-bottom:6px">🔍 Scan Networks</button>
        <div class="netlist" id="netList"></div>
        <div class="row"><label>SSID</label><input id="staSSID" type="text" placeholder="or type manually"></div>
        <div class="row"><label>Password</label><input id="staPass" type="password" placeholder="WiFi password"></div>
        <button class="btn" onclick="doConnect()" style="width:100%">Connect</button>
      </div>
      <div class="wmsg" id="wMsg"></div>
    </div>
  </div>
</div>

<script>
// ════════════════════════════════════════════════════════════
//  DS18B20 Temperature Monitor  –  Web UI
//
//  WebSocket JSON protocol:
//    ESP32 → Browser:
//      {t:"data", sensors:[{id,name,rom,tempC,tempF,min,max,avg,alarm},...],
//                 power:"ext|par", res:12, interval:5}
//      {t:"st",   m:"ap|sta", ip:"...", ss:"..."}
//      {t:"w",    n:[{s,r,e},...]}
//      {t:"msg",  m:"...", ok:true}
//      {t:"alarm",id:0, name:"...", tempC:28.5, hi:30, lo:5}
//    Browser → ESP32:
//      {c:"status"}
//      {c:"setAlarm", id:0, lo:-10, hi:85}
//      {c:"setInterval", v:5}
//      {c:"scan"}
//      {c:"ap", s:"ssid", p:"pass"}
//      {c:"sta", s:"ssid", p:"pass"}
// ════════════════════════════════════════════════════════════

const HIST = 60;    // history points per sensor

// Per-sensor chart history
const history = {};   // id → Float32Array(HIST)
const histIdx = {};   // id → next write index (ring buffer)

function ensureHistory(id) {
  if (!history[id]) {
    history[id] = new Float32Array(HIST).fill(NaN);
    histIdx[id]  = 0;
  }
}

function pushHistory(id, v) {
  ensureHistory(id);
  history[id][histIdx[id] % HIST] = v;
  histIdx[id]++;
}

function getHistory(id) {
  if (!history[id]) return [];
  const arr = history[id], idx = histIdx[id];
  const out = [];
  for (let i = 0; i < HIST; i++) out.push(arr[(idx + i) % HIST]);
  return out;
}

// ── Colour helpers ──────────────────────────────────────────
const PALETTE = ['#00c8ff','#00ff99','#ff9a3a','#ff5e3a','#c87aff','#ffee5e','#3affee','#ff3a8a'];

function tempClass(v, lo, hi) {
  if (v >= hi) return 'hot';
  if (v <= lo) return 'cold';
  return 'ok';
}

// ── Sensor card render / update ─────────────────────────────
const cardEls = {};   // id → card dom

function renderAllSensors(sensors) {
  const container = document.getElementById('cards');
  document.getElementById('noSensor').style.display = 'none';

  sensors.forEach((s, idx) => {
    if (!cardEls[s.id]) {
      // Create new card
      const card = document.createElement('div');
      card.className = 'card';
      card.id = 'card-' + s.id;
      card.innerHTML = cardHTML(s, idx);
      container.appendChild(card);
      cardEls[s.id] = card;
    }
    updateCard(s, idx);
    ensureHistory(s.id);
    pushHistory(s.id, s.tempC);
    drawChart(s.id, idx);
  });

  // Remove cards for sensors no longer present
  Object.keys(cardEls).forEach(id => {
    if (!sensors.find(s => String(s.id) === id)) {
      cardEls[id].remove();
      delete cardEls[id];
    }
  });
}

function cardHTML(s, idx) {
  return `
    <div class="card-hdr">
      <span class="card-name" id="cname-${s.id}">${esc(s.name)}</span>
      <span class="card-rom">${esc(s.rom)}</span>
    </div>
    <div class="temp-big ok" id="ctemp-${s.id}">--<span class="unit">°C</span></div>
    <div class="stats">
      <span>Min: <b id="cmin-${s.id}">--</b>°C</span>
      <span>Max: <b id="cmax-${s.id}">--</b>°C</span>
      <span>Avg: <b id="cavg-${s.id}">--</b>°C</span>
    </div>
    <div class="chart-wrap">
      <canvas class="chart" id="cv-${s.id}" height="80"></canvas>
    </div>
    <div class="alarm-row">
      <label>Lo</label>
      <input type="number" id="alo-${s.id}" value="${s.lo}" step="1" style="width:58px">
      <label>Hi</label>
      <input type="number" id="ahi-${s.id}" value="${s.hi}" step="1" style="width:58px">
      <button class="btn" onclick="setAlarm(${s.id})">Set</button>
    </div>`;
}

function updateCard(s, idx) {
  const card = cardEls[s.id];
  if (!card) return;
  const isAlarm = s.alarm;
  card.classList.toggle('alarm', isAlarm);

  const tc = document.getElementById('ctemp-' + s.id);
  if (tc) {
    tc.className = 'temp-big ' + tempClass(s.tempC, s.lo, s.hi);
    tc.innerHTML = s.tempC.toFixed(1) + '<span class="unit">°C</span>';
  }
  const setText = (id, val) => { const el = document.getElementById(id); if(el) el.textContent = val; };
  setText('cmin-'+s.id, isFinite(s.min) ? s.min.toFixed(1) : '--');
  setText('cmax-'+s.id, isFinite(s.max) ? s.max.toFixed(1) : '--');
  setText('cavg-'+s.id, isFinite(s.avg) ? s.avg.toFixed(1) : '--');
  setText('cname-'+s.id, s.name);

  // Only update alarm inputs if user is not focused on them
  const aloEl = document.getElementById('alo-'+s.id);
  const ahiEl = document.getElementById('ahi-'+s.id);
  if (aloEl && document.activeElement !== aloEl) aloEl.value = s.lo;
  if (ahiEl && document.activeElement !== ahiEl) ahiEl.value = s.hi;
}

// ── Chart drawing ───────────────────────────────────────────
function drawChart(id, colorIdx) {
  const cv = document.getElementById('cv-' + id);
  if (!cv) return;
  const W = cv.parentElement.clientWidth - 16 || 200;
  cv.width  = W;
  cv.height = 80;
  const ct = cv.getContext('2d');
  const H  = cv.height;
  const data = getHistory(id).filter(v => isFinite(v));
  if (data.length < 2) return;

  const lo = Math.min(...data) - 1, hi = Math.max(...data) + 1;
  const range = Math.max(hi - lo, 2);

  ct.clearRect(0, 0, W, H);

  // Grid
  ct.strokeStyle = 'rgba(255,255,255,0.05)';
  ct.lineWidth = 1;
  for (let g = 0; g <= 4; g++) {
    const y = (g / 4) * H;
    ct.beginPath(); ct.moveTo(0, y); ct.lineTo(W, y); ct.stroke();
  }

  const all = getHistory(id);
  const n = all.length;
  const color = PALETTE[colorIdx % PALETTE.length];

  // Gradient fill
  const grad = ct.createLinearGradient(0, 0, 0, H);
  grad.addColorStop(0, color.replace(')', ',0.55)').replace('rgb','rgba').replace('#', 'rgba(') || color + '88');
  grad.addColorStop(1, 'rgba(0,0,0,0)');
  // Safer gradient
  ct.fillStyle = color + '33';

  ct.beginPath();
  let started = false;
  let firstX = 0, lastX = 0;
  for (let i = 0; i < n; i++) {
    const v = all[i];
    if (!isFinite(v)) { started = false; continue; }
    const x = (i / (n-1)) * W;
    const y = H - ((v - lo) / range) * H;
    if (!started) { ct.moveTo(x, H); ct.lineTo(x, y); firstX = x; started = true; }
    else ct.lineTo(x, y);
    lastX = x;
  }
  if (started) { ct.lineTo(lastX, H); ct.closePath(); ct.fill(); }

  // Line
  ct.beginPath();
  ct.strokeStyle = color;
  ct.lineWidth = 1.5;
  started = false;
  for (let i = 0; i < n; i++) {
    const v = all[i];
    if (!isFinite(v)) { started = false; continue; }
    const x = (i / (n-1)) * W;
    const y = H - ((v - lo) / range) * H;
    started ? ct.lineTo(x, y) : ct.moveTo(x, y);
    started = true;
  }
  ct.stroke();

  // Latest dot
  if (data.length) {
    const lastValid = all.slice().reverse().find(v => isFinite(v));
    if (lastValid !== undefined) {
      const li = all.length - 1 - [...all].reverse().findIndex(v => isFinite(v));
      const x = (li / (n-1)) * W;
      const y = H - ((lastValid - lo) / range) * H;
      ct.beginPath();
      ct.arc(x, y, 3, 0, Math.PI*2);
      ct.fillStyle = color;
      ct.fill();
    }
  }

  // Y-axis labels
  ct.fillStyle = 'rgba(200,220,240,0.4)';
  ct.font = '8px Courier New';
  ct.fillText(hi.toFixed(1)+'°', 2, 9);
  ct.fillText(lo.toFixed(1)+'°', 2, H-2);
}

// ── Alarm command ───────────────────────────────────────────
function setAlarm(id) {
  const lo = parseFloat(document.getElementById('alo-'+id).value);
  const hi = parseFloat(document.getElementById('ahi-'+id).value);
  if (isNaN(lo) || isNaN(hi)) { wifiMsg('Invalid alarm values','err'); return; }
  if (lo >= hi) { wifiMsg('Lo must be < Hi','err'); return; }
  send({c:'setAlarm', id, lo: Math.round(lo), hi: Math.round(hi)});
  log('Alarm set for sensor ' + id + ': Lo=' + lo + ' Hi=' + hi, 'ok');
}

// ── Status bar ──────────────────────────────────────────────
function updateStatusBar(d) {
  const setText = (id, v) => { const el = document.getElementById(id); if(el) el.textContent = v; };
  setText('bSens', d.sensors ? d.sensors.length : 0);
  if (d.power) setText('bPwr', d.power === 'par' ? '⚡ Parasite' : '🔌 External');
  if (d.res)   setText('bRes', d.res);
  if (d.interval !== undefined) setText('bInt', d.interval);
}

// ── Time clock ──────────────────────────────────────────────
function updateClock() {
  const el = document.getElementById('bTime');
  if (el) el.textContent = new Date().toLocaleTimeString();
}
setInterval(updateClock, 1000);

// ── Event log ───────────────────────────────────────────────
function log(msg, cls) {
  const el = document.getElementById('logEl');
  const ts = new Date().toLocaleTimeString();
  const div = document.createElement('div');
  div.innerHTML = `<span class="ts">[${ts}]</span> <span class="${cls||''}">${esc(msg)}</span>`;
  el.appendChild(div);
  el.scrollTop = el.scrollHeight;
  // Keep max 200 lines
  while (el.children.length > 200) el.removeChild(el.firstChild);
}
function clearLog() { document.getElementById('logEl').innerHTML = ''; }

// ── WebSocket ────────────────────────────────────────────────
let ws, retryTimer;

function wsConnect() {
  if (ws) { try { ws.close(); } catch(_){} }
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onopen  = () => { setConn(true); send({c:'status'}); log('Connected','ok'); };
  ws.onclose = () => { setConn(false); log('Disconnected – retrying…','err'); clearTimeout(retryTimer); retryTimer = setTimeout(wsConnect, 3000); };
  ws.onerror = () => ws.close();
  ws.onmessage = ({data}) => {
    let msg;
    try { msg = JSON.parse(data); } catch(_) { return; }

    if (msg.t === 'data') {
      updateStatusBar(msg);
      renderAllSensors(msg.sensors || []);
      document.getElementById('bSens').textContent = (msg.sensors||[]).length;

    } else if (msg.t === 'alarm') {
      log(`🚨 ALARM: ${msg.name} = ${msg.tempC.toFixed(1)}°C (Lo=${msg.lo} Hi=${msg.hi})`, 'err');
      if (Notification && Notification.permission === 'granted')
        new Notification('DS18B20 Alarm', {body: `${msg.name}: ${msg.tempC.toFixed(1)}°C`});

    } else if (msg.t === 'w') {
      renderNetworks(msg.n);

    } else if (msg.t === 'st') {
      document.getElementById('bMode').textContent = msg.m === 'ap' ? '📡 AP' : '🌐 STA';
      document.getElementById('bIP').textContent   = msg.ip || '--';
      if (msg.m === 'ap' && msg.ss) document.getElementById('apSSID').value = msg.ss;

    } else if (msg.t === 'msg') {
      wifiMsg(msg.m, msg.ok ? 'ok' : 'err');
      log(msg.m, msg.ok ? 'ok' : 'err');
    }
  };
}

function setConn(ok) {
  document.getElementById('dot').className = 'dot' + (ok ? ' on' : '');
  document.getElementById('connTxt').textContent = ok ? 'Live' : 'Disconnected';
}
function send(obj) { if(ws && ws.readyState===1) ws.send(JSON.stringify(obj)); }

// ── WiFi panel ───────────────────────────────────────────────
function toggleWP() {
  const wp = document.getElementById('wifiPanel');
  const col = wp.classList.toggle('collapsed');
  document.getElementById('wpArrow').textContent = col ? '▼' : '▲';
}
let curTab = 'ap';
function switchTab(tab) {
  curTab = tab;
  document.getElementById('tabAP').classList.toggle('act',  tab==='ap');
  document.getElementById('tabSTA').classList.toggle('act', tab==='sta');
  document.getElementById('panelAP').style.display  = tab==='ap'  ? '' : 'none';
  document.getElementById('panelSTA').style.display = tab==='sta' ? '' : 'none';
}
function applyAP() {
  const s = document.getElementById('apSSID').value.trim();
  const p = document.getElementById('apPass').value;
  if (!s) { wifiMsg('AP name is required','err'); return; }
  if (p && p.length < 8) { wifiMsg('Password must be ≥8 chars','err'); return; }
  send({c:'ap', s, p}); wifiMsg('Applying…','info');
}
function doScan() { send({c:'scan'}); wifiMsg('Scanning…','info'); }
function renderNetworks(nets) {
  const el = document.getElementById('netList');
  if (!nets || !nets.length) { el.innerHTML = '<div style="color:var(--dim);padding:4px">No networks found</div>'; return; }
  el.innerHTML = nets.map(n=>`
    <div class="net" onclick="pickNet(this,'${esc(n.s)}')">
      <span class="net-s">${esc(n.s)}</span>
      <span class="net-r">${n.r}dBm</span>
      <span>${n.e?'🔒':'🔓'}</span>
    </div>`).join('');
  wifiMsg(`Found ${nets.length} network(s)`,'ok');
}
function pickNet(el, ssid) {
  document.getElementById('staSSID').value = ssid;
  document.querySelectorAll('.net').forEach(e=>e.classList.toggle('sel',e===el));
}
function doConnect() {
  const s = document.getElementById('staSSID').value.trim();
  const p = document.getElementById('staPass').value;
  if (!s) { wifiMsg('SSID required','err'); return; }
  send({c:'sta', s, p}); wifiMsg('Connecting to "'+esc(s)+'"…','info');
}
function wifiMsg(txt, type) {
  const el = document.getElementById('wMsg');
  el.textContent = txt; el.className = 'wmsg '+type; el.style.display='';
}

function esc(s) {
  return String(s)
    .replace(/&/g,'&amp;').replace(/</g,'&lt;')
    .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

// ── Notification permission ──────────────────────────────────
if (typeof Notification !== 'undefined' && Notification.permission === 'default')
  Notification.requestPermission();

// ── Boot ─────────────────────────────────────────────────────
window.addEventListener('load', wsConnect);
window.addEventListener('resize', () => {
  Object.keys(cardEls).forEach((id,i) => drawChart(id,i));
});
</script>
</body>
</html>
)HTMLEOF";
