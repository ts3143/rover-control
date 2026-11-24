// Controller web page, served by the rover at http://192.168.4.1
#pragma once

const char PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>Rover</title>
<style>
:root{--bg:#0f1115;--panel:#181b22;--line:#2a2f3a;--text:#e6e8ec;--dim:#8a91a0;--accent:#3ddc97;--warn:#ff6b5b}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;min-height:100%;background:var(--bg);color:var(--text);font:15px -apple-system,BlinkMacSystemFont,system-ui,sans-serif;overscroll-behavior:none;-webkit-user-select:none;user-select:none}
.app{max-width:900px;margin:0 auto;padding:16px;display:grid;gap:12px}
header{display:flex;justify-content:space-between;align-items:center}
h1{font-size:18px;margin:0}
.st{display:flex;gap:8px;align-items:center;color:var(--dim);font-size:13px}
.dot{width:10px;height:10px;border-radius:50%;background:var(--warn)}
.dot.on{background:var(--accent);box-shadow:0 0 8px var(--accent)}
.panel{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:14px}
.main{display:grid;grid-template-columns:1fr 1fr;gap:12px}
@media(max-width:640px){.main{grid-template-columns:1fr}}
#pad{position:relative;width:min(72vw,260px);aspect-ratio:1;margin:8px auto;border-radius:50%;background:radial-gradient(circle,#222733 0,#1b1f28 70%);border:1px solid var(--line);touch-action:none}
#knob{position:absolute;width:38%;height:38%;left:31%;top:31%;border-radius:50%;background:#2f3646;border:2px solid #4a5366;pointer-events:none}
#pad.active #knob{background:var(--accent);border-color:var(--accent)}
.label{font-size:11px;color:var(--dim);text-transform:uppercase;letter-spacing:.08em;margin-bottom:6px}
.bars{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:14px}
.bar{height:90px;background:#11141a;border:1px solid var(--line);border-radius:8px;position:relative;overflow:hidden}
.bar i{position:absolute;left:0;right:0;top:50%;height:0;background:var(--accent)}
.bar:after{content:"";position:absolute;left:0;right:0;top:50%;border-top:1px dashed #3a4152}
.bar span{position:absolute;bottom:4px;left:0;right:0;text-align:center;font-size:11px;color:var(--dim)}
.row{display:flex;align-items:center;gap:10px;margin-bottom:12px}
input[type=range]{flex:1;accent-color:var(--accent)}
select,button{font:inherit;color:var(--text);background:#222733;border:1px solid var(--line);border-radius:10px;padding:8px 12px;max-width:100%}
#stop{width:100%;background:#3a1c1c;border-color:#5a2a2a;color:#ffb3ab;font-weight:700;padding:14px}
#stop.on{background:var(--warn);color:#fff}
.small{font-size:12px;color:var(--dim);line-height:1.6}
#gp{font-size:13px;margin-bottom:8px}
</style></head><body>
<div class="app">
  <header><h1>Rover</h1><div class="st"><span class="dot" id="dot"></span><span id="conn">Connecting…</span></div></header>
  <div class="main">
    <div class="panel">
      <div class="label">Touch joystick</div>
      <div id="pad"><div id="knob"></div></div>
    </div>
    <div class="panel">
      <div class="label">Motor output</div>
      <div class="bars">
        <div class="bar"><i id="bl"></i><span id="tl">Left 0</span></div>
        <div class="bar"><i id="br"></i><span id="tr">Right 0</span></div>
      </div>
      <div class="label">Max speed</div>
      <div class="row"><input type="range" id="max" min="20" max="100" step="5" value="70"><b id="maxv">70%</b></div>
      <div class="label">Game controller</div>
      <div id="gp">None detected. Press a button on the controller.</div>
      <div class="row"><select id="mode">
        <option value="split">Split: left stick drive, right stick steer</option>
        <option value="arcade">Arcade: left stick only</option>
        <option value="race">Racing: RT forward, LT reverse, left stick steer</option>
      </select></div>
      <label class="row small"><input type="checkbox" id="inv"> Invert steering</label>
      <button id="stop">STOP</button>
      <p class="small">Controller: B = stop / resume &middot; LB / RB = speed &minus; / +</p>
    </div>
  </div>
</div>
<script>
const $ = id => document.getElementById(id);
const host = location.hostname || '192.168.4.1';
let ws = null, connected = false, estop = false;
const touch = {x: 0, y: 0, active: false};
const prevBtn = [];
let lastSend = 0;

// ---- settings persist per device (best effort) ----
function load(k, d) { try { const v = localStorage.getItem(k); return v === null ? d : JSON.parse(v); } catch (e) { return d; } }
function save(k, v) { try { localStorage.setItem(k, JSON.stringify(v)); } catch (e) {} }
$('max').value = load('max', 70); $('maxv').textContent = $('max').value + '%';
$('mode').value = load('mode', 'split');
$('inv').checked = load('inv', false);
$('max').oninput = () => { $('maxv').textContent = $('max').value + '%'; save('max', +$('max').value); };
$('mode').onchange = () => save('mode', $('mode').value);
$('inv').onchange = () => save('inv', $('inv').checked);

// ---- connection ----
function setConn() {
  $('dot').classList.toggle('on', connected);
  $('conn').textContent = connected ? 'Connected' : 'Reconnecting…';
}
function connect() {
  ws = new WebSocket('ws://' + host + ':81/');
  ws.onopen = () => { connected = true; setConn(); };
  ws.onclose = () => { connected = false; setConn(); setTimeout(connect, 1000); };
  ws.onerror = () => { try { ws.close(); } catch (e) {} };
}
function sendRaw(s) { if (ws && ws.readyState === 1) ws.send(s); }

// ---- stop / speed ----
function setStop(on) {
  estop = on;
  $('stop').classList.toggle('on', on);
  $('stop').textContent = on ? 'STOPPED: tap to resume' : 'STOP';
  if (on) sendRaw('s');
}
function setMax(v) {
  v = Math.max(20, Math.min(100, v));
  $('max').value = v; $('maxv').textContent = v + '%'; save('max', v);
}
$('stop').onclick = () => setStop(!estop);

// ---- touch joystick ----
const pad = $('pad'), knob = $('knob');
function drawKnob(x, y) { knob.style.transform = 'translate(' + (x * 31 / 38 * 100) + '%,' + (-y * 31 / 38 * 100) + '%)'; }
function padMove(e) {
  const r = pad.getBoundingClientRect();
  let x = (e.clientX - r.left) / r.width * 2 - 1;
  let y = -((e.clientY - r.top) / r.height * 2 - 1);
  const m = Math.hypot(x, y);
  if (m > 1) { x /= m; y /= m; }
  touch.x = x; touch.y = y; drawKnob(x, y);
}
function padEnd() { touch.active = false; touch.x = 0; touch.y = 0; pad.classList.remove('active'); drawKnob(0, 0); }
pad.addEventListener('pointerdown', e => { pad.setPointerCapture(e.pointerId); touch.active = true; pad.classList.add('active'); padMove(e); });
pad.addEventListener('pointermove', e => { if (touch.active) padMove(e); });
pad.addEventListener('pointerup', padEnd);
pad.addEventListener('pointercancel', padEnd);

// ---- game controller (standard mapping) ----
function dz(v, d) { d = d || 0.12; const a = Math.abs(v); return a < d ? 0 : Math.sign(v) * (a - d) / (1 - d); }
function readGamepad() {
  const list = navigator.getGamepads ? navigator.getGamepads() : [];
  let gp = null;
  for (const g of list) { if (g && g.connected) { gp = g; break; } }
  if (!gp) { $('gp').textContent = 'None detected. Press a button on the controller.'; return null; }
  $('gp').textContent = 'Detected: ' + gp.id.slice(0, 48);
  const btn = i => gp.buttons[i] || {pressed: false, value: 0};
  const edge = i => { const p = btn(i).pressed, was = prevBtn[i]; prevBtn[i] = p; return p && !was; };
  if (edge(1)) setStop(!estop);                 // B
  if (edge(4)) setMax(+$('max').value - 10);    // LB
  if (edge(5)) setMax(+$('max').value + 10);    // RB
  const ax = i => gp.axes[i] || 0;
  const mode = $('mode').value;
  if (mode === 'arcade') return {x: dz(ax(0)), y: dz(-ax(1))};
  if (mode === 'race') return {x: dz(ax(0)), y: btn(7).value - btn(6).value};
  return {x: dz(ax(2)), y: dz(-ax(1))};
}

// ---- mixing: stick (x,y) -> left/right motor speeds ----
function curve(v) { return Math.sign(v) * Math.pow(Math.abs(v), 1.6); }  // finer control near center
function mix(x, y) {
  x = curve(x); y = curve(y);
  if ($('inv').checked) x = -x;
  let l = y + x, r = y - x;
  const m = Math.max(1, Math.abs(l), Math.abs(r));
  const s = +$('max').value / 100;
  return {l: Math.round(l / m * s * 255), r: Math.round(r / m * s * 255)};
}
function drawBar(el, label, name, v) {
  const h = Math.abs(v) / 255 * 50;
  el.style.height = h + '%';
  el.style.top = (v >= 0 ? 50 - h : 50) + '%';
  label.textContent = name + ' ' + Math.round(v / 2.55) + '%';
}

// ---- main loop: 20 commands/s; pauses when the tab is hidden, so the rover failsafe stops it ----
function tick(t) {
  const g = readGamepad();
  let x = 0, y = 0;
  if (touch.active) { x = touch.x; y = touch.y; } else if (g) { x = g.x; y = g.y; }
  const out = estop ? {l: 0, r: 0} : mix(x, y);
  drawBar($('bl'), $('tl'), 'Left', out.l);
  drawBar($('br'), $('tr'), 'Right', out.r);
  if (t - lastSend >= 50) { sendRaw('d,' + out.l + ',' + out.r); lastSend = t; }
  requestAnimationFrame(tick);
}
document.addEventListener('visibilitychange', () => { if (document.hidden) sendRaw('s'); });
window.addEventListener('pagehide', () => sendRaw('s'));

connect();
requestAnimationFrame(tick);
</script>
</body></html>)rawliteral";
