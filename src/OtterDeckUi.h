#pragma once

#if defined(ARDUINO_ARCH_ESP32) && defined(RO_ENABLE_WIFI_WORKSTATION)
#include <pgmspace.h>

static const char OTTERDECK_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>RubberOrca // OtterDeck Workstation</title>
<style>
:root{color-scheme:dark;--bg:#09090b;--sf:#111114;--bd:#27272a;--tx:#f4f4f5;--mt:#9898a3;--ac:#ff4500}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--tx);font:14px/1.5 ui-monospace,SFMono-Regular,Menlo,monospace;padding:16px;max-width:960px;margin:0 auto}
header{display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid var(--bd);padding-bottom:12px;margin-bottom:16px;gap:8px;flex-wrap:wrap}
h1{font-size:15px;letter-spacing:.08em} .ac{color:var(--ac)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:14px}
.card{background:var(--sf);border:1px solid var(--bd);border-radius:6px;padding:14px;display:flex;flex-direction:column;gap:10px}
.card h2{font-size:12px;color:var(--ac);letter-spacing:.06em}
input,textarea,button{font:inherit;background:#09090b;color:var(--tx);border:1px solid var(--bd);border-radius:4px;padding:8px 10px}
button{cursor:pointer;min-height:40px} button.pri{background:var(--ac);color:#09090b;border-color:var(--ac);font-weight:700}
.row{display:flex;gap:8px;flex-wrap:wrap} .row>*{flex:1}
#pad{height:120px;border:1px dashed var(--bd);border-radius:4px;display:grid;place-items:center;color:var(--mt);touch-action:none;user-select:none}
#log{background:#060608;border:1px solid var(--bd);border-radius:4px;padding:8px;height:120px;overflow:auto;font-size:12px;color:#d4d4d8;white-space:pre-wrap}
.sitem{display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid var(--bd);padding:6px 0;gap:6px}
</style>
</head>
<body>
<header>
  <h1><span class="ac">●</span> RUBBERORCA // OTTERDECK v2.0</h1>
  <div class="row" style="max-width:340px">
    <input id="tok" type="password" placeholder="API / OTA Token (12+ chars)">
    <button class="pri" onclick="connectWs()">CONNECT</button>
  </div>
</header>
<div class="grid">
  <section class="card">
    <h2>01 // LIVE HID &amp; WEBSOCKET</h2>
    <div class="row"><input id="cmd" placeholder='type "hello" && enter'><button class="pri" style="flex:0" onclick="sendCmd()">RUN</button></div>
    <div class="row">
      <button onclick="wsSend('media play_pause')">PLAY/PAUSE</button>
      <button onclick="wsSend('media volume_up')">VOL+</button>
      <button onclick="wsSend('media mute')">MUTE</button>
      <button onclick="wsSend('jiggler toggle')">JIGGLER</button>
    </div>
    <div id="pad">DRAG FOR RELATIVE MOUSE</div>
    <div class="row">
      <button onclick="wsSend('mouse click left')">L-CLICK</button>
      <button onclick="wsSend('mouse click right')">R-CLICK</button>
    </div>
    <div id="log">// Enter token and click CONNECT.</div>
  </section>
  <section class="card">
    <h2>02 // LITTLEFS SCRIPTS (/scripts)</h2>
    <div class="row"><input id="sname" placeholder="demo.orca"><button class="pri" style="flex:0" onclick="saveScript()">SAVE</button></div>
    <textarea id="sbody" rows="6" placeholder="REM DuckyScript or RubberOrca&#10;STRINGLN Hello from LittleFS"></textarea>
    <div id="slist"></div>
  </section>
  <section class="card">
    <h2>03 // AUTHENTICATED A/B OTA</h2>
    <p style="font-size:12px;color:var(--mt)">Upload PlatformIO <code>firmware.bin</code> (requires configured NVS OTA token).</p>
    <input id="otaFile" type="file" accept=".bin">
    <button class="pri" onclick="uploadOta()">FLASH OTA FIRMWARE</button>
  </section>
</div>
<script>
let ws=null,lx=0,ly=0;
const log=m=>{const el=document.getElementById('log');el.textContent+=m+'\n';el.scrollTop=el.scrollHeight};
const hdr=()=>({'X-Orca-Token':document.getElementById('tok').value});
function connectWs(){
  const t=document.getElementById('tok').value.trim();
  if(ws) ws.close();
  ws=new WebSocket(`ws://${location.hostname}:81/`);
  ws.onopen=()=>{ws.send('auth '+t);log('[WS] connected');loadScripts()};
  ws.onmessage=e=>log('[DEV] '+e.data);
  ws.onclose=()=>log('[WS] closed');
}
function wsSend(c){if(ws&&ws.readyState===1) ws.send(c); else fetch('/api/cmd',{method:'POST',headers:hdr(),body:c}).then(r=>r.text()).then(t=>log('[HTTP] '+t))}
function sendCmd(){const i=document.getElementById('cmd');if(i.value.trim()){wsSend(i.value.trim());i.value=''}}
const pad=document.getElementById('pad');
pad.onpointerdown=e=>{lx=e.clientX;ly=e.clientY;pad.setPointerCapture(e.pointerId)};
pad.onpointermove=e=>{if(e.buttons!==1)return;const dx=Math.round((e.clientX-lx)*1.5),dy=Math.round((e.clientY-ly)*1.5);if(dx||dy){lx=e.clientX;ly=e.clientY;wsSend(`mouse move ${dx} ${dy}`)}};
async function loadScripts(){
  const r=await fetch('/api/scripts',{headers:hdr()});if(!r.ok)return;
  const arr=await r.json(),el=document.getElementById('slist');el.innerHTML='';
  arr.forEach(s=>{const d=document.createElement('div');d.className='sitem';
    d.innerHTML=`<span>${s.name} (${s.size}B)</span><span><button onclick="wsSend('script run ${s.name}')">RUN</button> <button onclick="delScript('${s.name}')">DEL</button></span>`;
    el.appendChild(d)});
}
async function saveScript(){
  const n=document.getElementById('sname').value.trim(),b=document.getElementById('sbody').value;
  const r=await fetch('/api/scripts?name='+encodeURIComponent(n),{method:'POST',headers:hdr(),body:b});
  log('[SCRIPT] '+(await r.text()));loadScripts();
}
async function delScript(n){await fetch('/api/scripts?name='+encodeURIComponent(n),{method:'DELETE',headers:hdr()});loadScripts()}
async function uploadOta(){
  const f=document.getElementById('otaFile').files[0];if(!f)return log('[OTA] choose .bin');
  log('[OTA] uploading '+f.size+' bytes...');
  const fd=new FormData();fd.append('firmware',f,f.name);
  const r=await fetch('/api/ota',{method:'POST',headers:hdr(),body:fd});
  log('[OTA] '+r.status+' '+(await r.text()));
}
</script>
</body>
</html>)rawliteral";
#endif
