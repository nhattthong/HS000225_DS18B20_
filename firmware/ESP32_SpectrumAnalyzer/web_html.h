#pragma once
// ============================================================
// web_html.h  –  Full single-page spectrum-analyser UI
// Stored in flash (PROGMEM).  Served as text/html.
// ============================================================
#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"HTMLEOF(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>RF Spectrum Analyzer</title>
<style>
:root{--bg:#080810;--panel:#0e0e1c;--border:#1c2436;--acc:#00d4ff;--acc2:#00ff88;--warn:#ff6b35;--txt:#b0c0d0;--dim:#334455}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--txt);font-family:'Courier New',monospace;font-size:13px;overflow-x:hidden}
/* ── Header ── */
.hdr{display:flex;align-items:center;justify-content:space-between;padding:7px 14px;background:var(--panel);border-bottom:1px solid var(--border)}
.hdr h1{color:var(--acc);font-size:15px;letter-spacing:1px}
.badge{font-size:11px;color:var(--dim);margin-left:14px}
.badge b{color:var(--acc)}
.dot{width:8px;height:8px;border-radius:50%;background:#222;display:inline-block;margin-right:5px;transition:background .4s}
.dot.on{background:var(--acc2);box-shadow:0 0 6px var(--acc2)}
/* ── Control bar ── */
.ctrl{display:flex;flex-wrap:wrap;gap:6px;padding:6px 14px;background:var(--panel);border-bottom:1px solid var(--border);align-items:center}
.btn{padding:3px 11px;border:1px solid var(--border);background:transparent;color:var(--txt);cursor:pointer;border-radius:3px;font-size:11px;font-family:inherit;transition:all .15s}
.btn:hover,.btn.act{border-color:var(--acc);color:var(--acc);background:rgba(0,212,255,.08)}
.sep{width:1px;height:18px;background:var(--border);margin:0 3px}
.fps{font-size:10px;color:var(--dim)}
.fps b{color:var(--acc)}
/* ── Gradient legend ── */
.legend{display:flex;align-items:center;gap:5px;font-size:10px;color:var(--dim)}
.lgrad{width:100px;height:7px;border-radius:2px;background:linear-gradient(to right,#00003e,#0000ff,#00ffff,#00ff00,#ffff00,#ff0000)}
/* ── Display panels ── */
.disp{padding:8px 8px 0}
.band-hdr{font-size:10px;padding:4px 0 3px;letter-spacing:.5px}
.band-hdr.b24{color:var(--acc)}
.band-hdr.b58{color:var(--warn)}
.cw{position:relative;background:#04040c;border:1px solid var(--border);border-radius:3px;overflow:hidden;margin-bottom:5px}
.clabel{position:absolute;top:3px;left:6px;font-size:9px;color:var(--dim);pointer-events:none;z-index:2}
canvas{display:block;width:100%}
.fax{display:flex;justify-content:space-between;padding:1px 4px 4px;font-size:9px;color:var(--dim)}
/* ── WiFi panel ── */
.wp{margin:0 8px 10px;border:1px solid var(--border);border-radius:4px;overflow:hidden}
.wphdr{display:flex;align-items:center;justify-content:space-between;padding:8px 12px;background:rgba(0,212,255,.04);border-bottom:1px solid var(--border);cursor:pointer;user-select:none}
.wphdr h3{font-size:12px;color:var(--acc)}
.wpbody{padding:12px}
.collapsed .wpbody{display:none}
.tabs{display:flex;gap:5px;margin-bottom:11px}
.tab{flex:1;padding:5px;text-align:center;border:1px solid var(--border);border-radius:3px;cursor:pointer;font-size:11px;transition:all .15s}
.tab.act{border-color:var(--acc);background:rgba(0,212,255,.08);color:var(--acc)}
.row{display:flex;align-items:center;gap:8px;margin-bottom:8px}
.row label{min-width:62px;font-size:11px;color:var(--dim)}
input,select{flex:1;background:#06060f;border:1px solid var(--border);color:var(--txt);padding:5px 8px;border-radius:3px;font-family:inherit;font-size:12px}
input:focus,select:focus{outline:none;border-color:var(--acc)}
select option{background:#06060f}
.netlist{margin:6px 0 10px}
.net{display:flex;align-items:center;padding:4px 8px;border:1px solid transparent;border-radius:3px;cursor:pointer;font-size:11px;transition:all .1s}
.net:hover,.net.sel{border-color:var(--border);background:rgba(255,255,255,.03)}
.net-s{flex:1}
.net-r{color:var(--dim);font-size:10px;margin-right:6px}
.net-l{font-size:10px}
.wmsg{padding:5px 8px;border-radius:3px;font-size:11px;margin-top:8px;display:none}
.wmsg.ok{background:rgba(0,255,136,.08);color:var(--acc2)}
.wmsg.err{background:rgba(255,107,53,.1);color:var(--warn)}
.wmsg.info{background:rgba(0,212,255,.08);color:var(--acc)}
</style>
</head>
<body>

<!-- ── Header ─────────────────────────────── -->
<div class="hdr">
  <div style="display:flex;align-items:center">
    <h1>⚡ RF SPECTRUM ANALYZER</h1>
    <span class="badge">Mode: <b id="bMode">--</b></span>
    <span class="badge">IP: <b id="bIP">--</b></span>
  </div>
  <div style="display:flex;align-items:center;gap:4px">
    <span class="dot" id="dot"></span>
    <span id="connTxt" style="font-size:11px;color:var(--dim)">Connecting…</span>
  </div>
</div>

<!-- ── Control bar ────────────────────────── -->
<div class="ctrl">
  <span style="font-size:10px;color:var(--dim)">BAND:</span>
  <button class="btn act" id="btnAll"  onclick="setBand(0)">BOTH</button>
  <button class="btn"     id="btn24"   onclick="setBand(1)">2.4 GHz</button>
  <button class="btn"     id="btn58"   onclick="setBand(2)">5.8 GHz</button>
  <div class="sep"></div>
  <span class="fps">nRF24: <b id="fps24">--</b> fps</span>
  <span class="fps">RX5808: <b id="fps58">--</b> fps</span>
  <div class="sep"></div>
  <div class="legend"><span>Weak</span><div class="lgrad"></div><span>Strong</span></div>
</div>

<!-- ── Display area ───────────────────────── -->
<div class="disp">

  <!-- 2.4 GHz -->
  <div id="sec24">
    <div class="band-hdr b24">▶ 2.4 GHz  (nRF24L01 — ch 0-127 = 2400-2527 MHz)</div>
    <div class="cw" style="height:90px">
      <div class="clabel">SPECTRUM</div>
      <canvas id="sp24" height="90"></canvas>
    </div>
    <div class="cw" style="height:180px">
      <div class="clabel">WATERFALL  ↑ older → newer ↓</div>
      <canvas id="wf24" height="180"></canvas>
    </div>
    <div class="fax" id="fax24"></div>
  </div>

  <!-- 5.8 GHz -->
  <div id="sec58">
    <div class="band-hdr b58">▶ 5.8 GHz  (RX5808 — 40 ch = 5645-5945 MHz)</div>
    <div class="cw" style="height:90px">
      <div class="clabel">SPECTRUM</div>
      <canvas id="sp58" height="90"></canvas>
    </div>
    <div class="cw" style="height:180px">
      <div class="clabel">WATERFALL  ↑ older → newer ↓</div>
      <canvas id="wf58" height="180"></canvas>
    </div>
    <div class="fax" id="fax58"></div>
  </div>

</div><!-- /disp -->

<!-- ── WiFi panel ─────────────────────────── -->
<div class="wp" id="wifiPanel">
  <div class="wphdr" onclick="toggleWP()">
    <h3>⚙ WiFi Configuration</h3>
    <span id="wpArrow" style="font-size:10px">▲</span>
  </div>
  <div class="wpbody">
    <div class="tabs">
      <div class="tab act" id="tabAP"  onclick="switchTab('ap')">📡 Access Point</div>
      <div class="tab"     id="tabSTA" onclick="switchTab('sta')">🌐 Station (client)</div>
    </div>

    <!-- AP panel -->
    <div id="panelAP">
      <div class="row"><label>AP Name</label><input id="apSSID" type="text" placeholder="SpectrumAnalyzer"></div>
      <div class="row"><label>Password</label><input id="apPass" type="password" placeholder="min 8 chars (blank = open)"></div>
      <button class="btn" onclick="applyAP()" style="width:100%">Apply &amp; Save</button>
    </div>

    <!-- STA panel -->
    <div id="panelSTA" style="display:none">
      <button class="btn" onclick="doScan()" style="margin-bottom:8px">🔍 Scan Networks</button>
      <div class="netlist" id="netList"></div>
      <div class="row"><label>SSID</label><input id="staSSID" type="text" placeholder="or type manually"></div>
      <div class="row"><label>Password</label><input id="staPass" type="password" placeholder="WiFi password"></div>
      <button class="btn" onclick="doConnect()" style="width:100%">Connect</button>
    </div>

    <div class="wmsg" id="wMsg"></div>
  </div>
</div>

<script>
// ============================================================
//  RF Spectrum Analyzer  –  Web UI
//  WebSocket protocol (JSON):
//    ESP32 → Browser:
//      {"t":"s","b":0,"d":[...128]}        2.4 GHz scan
//      {"t":"s","b":1,"f":[...40],"d":[...40]}  5.8 GHz scan
//      {"t":"w","n":[{s,r,e},...]}          WiFi scan result
//      {"t":"st","m":"ap|sta","ip":"...","ss":"..."}
//      {"t":"msg","m":"...","ok":true|false}
//    Browser → ESP32:
//      {"c":"status"}
//      {"c":"band","b":0|1|2}
//      {"c":"scan"}
//      {"c":"ap","s":"ssid","p":"pass"}
//      {"c":"sta","s":"ssid","p":"pass"}
// ============================================================

// ── Canvas references ──────────────────────────────────────
const C = (id) => document.getElementById(id);
let ctx = {};   // canvas 2d contexts
let wfd = {};   // waterfall ImageData objects

// ── Colour LUT  (0-100 → RGB) ──────────────────────────────
const LUT = new Uint8Array(101 * 3);
(function () {
  const stops = [
    [0,   [0,   0,   60]],
    [12,  [0,   0,   220]],
    [30,  [0,   120, 255]],
    [48,  [0,   255, 220]],
    [62,  [0,   255, 0]],
    [76,  [255, 255, 0]],
    [88,  [255, 100, 0]],
    [100, [255, 0,   0]],
  ];
  for (let v = 0; v <= 100; v++) {
    let lo = stops[0], hi = stops[1];
    for (let i = 0; i < stops.length - 1; i++) {
      if (v >= stops[i][0] && v <= stops[i+1][0]) { lo = stops[i]; hi = stops[i+1]; break; }
    }
    const t = (hi[0] === lo[0]) ? 0 : (v - lo[0]) / (hi[0] - lo[0]);
    LUT[v*3]   = lo[1][0] + t * (hi[1][0] - lo[1][0]);
    LUT[v*3+1] = lo[1][1] + t * (hi[1][1] - lo[1][1]);
    LUT[v*3+2] = lo[1][2] + t * (hi[1][2] - lo[1][2]);
  }
})();

function col(v, a) {
  v = Math.min(100, Math.max(0, Math.round(v)));
  return a !== undefined
    ? `rgba(${LUT[v*3]},${LUT[v*3+1]},${LUT[v*3+2]},${a})`
    : `rgb(${LUT[v*3]},${LUT[v*3+1]},${LUT[v*3+2]})`;
}

// ── Canvas init ────────────────────────────────────────────
function initCanvases() {
  ['sp24','sp58','wf24','wf58'].forEach(id => {
    const c = C(id);
    c.width = c.parentElement.clientWidth || 700;
  });
  C('sp24').height = 90;  C('sp58').height = 90;
  C('wf24').height = 180; C('wf58').height = 180;

  ['sp24','sp58','wf24','wf58'].forEach(id => {
    ctx[id] = C(id).getContext('2d');
  });

  ['24','58'].forEach(b => {
    const c = C('wf' + b);
    wfd[b] = ctx['wf' + b].createImageData(c.width, c.height);
    wfd[b].data.fill(0);
    for (let i = 3; i < wfd[b].data.length; i += 4) wfd[b].data[i] = 255;
  });

  buildFreqAxis('fax24',
    [2400,2410,2420,2430,2440,2450,2460,2470,2480,2490,2500,2510,2520,2527]);
  buildFreqAxis('fax58',
    [5645,5680,5720,5760,5800,5840,5880,5920,5945]);
}

function buildFreqAxis(id, freqs) {
  C(id).innerHTML = freqs.map(f => `<span>${f}</span>`).join('');
}

// ── Spectrum draw ──────────────────────────────────────────
function drawSpectrum(ctxId, data) {
  const ct = ctx[ctxId];
  const W = ct.canvas.width, H = ct.canvas.height;
  ct.clearRect(0, 0, W, H);
  // Grid
  ct.strokeStyle = '#0b0b18'; ct.lineWidth = 1;
  for (let dB = 0; dB <= 100; dB += 25) {
    const y = H - (dB / 100) * H;
    ct.beginPath(); ct.moveTo(0, y); ct.lineTo(W, y); ct.stroke();
  }
  // dB labels
  ct.fillStyle = '#1a1f33'; ct.font = '8px Courier New';
  [0,25,50,75,100].forEach(dB => {
    ct.fillText(dB + '%', 2, H - (dB / 100) * H - 2);
  });
  const n = data.length;
  const bw = W / n;
  for (let i = 0; i < n; i++) {
    const v = data[i] || 0;
    const h = (v / 100) * H;
    ct.fillStyle = col(v);
    ct.fillRect(i * bw, H - h, Math.max(1, bw), h);
  }
  // Peak line
  ct.beginPath(); ct.strokeStyle = 'rgba(255,255,255,.2)'; ct.lineWidth = 1;
  for (let i = 0; i < n; i++) {
    const x = (i + 0.5) * bw, y = H - ((data[i] || 0) / 100) * H;
    i === 0 ? ct.moveTo(x, y) : ct.lineTo(x, y);
  }
  ct.stroke();
}

// ── Waterfall row ──────────────────────────────────────────
function addWFRow(band, data) {
  const id = band === 0 ? '24' : '58';
  const ct = ctx['wf' + id];
  const W = ct.canvas.width, H = ct.canvas.height;
  const d = wfd[id].data;
  const n = data.length;
  // Shift existing rows down by 1
  d.copyWithin(W * 4, 0, (H - 1) * W * 4);
  // Paint new top row
  for (let px = 0; px < W; px++) {
    const ci = Math.min(n - 1, Math.floor(px * n / W));
    const v  = Math.min(100, Math.max(0, data[ci] || 0));
    const o  = px * 4;
    d[o]   = LUT[v*3];
    d[o+1] = LUT[v*3+1];
    d[o+2] = LUT[v*3+2];
    d[o+3] = 255;
  }
  ct.putImageData(wfd[id], 0, 0);
}

// ── FPS ────────────────────────────────────────────────────
let fCnt = {0:0, 1:0}, fTime = Date.now();
function tickFPS(band) {
  fCnt[band]++;
  const now = Date.now(), dt = now - fTime;
  if (dt >= 1000) {
    C('fps24').textContent = Math.round(fCnt[0] / dt * 1000);
    C('fps58').textContent = Math.round(fCnt[1] / dt * 1000);
    fCnt[0] = fCnt[1] = 0;
    fTime = now;
  }
}

// ── Band select ────────────────────────────────────────────
let activeBand = 0;
function setBand(b) {
  activeBand = b;
  ['btnAll','btn24','btn58'].forEach((id, i) =>
    C(id).classList.toggle('act', i === b));
  C('sec24').style.display = (b === 2) ? 'none' : '';
  C('sec58').style.display = (b === 1) ? 'none' : '';
  send({c:'band', b});
}

// ── WebSocket ──────────────────────────────────────────────
let ws, retryTimer;

function wsConnect() {
  if (ws) { try { ws.close(); } catch(_) {} }
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onopen  = () => { setConn(true);  send({c:'status'}); };
  ws.onclose = () => { setConn(false); clearTimeout(retryTimer); retryTimer = setTimeout(wsConnect, 3000); };
  ws.onerror = () => ws.close();
  ws.onmessage = ({ data }) => {
    let msg;
    try { msg = JSON.parse(data); } catch(_) { return; }
    if (msg.t === 's') {
      const d = msg.d;
      if (msg.b === 0) {
        drawSpectrum('sp24', d); addWFRow(0, d);
      } else {
        drawSpectrum('sp58', d); addWFRow(1, d);
      }
      tickFPS(msg.b);
    } else if (msg.t === 'w') {
      renderNetworks(msg.n);
    } else if (msg.t === 'st') {
      C('bMode').textContent = msg.m === 'ap' ? '📡 AP' : '🌐 STA';
      C('bIP').textContent   = msg.ip || '--';
      if (msg.m === 'ap' && msg.ss) C('apSSID').value = msg.ss;
    } else if (msg.t === 'msg') {
      wifiMsg(msg.m, msg.ok ? 'ok' : 'err');
    }
  };
}

function setConn(ok) {
  C('dot').className  = 'dot' + (ok ? ' on' : '');
  C('connTxt').textContent = ok ? 'Live' : 'Disconnected';
}

function send(obj) {
  if (ws && ws.readyState === 1) ws.send(JSON.stringify(obj));
}

// ── WiFi panel helpers ─────────────────────────────────────
let wpOpen = true;
function toggleWP() {
  wpOpen = !wpOpen;
  C('wifiPanel').classList.toggle('collapsed', !wpOpen);
  C('wpArrow').textContent = wpOpen ? '▲' : '▼';
}

let curTab = 'ap';
function switchTab(tab) {
  curTab = tab;
  C('tabAP').classList.toggle('act',  tab === 'ap');
  C('tabSTA').classList.toggle('act', tab === 'sta');
  C('panelAP').style.display  = tab === 'ap'  ? '' : 'none';
  C('panelSTA').style.display = tab === 'sta' ? '' : 'none';
}

function applyAP() {
  const s = C('apSSID').value.trim(), p = C('apPass').value;
  if (!s) { wifiMsg('AP name is required', 'err'); return; }
  if (p && p.length < 8) { wifiMsg('Password must be ≥ 8 chars', 'err'); return; }
  send({c:'ap', s, p});
  wifiMsg('Applying AP settings…', 'info');
}

function doScan() {
  send({c:'scan'});
  wifiMsg('Scanning for networks…', 'info');
}

let selNet = '';
function renderNetworks(nets) {
  const el = C('netList');
  if (!nets || !nets.length) {
    el.innerHTML = '<div style="color:var(--dim);font-size:11px;padding:4px">No networks found</div>';
    wifiMsg('Scan complete – none found', 'info');
    return;
  }
  el.innerHTML = nets.map(n => `
    <div class="net" onclick="pickNet(this,'${esc(n.s)}')">
      <span class="net-s">${esc(n.s)}</span>
      <span class="net-r">${n.r} dBm</span>
      <span class="net-l">${n.e ? '🔒' : '🔓'}</span>
    </div>`).join('');
  wifiMsg(`Found ${nets.length} network(s)`, 'ok');
}

function pickNet(el, ssid) {
  selNet = ssid;
  C('staSSID').value = ssid;
  document.querySelectorAll('.net').forEach(e => e.classList.toggle('sel', e === el));
}

function doConnect() {
  const s = C('staSSID').value.trim(), p = C('staPass').value;
  if (!s) { wifiMsg('SSID is required', 'err'); return; }
  send({c:'sta', s, p});
  wifiMsg('Connecting to "' + esc(s) + '"…', 'info');
}

function wifiMsg(txt, type) {
  const el = C('wMsg');
  el.textContent = txt;
  el.className = 'wmsg ' + type;
  el.style.display = '';
}

function esc(s) {
  return String(s)
    .replace(/&/g,'&amp;').replace(/</g,'&lt;')
    .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

// ── Bootstrap ──────────────────────────────────────────────
window.addEventListener('load', () => {
  initCanvases();
  wsConnect();
});
window.addEventListener('resize', initCanvases);
</script>
</body>
</html>
)HTMLEOF";
