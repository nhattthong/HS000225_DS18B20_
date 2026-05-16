#pragma once
// ============================================================
// web_html.h  –  RF Spectrum Analyzer Web UI  (v2)
// Stored in flash (PROGMEM).  Served as text/html.
//
// Improvements over v1:
//   • Waterfall newest-at-bottom / scrolls upward (standard)
//   • Time axis with -Ns labels on left edge of waterfall
//   • Linear interpolation between scan channels (smooth)
//   • Gradient fill under spectrum curve (Karogic-style)
//   • Peak-hold line (yellow, slow decay)
//   • Frequency crosshair tooltip on mouse hover
//   • Responsive: two-column on wide screens
//   • WiFi panel starts collapsed
//   • Larger waterfall (260 px)
// ============================================================
#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"HTMLEOF(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>RF Spectrum Analyzer</title>
<style>
:root{--bg:#060608;--panel:#0c0c14;--border:#1a2030;--acc:#00d4ff;--acc2:#00ff88;--warn:#ff6b35;--txt:#a0b0c0;--dim:#2a3a4a;--grid:rgba(255,255,255,.04)}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--txt);font-family:'Courier New',monospace;font-size:12px;overflow-x:hidden;height:100vh;display:flex;flex-direction:column}
.hdr{display:flex;align-items:center;justify-content:space-between;padding:6px 12px;background:var(--panel);border-bottom:1px solid var(--border);flex-shrink:0}
.hdr h1{color:var(--acc);font-size:13px;letter-spacing:2px}
.hdr-l{display:flex;align-items:center;gap:12px}
.badge{font-size:10px;color:var(--dim)}.badge b{color:var(--acc)}
.dot{width:7px;height:7px;border-radius:50%;background:#333;display:inline-block;margin-right:4px;transition:background .4s}
.dot.on{background:var(--acc2);box-shadow:0 0 5px var(--acc2)}
.ctrl{display:flex;flex-wrap:wrap;align-items:center;gap:5px;padding:4px 12px;background:var(--panel);border-bottom:1px solid var(--border);flex-shrink:0}
.btn{padding:2px 10px;border:1px solid var(--border);background:transparent;color:var(--txt);cursor:pointer;border-radius:2px;font-size:10px;font-family:inherit;transition:all .15s}
.btn:hover,.btn.act{border-color:var(--acc);color:var(--acc);background:rgba(0,212,255,.08)}
.sep{width:1px;height:16px;background:var(--border);margin:0 2px}
.stat{font-size:10px;color:var(--dim)}.stat b{color:var(--acc)}
.lgrad{width:80px;height:6px;border-radius:1px;background:linear-gradient(to right,#02020f,#0000cc,#00ccff,#00ff44,#ffff00,#ff0000)}
.main{flex:1;overflow-y:auto;padding:6px;display:flex;flex-direction:column;gap:6px}
@media(min-width:900px){.main{flex-direction:row}}
.bp{flex:1;display:flex;flex-direction:column;min-width:0}
.bhdr{font-size:10px;padding:3px 4px;letter-spacing:.5px;display:flex;justify-content:space-between;align-items:center}
.b24{color:var(--acc)}.b58{color:var(--warn)}
.pi b{color:#ffff00}
.spwf{position:relative;border:1px solid var(--border);border-radius:3px;overflow:hidden;background:#03030a}
.spwf canvas{display:block;width:100%}
.sp-canvas{border-bottom:1px solid var(--border)}
.ch{position:absolute;top:0;left:0;right:0;bottom:0;pointer-events:none;display:none;z-index:5}
.chv{position:absolute;top:0;bottom:0;width:1px;background:rgba(255,255,255,.35)}
.chh{position:absolute;left:0;right:0;height:1px;background:rgba(255,255,255,.25)}
.cht{position:absolute;background:rgba(0,0,0,.85);border:1px solid var(--acc);border-radius:2px;padding:2px 6px;font-size:10px;color:var(--acc);white-space:nowrap}
.fax{display:flex;justify-content:space-between;padding:2px 0;font-size:9px;color:var(--dim)}
.wp{border:1px solid var(--border);border-radius:3px;overflow:hidden;flex-shrink:0;margin:0 0 2px}
.wphdr{display:flex;align-items:center;justify-content:space-between;padding:6px 10px;background:rgba(0,212,255,.03);border-bottom:1px solid var(--border);cursor:pointer;user-select:none}
.wphdr h3{font-size:11px;color:var(--acc)}
.wpbody{padding:10px;display:none}
.wp.open .wpbody{display:block}
.tabs{display:flex;gap:4px;margin-bottom:10px}
.tab{flex:1;padding:4px;text-align:center;border:1px solid var(--border);border-radius:2px;cursor:pointer;font-size:10px;transition:all .15s}
.tab.act{border-color:var(--acc);color:var(--acc);background:rgba(0,212,255,.08)}
.row{display:flex;align-items:center;gap:6px;margin-bottom:7px}
.row label{min-width:58px;font-size:10px;color:var(--dim)}
input,select{flex:1;background:#05050e;border:1px solid var(--border);color:var(--txt);padding:4px 7px;border-radius:2px;font-family:inherit;font-size:11px}
input:focus,select:focus{outline:none;border-color:var(--acc)}
select option{background:#05050e}
.netlist{margin:4px 0 8px;max-height:130px;overflow-y:auto}
.net{display:flex;align-items:center;padding:3px 6px;border:1px solid transparent;border-radius:2px;cursor:pointer;font-size:10px;transition:all .1s}
.net:hover,.net.sel{border-color:var(--border);background:rgba(255,255,255,.03)}
.net-s{flex:1}.net-r{color:var(--dim);font-size:9px;margin-right:4px}
.wmsg{padding:4px 7px;border-radius:2px;font-size:10px;margin-top:6px;display:none}
.wmsg.ok{background:rgba(0,255,136,.08);color:var(--acc2)}
.wmsg.err{background:rgba(255,107,53,.1);color:var(--warn)}
.wmsg.info{background:rgba(0,212,255,.08);color:var(--acc)}
</style>
</head>
<body>

<div class="hdr">
  <div class="hdr-l">
    <h1>⚡ RF SPECTRUM</h1>
    <span class="badge">Mode: <b id="bMode">--</b></span>
    <span class="badge">IP: <b id="bIP">--</b></span>
  </div>
  <div style="display:flex;align-items:center;gap:4px">
    <span class="dot" id="dot"></span>
    <span id="connTxt" style="font-size:10px;color:var(--dim)">Connecting…</span>
  </div>
</div>

<div class="ctrl">
  <span class="stat" style="color:#444">BAND:</span>
  <button class="btn act" id="btnAll" onclick="setBand(0)">BOTH</button>
  <button class="btn" id="btn24" onclick="setBand(1)">2.4 GHz</button>
  <button class="btn" id="btn58" onclick="setBand(2)">5.8 GHz</button>
  <div class="sep"></div>
  <button class="btn" id="btnPeak" onclick="togglePeak()" title="Peak Hold">PEAK</button>
  <div class="sep"></div>
  <span class="stat">2.4: <b id="fps24">--</b>fps</span>
  <span class="stat">5.8: <b id="fps58">--</b>fps</span>
  <div class="sep"></div>
  <span class="stat" style="color:#444;font-size:9px">Weak</span>
  <div class="lgrad"></div>
  <span class="stat" style="color:#444;font-size:9px">Strong</span>
</div>

<div class="main" id="mainDisp">

  <!-- 2.4 GHz panel -->
  <div class="bp" id="sec24">
    <div class="bhdr b24">
      <span>▶ 2.4 GHz &nbsp;·&nbsp; nRF24L01 &nbsp;·&nbsp; 2400–2527 MHz</span>
      <span class="pi" id="pi24"></span>
    </div>
    <div class="spwf" id="spwf24">
      <canvas class="sp-canvas" id="sp24" height="80"></canvas>
      <canvas id="wf24" height="260"></canvas>
      <div class="ch" id="ch24">
        <div class="chv" id="chv24"></div>
        <div class="chh" id="chh24"></div>
        <div class="cht" id="cht24"></div>
      </div>
    </div>
    <div class="fax" id="fax24"></div>
  </div>

  <!-- 5.8 GHz panel -->
  <div class="bp" id="sec58">
    <div class="bhdr b58">
      <span>▶ 5.8 GHz &nbsp;·&nbsp; RX5808 &nbsp;·&nbsp; 5645–5945 MHz</span>
      <span class="pi" id="pi58"></span>
    </div>
    <div class="spwf" id="spwf58">
      <canvas class="sp-canvas" id="sp58" height="80"></canvas>
      <canvas id="wf58" height="260"></canvas>
      <div class="ch" id="ch58">
        <div class="chv" id="chv58"></div>
        <div class="chh" id="chh58"></div>
        <div class="cht" id="cht58"></div>
      </div>
    </div>
    <div class="fax" id="fax58"></div>
  </div>

</div>

<!-- WiFi panel (starts collapsed) -->
<div class="wp" id="wifiPanel" style="margin:0 6px 6px">
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
      <div class="row"><label>AP Name</label><input id="apSSID" type="text" placeholder="SpectrumAnalyzer"></div>
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

<script>
// ════════════════════════════════════════════════════════════
// RF Spectrum Analyzer  –  Web UI  (v2)
//
// WebSocket JSON protocol (unchanged from firmware):
//   ESP32→Browser
//     {t:"s", b:0, d:[...128]}          2.4 GHz scan
//     {t:"s", b:1, f:[...40], d:[...40]}  5.8 GHz scan
//     {t:"w", n:[{s,r,e},...]}           WiFi scan results
//     {t:"st", m:"ap|sta", ip:"...", ss:"..."}
//     {t:"msg", m:"...", ok:true|false}
//   Browser→ESP32
//     {c:"status"} {c:"band",b:0|1|2}
//     {c:"scan"}
//     {c:"ap",s:"ssid",p:"pass"}
//     {c:"sta",s:"ssid",p:"pass"}
// ════════════════════════════════════════════════════════════

const WF_H = 260;   // must match canvas height="260" above

// ── Colour LUT  0-100 → RGB ──────────────────────────────────
const LUT = new Uint8Array(101 * 3);
(function() {
  const stops = [
    [0,   [2,   2,   20]],
    [10,  [0,   0,   130]],
    [25,  [0,   60,  200]],
    [40,  [0,   190, 210]],
    [55,  [0,   230, 80]],
    [68,  [190, 255, 0]],
    [80,  [255, 200, 0]],
    [90,  [255, 80,  0]],
    [100, [255, 0,   0]],
  ];
  for (let v = 0; v <= 100; v++) {
    let lo = stops[0], hi = stops[stops.length-1];
    for (let i = 0; i < stops.length-1; i++) {
      if (v >= stops[i][0] && v <= stops[i+1][0]) { lo = stops[i]; hi = stops[i+1]; break; }
    }
    const t = hi[0]===lo[0] ? 0 : (v-lo[0])/(hi[0]-lo[0]);
    LUT[v*3]   = Math.round(lo[1][0] + t*(hi[1][0]-lo[1][0]));
    LUT[v*3+1] = Math.round(lo[1][1] + t*(hi[1][1]-lo[1][1]));
    LUT[v*3+2] = Math.round(lo[1][2] + t*(hi[1][2]-lo[1][2]));
  }
})();

function lutStr(v) {
  v = Math.min(100,Math.max(0,Math.round(v)));
  return `rgb(${LUT[v*3]},${LUT[v*3+1]},${LUT[v*3+2]})`;
}
function lutRGBA(v,a) {
  v = Math.min(100,Math.max(0,Math.round(v)));
  return `rgba(${LUT[v*3]},${LUT[v*3+1]},${LUT[v*3+2]},${a})`;
}

// ── State ─────────────────────────────────────────────────────
const $ = id => document.getElementById(id);
let ctx = {}, wfd = {};
let peakData = { '24': null, '58': null };
let peakHold = false;

// Scan timing (for time axis labels)
const SCAN_HIST = 10;
const scanTimes  = { '24': [], '58': [] };
const scanIntMs  = { '24': 1000, '58': 5000 };  // updated dynamically

// Band metadata
const BANDS = {
  '24': { fMin: 2400, fMax: 2527 },
  '58': { fMin: 5645, fMax: 5945 },
};

// ── Canvas initialisation ─────────────────────────────────────
function initCanvases() {
  ['24','58'].forEach(b => {
    const sp = $('sp'+b), wf = $('wf'+b);
    const W = sp.parentElement.clientWidth || 600;
    sp.width = wf.width = W;
    sp.height = 80; wf.height = WF_H;
    ctx['sp'+b] = sp.getContext('2d');
    ctx['wf'+b] = wf.getContext('2d');
    // (Re)create waterfall pixel buffer
    wfd[b] = ctx['wf'+b].createImageData(W, WF_H);
    // Fill opaque dark blue
    for (let i = 0; i < WF_H*W; i++) {
      wfd[b].data[i*4]   = LUT[0];
      wfd[b].data[i*4+1] = LUT[1];
      wfd[b].data[i*4+2] = LUT[2];
      wfd[b].data[i*4+3] = 255;
    }
    ctx['wf'+b].putImageData(wfd[b], 0, 0);
    peakData[b] = new Float32Array(b==='24' ? 128 : 40).fill(0);
  });
  buildFreqAxes();
  initCrosshairs();
}

function buildFreqAxes() {
  $('fax24').innerHTML = [2400,2420,2440,2450,2462,2480,2500,2527]
    .map(f=>`<span>${f}</span>`).join('');
  $('fax58').innerHTML = [5645,5700,5750,5800,5850,5900,5945]
    .map(f=>`<span>${f}</span>`).join('');
}

// ── Interpolated value between channels ───────────────────────
function lerp(data, px, W) {
  const n = data.length;
  const fi = px * (n-1) / Math.max(1, W-1);
  const lo = Math.floor(fi), hi = Math.min(n-1, lo+1);
  return (data[lo]||0) * (1-(fi-lo)) + (data[hi]||0) * (fi-lo);
}

// ── Spectrum draw ─────────────────────────────────────────────
function drawSpectrum(b, data) {
  const ct = ctx['sp'+b];
  const W = ct.canvas.width, H = ct.canvas.height;
  ct.clearRect(0, 0, W, H);

  // Horizontal grid lines
  ct.lineWidth = 1;
  [25, 50, 75].forEach(pct => {
    const y = H - (pct/100)*H;
    ct.strokeStyle = 'rgba(255,255,255,0.05)';
    ct.beginPath(); ct.moveTo(0,y); ct.lineTo(W,y); ct.stroke();
    ct.fillStyle = 'rgba(255,255,255,0.18)';
    ct.font = '8px Courier New';
    ct.fillText(pct+'%', 3, y-2);
  });

  // Gradient fill under curve (Karogic style)
  ct.beginPath();
  ct.moveTo(0, H);
  for (let px = 0; px < W; px++) {
    ct.lineTo(px, H - (lerp(data,px,W)/100)*H);
  }
  ct.lineTo(W, H);
  ct.closePath();
  const grad = ct.createLinearGradient(0, 0, 0, H);
  grad.addColorStop(0,   'rgba(255,120,0,0.85)');
  grad.addColorStop(0.35,'rgba(0,200,255,0.65)');
  grad.addColorStop(0.75,'rgba(0,50,180,0.35)');
  grad.addColorStop(1,   'rgba(0,0,30,0.05)');
  ct.fillStyle = grad;
  ct.fill();

  // Peak hold line (yellow)
  if (peakHold && peakData[b]) {
    ct.beginPath();
    ct.strokeStyle = 'rgba(255,220,0,0.75)';
    ct.lineWidth = 1;
    for (let px = 0; px < W; px++) {
      const pv = lerp(Array.from(peakData[b]), px, W);
      const y = H - (pv/100)*H;
      px===0 ? ct.moveTo(px,y) : ct.lineTo(px,y);
    }
    ct.stroke();
  }

  // Signal outline (white top edge)
  ct.beginPath();
  ct.strokeStyle = 'rgba(255,255,255,0.65)';
  ct.lineWidth = 1.5;
  for (let px = 0; px < W; px++) {
    const y = H - (lerp(data,px,W)/100)*H;
    px===0 ? ct.moveTo(px,y) : ct.lineTo(px,y);
  }
  ct.stroke();

  // Peak frequency annotation
  let maxV=0, maxI=0;
  data.forEach((v,i) => { if(v>maxV){maxV=v;maxI=i;} });
  const bm = BANDS[b];
  if (maxV > 2) {
    const freq = Math.round(bm.fMin + maxI*(bm.fMax-bm.fMin)/(data.length-1));
    $('pi'+b).innerHTML = `Peak: <b>${freq} MHz</b> @ ${maxV}%`;
  } else {
    $('pi'+b).innerHTML = '';
  }
}

// ── Peak hold update ─────────────────────────────────────────
function updatePeak(b, data) {
  if (!peakData[b]) return;
  for (let i=0; i<data.length; i++) {
    if (!peakHold) { peakData[b][i] = 0; continue; }
    if (data[i] > peakData[b][i]) peakData[b][i] = data[i];
    else peakData[b][i] = Math.max(0, peakData[b][i] - 0.4);
  }
}

// ── Waterfall: newest at bottom, scrolls upward ───────────────
function addWFRow(b, data) {
  const ct = ctx['wf'+b];
  const W = ct.canvas.width;
  const d = wfd[b].data;
  const n = data.length;

  // Shift all rows UP by one (row 0 is top/oldest, row H-1 is bottom/newest)
  d.copyWithin(0, W * 4);

  // Paint new bottom row
  const rowStart = (WF_H - 1) * W * 4;
  for (let px = 0; px < W; px++) {
    const v = Math.min(100, Math.max(0, lerp(data, px, W)));
    const vi = Math.round(v);
    const o = rowStart + px * 4;
    d[o]   = LUT[vi*3];
    d[o+1] = LUT[vi*3+1];
    d[o+2] = LUT[vi*3+2];
    d[o+3] = 255;
  }

  ct.putImageData(wfd[b], 0, 0);
  drawTimeAxis(b);
}

// ── Time axis (rendered over waterfall after putImageData) ────
function recordScan(b) {
  const now = Date.now();
  const arr = scanTimes[b];
  arr.push(now);
  if (arr.length > SCAN_HIST) arr.shift();
  if (arr.length >= 2)
    scanIntMs[b] = (arr[arr.length-1] - arr[0]) / (arr.length-1);
}

function drawTimeAxis(b) {
  const ct = ctx['wf'+b];
  const H  = WF_H;
  const intv = scanIntMs[b] / 1000;   // seconds per row
  // 5 evenly-spaced labels: bottom ("now") → top (oldest)
  ct.font = '9px Courier New';
  [1.0, 0.75, 0.5, 0.25, 0.0].forEach(frac => {
    // frac=0 → bottom row (y ≈ H-8) = newest
    // frac=1 → top row    (y ≈ 2)   = oldest
    const y   = Math.round(H * (1 - frac));
    const age = Math.round(frac * (H-1) * intv);
    const lbl = frac === 0 ? 'now' : `-${age}s`;
    const lw  = ct.measureText(lbl).width;
    // Semi-transparent pill behind label
    ct.fillStyle = 'rgba(0,0,0,0.55)';
    ct.fillRect(2, Math.max(0, y-11), lw+5, 13);
    ct.fillStyle = 'rgba(255,255,255,0.6)';
    ct.fillText(lbl, 4, Math.max(9, y-1));
  });
}

// ── FPS counter ───────────────────────────────────────────────
let fCnt = {'24':0,'58':0}, fTime = Date.now();
function tickFPS(b) {
  fCnt[b]++;
  const now = Date.now(), dt = now - fTime;
  if (dt >= 1000) {
    $('fps24').textContent = Math.round(fCnt['24']/dt*1000);
    $('fps58').textContent = Math.round(fCnt['58']/dt*1000);
    fCnt['24'] = fCnt['58'] = 0;
    fTime = now;
  }
}

// ── Crosshair ─────────────────────────────────────────────────
function initCrosshairs() {
  ['24','58'].forEach(b => {
    const el = $('spwf'+b);
    el.addEventListener('mousemove', e => moveCH(e, b));
    el.addEventListener('mouseleave', () => { $('ch'+b).style.display='none'; });
  });
}

function moveCH(e, b) {
  const el = $('spwf'+b);
  const r  = el.getBoundingClientRect();
  const mx = e.clientX - r.left, my = e.clientY - r.top;
  const W = r.width, H = r.height;
  const bm = BANDS[b];
  const freq = Math.round(bm.fMin + (mx/W)*(bm.fMax - bm.fMin));

  const ch = $('ch'+b);
  ch.style.display = '';
  $('chv'+b).style.left = mx + 'px';
  $('chh'+b).style.top  = my + 'px';
  const tip = $('cht'+b);
  tip.textContent = freq + ' MHz';
  const tw = 70;
  tip.style.left = (mx+8+tw > W ? mx-tw-6 : mx+6) + 'px';
  tip.style.top  = Math.max(0, my-20) + 'px';
}

// ── Band select ───────────────────────────────────────────────
let activeBand = 0;
function setBand(b) {
  activeBand = b;
  ['btnAll','btn24','btn58'].forEach((id,i) =>
    $(id).classList.toggle('act', i===b));
  $('sec24').style.display = (b===2) ? 'none' : '';
  $('sec58').style.display = (b===1) ? 'none' : '';
  send({c:'band', b});
}

// ── Peak hold ─────────────────────────────────────────────────
function togglePeak() {
  peakHold = !peakHold;
  $('btnPeak').classList.toggle('act', peakHold);
  if (!peakHold) {
    peakData['24'] && peakData['24'].fill(0);
    peakData['58'] && peakData['58'].fill(0);
  }
}

// ── WebSocket ─────────────────────────────────────────────────
let ws, retryTimer;

function wsConnect() {
  if (ws) { try { ws.close(); } catch(_){} }
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onopen  = () => { setConn(true);  send({c:'status'}); };
  ws.onclose = () => { setConn(false); clearTimeout(retryTimer); retryTimer = setTimeout(wsConnect, 3000); };
  ws.onerror = () => ws.close();
  ws.onmessage = ({data}) => {
    let msg;
    try { msg = JSON.parse(data); } catch(_) { return; }
    if (msg.t === 's') {
      const b = msg.b===0 ? '24' : '58';
      recordScan(b);
      updatePeak(b, msg.d);
      drawSpectrum(b, msg.d);
      addWFRow(b, msg.d);
      tickFPS(b);
    } else if (msg.t === 'w') {
      renderNetworks(msg.n);
    } else if (msg.t === 'st') {
      $('bMode').textContent = msg.m==='ap' ? '📡 AP' : '🌐 STA';
      $('bIP').textContent   = msg.ip || '--';
      if (msg.m==='ap' && msg.ss) $('apSSID').value = msg.ss;
    } else if (msg.t === 'msg') {
      wifiMsg(msg.m, msg.ok ? 'ok' : 'err');
    }
  };
}

function setConn(ok) {
  $('dot').className = 'dot'+(ok?' on':'');
  $('connTxt').textContent = ok ? 'Live' : 'Disconnected';
}
function send(obj) { if(ws && ws.readyState===1) ws.send(JSON.stringify(obj)); }

// ── WiFi panel ────────────────────────────────────────────────
function toggleWP() {
  const wp = $('wifiPanel');
  const open = wp.classList.toggle('open');
  $('wpArrow').textContent = open ? '▲' : '▼';
}

let curTab = 'ap';
function switchTab(tab) {
  curTab = tab;
  $('tabAP').classList.toggle('act',  tab==='ap');
  $('tabSTA').classList.toggle('act', tab==='sta');
  $('panelAP').style.display  = tab==='ap'  ? '' : 'none';
  $('panelSTA').style.display = tab==='sta' ? '' : 'none';
}

function applyAP() {
  const s = $('apSSID').value.trim(), p = $('apPass').value;
  if (!s) { wifiMsg('AP name is required','err'); return; }
  if (p && p.length < 8) { wifiMsg('Password must be ≥8 chars','err'); return; }
  send({c:'ap', s, p});
  wifiMsg('Applying AP settings…','info');
}

function doScan() { send({c:'scan'}); wifiMsg('Scanning…','info'); }

function renderNetworks(nets) {
  const el = $('netList');
  if (!nets || !nets.length) {
    el.innerHTML = '<div style="color:var(--dim);padding:4px">No networks found</div>';
    wifiMsg('Scan complete – none found','info');
    return;
  }
  el.innerHTML = nets.map(n => `
    <div class="net" onclick="pickNet(this,'${esc(n.s)}')">
      <span class="net-s">${esc(n.s)}</span>
      <span class="net-r">${n.r}dBm</span>
      <span>${n.e?'🔒':'🔓'}</span>
    </div>`).join('');
  wifiMsg(`Found ${nets.length} network(s)`,'ok');
}

function pickNet(el, ssid) {
  $('staSSID').value = ssid;
  document.querySelectorAll('.net').forEach(e => e.classList.toggle('sel', e===el));
}

function doConnect() {
  const s = $('staSSID').value.trim(), p = $('staPass').value;
  if (!s) { wifiMsg('SSID is required','err'); return; }
  send({c:'sta', s, p});
  wifiMsg('Connecting to "'+esc(s)+'"…','info');
}

function wifiMsg(txt, type) {
  const el = $('wMsg');
  el.textContent = txt;
  el.className = 'wmsg '+type;
  el.style.display = '';
}

function esc(s) {
  return String(s)
    .replace(/&/g,'&amp;').replace(/</g,'&lt;')
    .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

// ── Boot ──────────────────────────────────────────────────────
window.addEventListener('load', () => { initCanvases(); wsConnect(); });
window.addEventListener('resize', initCanvases);
</script>
</body>
</html>
)HTMLEOF";
