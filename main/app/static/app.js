const qs=(k)=>encodeURIComponent(k);
let currentSsid='';
let currentMode='client';

function setBadge(id,valueId,text,state){
  const box=document.getElementById(id);
  const val=document.getElementById(valueId);
  if(box){ box.dataset.state=state||''; }
  if(val){ val.textContent=text; }
}

function setPill(id,text,state){
  const pill=document.getElementById(id);
  if(!pill) return;
  pill.textContent=text;
  if(state){ pill.dataset.state=state; } else { pill.removeAttribute('data-state'); }
}

function setCard(id,state,subtitle,iconText){
  const card=document.getElementById(id);
  if(!card) return;
  card.classList.remove('ok','warn','error','info');
  if(state) card.classList.add(state);
  const sub=document.getElementById(`${id}-sub`);
  if(sub) sub.textContent=subtitle;
  const icon=document.getElementById(`icon-${id.split('card-')[1]}`);
  if(icon){
    icon.dataset.icon=iconText||'';
    const svg=icon.querySelector('svg');
    if(svg && iconText){
      svg.setAttribute('data-icon', iconText);
    }
  }
}

function updateBackendIndicators(url, reachable){
  const hasUrl=!!(url&&url.length);
  if(!hasUrl){
    setBadge('status-backend','status-backend-value','Not set','warn');
    setCard('card-backend','warn','Not set','cloud');
    const status=document.getElementById('backend-status');
    if(status) status.textContent='Not set';
    return;
  }
  const tone=reachable?'ok':'error';
  const icon = reachable ? 'cloud_done' : 'cloud_off';
  setBadge('status-backend','status-backend-value',reachable?'Connected':'Offline',tone);
  setCard('card-backend',tone,reachable?'Connected':'Offline',icon);
  const status=document.getElementById('backend-status');
  if(status) status.textContent=reachable?'Reachable':'Unreachable';
}

function toast(msg,type="info"){
  const t=document.getElementById('toast');
  t.textContent=msg;
  t.className=`toast ${type}`;
  t.classList.add('show');
  setTimeout(()=>t.classList.remove('show'),2500);
}

const isHex = (str,len) => new RegExp(`^[0-9a-fA-F]{${len}}$`).test(str||'');

function renderWhitelist(entries){
  const box=document.getElementById('wl-list');
  box.innerHTML='';
  if(!entries||!entries.length){
    box.innerHTML='<div class="list-item"><span class="muted">No entries</span></div>';
    return;
  }
  entries.forEach(e=>{
    const div=document.createElement('div');
    div.className='list-item';
    div.innerHTML=`<span class="tag">${e.manuf} / ${e.id}</span><button class="icon-btn icon-trash" onclick="delWhitelist('${e.manuf}','${e.id}')"></button>`;
    box.appendChild(div);
  });
}

function renderPackets(list){
  const box=document.getElementById('pkt-list');
  box.innerHTML='';
  document.getElementById('card-monitor-sub').textContent=`${list?list.length:0} packets`;
  if(!list||!list.length){
    box.innerHTML='<div class="list-item"><span class="muted">No packets yet</span></div>';
    setCard('card-monitor','warn','No packets','sensors');
    return;
  }
  list.slice(0,20).forEach(p=>{
    const div=document.createElement('div');
    div.className='list-item';
    div.innerHTML=`<div><div class="tag">${p.id}</div><div class="muted">RSSI ${p.rssi.toFixed(1)} | len ${p.payload_len}</div></div><div class="tag">${p.whitelisted?'whitelisted':'new'}</div>`;
    box.appendChild(div);
  });
  setCard('card-monitor','info',`${list.length} packets`,'sensors');
}

async function fetchJSON(path){
  const r=await fetch(path);
  if(!r.ok) throw new Error('req failed');
  return r.json();
}

async function postExpectOk(url){
  const r=await fetch(url,{method:'POST'});
  if(!r.ok){
    let msg=`Error ${r.status}`;
    try{const j=await r.json();if(j.error) msg=j.error;}catch(_){/* ignore */}
    throw new Error(msg);
  }
}

function fillStatus(data){
  const ap = data.ap || {ssid:'',channel:0,has_pass:false};
  document.getElementById('hostname-pill').textContent=data.hostname||'oms-gateway';
  document.getElementById('hostname').value=data.hostname||'';
  document.getElementById('backend-url').value=data.backend.url||'';
  currentSsid=data.wifi.ssid||'';
  document.getElementById('wifi-ssid').value=currentSsid;
  const passInput=document.getElementById('wifi-pass');
  passInput.value='';
  passInput.placeholder=data.wifi.has_pass?'********':'';
  document.getElementById('wifi-ip').textContent=data.wifi.ip||'-';
  document.getElementById('wifi-rssi').textContent=(data.wifi.connected && data.wifi.rssi!==undefined)?`${data.wifi.rssi} dBm`:'-';
  document.getElementById('wifi-gw').textContent=data.wifi.gateway||'-';
  document.getElementById('wifi-dns').textContent=data.wifi.dns||'-';
  document.getElementById('ap-ssid').value=ap.ssid||'';
  document.getElementById('ap-pass').value='';
  document.getElementById('cs-level').value=data.radio.cs_level;
  document.getElementById('sync-mode').value=data.radio.sync_mode;
  setBadge('status-wifi','status-wifi-value',data.wifi.connected?`Connected (${data.wifi.ip})`:'Not connected',data.wifi.connected?'ok':'warn');
  setBadge('status-radio','status-radio-value',`CS ${data.radio.cs_level} · Sync ${data.radio.sync_mode}`,'ok');
  updateBackendIndicators(data.backend.url,data.backend.reachable);
  const wlEntries = (data.whitelist && Array.isArray(data.whitelist.entries)) ? data.whitelist.entries : [];
  renderWhitelist(wlEntries);
  const wlCount = wlEntries.length;
  setCard('card-wifi',data.wifi.connected?'ok':'warn',data.wifi.connected?`STA ${data.wifi.ssid}`:'AP Mode','wifi');
  setCard('card-whitelist',wlCount>0?'info':'warn',wlCount>0?`${wlCount} entries`:'No entries','shield');
}

async function loadStatus(){
  try{const data=await fetchJSON('/api/status');fillStatus(data);}catch(e){toast('Failed to load status','error');}
}

function showMode(mode){
  currentMode=mode;
  document.getElementById('client-form').style.display=mode==='client'?'block':'none';
  document.getElementById('ap-form').style.display=mode==='ap'?'block':'none';
  document.getElementById('btn-client').classList.toggle('active',mode==='client');
  document.getElementById('btn-ap').classList.toggle('active',mode==='ap');
}

async function saveNetwork(){
  try{
    const ssid=document.getElementById('wifi-ssid').value.trim();
    const pass=document.getElementById('wifi-pass').value;
    if(!ssid){ toast('SSID cannot be empty','error'); return; }
    await postExpectOk(`/api/wifi?ssid=${qs(ssid)}&pass=${qs(pass)}`);
    toast('Wi-Fi saved','success');
    loadStatus();
  }catch(e){toast(e.message,'error');}
}
async function saveHostname(){
  try{
    const hostname=document.getElementById('hostname').value.trim();
    if(!hostname){ toast('Hostname cannot be empty','error'); return; }
    await postExpectOk(`/api/hostname?name=${qs(hostname)}`);
    toast('Hostname saved','success');
    loadStatus();
  }catch(e){toast(e.message,'error');}
}
async function saveBackend(){
  try{
    const url=document.getElementById('backend-url').value.trim();
    if(!url){ toast('Please set a backend URL','error'); return; }
    await postExpectOk(`/api/backend?url=${qs(url)}`);
    toast('Backend saved','success');
    loadStatus();
  }catch(e){toast(e.message,'error');}
}
async function testBackend(){
  try{
    const url=document.getElementById('backend-url').value.trim();
    if(!url){ toast('No backend configured','error'); return; }
    const r=await fetch(`/api/backend/test?url=${qs(url)}`);
    updateBackendIndicators(url,r.ok);
    toast(r.ok?'Backend reachable':'Backend unreachable', r.ok?'success':'error');
  }catch(e){toast('Test failed','error');}
}
async function saveAp(){
  try{
    const ssid=document.getElementById('ap-ssid').value.trim();
    const pass=document.getElementById('ap-pass').value;
    if(!ssid){ toast('AP SSID cannot be empty','error'); return; }
    await postExpectOk(`/api/ap?ssid=${qs(ssid)}&pass=${qs(pass)}`);
    toast('AP saved','success');
    loadStatus();
  }catch(e){toast(e.message,'error');}
}
async function saveRadio(){
  try{
    const cs=document.getElementById('cs-level').value;
    const sync=document.getElementById('sync-mode').value;
    await postExpectOk(`/api/radio?cs=${qs(cs)}&sync=${qs(sync)}`);
    toast('Radio saved','success');
    loadStatus();
  }catch(e){toast(e.message,'error');}
}
async function addWhitelist(){
  try{
    const manuf=document.getElementById('wl-manuf').value.trim();
    const id=document.getElementById('wl-id').value.trim();
    if(!isHex(manuf,4)||!isHex(id,8)){ toast('Enter manufacturer (4 hex) and ID (8 hex)','error'); return; }
    await postExpectOk(`/api/whitelist/add?manuf=${qs(manuf)}&id=${qs(id)}`);
    toast('Whitelist updated','success');
    loadStatus();
  }catch(e){toast(e.message,'error');}
}
async function delWhitelist(manuf,id){
  try{
    await postExpectOk(`/api/whitelist/del?manuf=${qs(manuf)}&id=${qs(id)}`);
    toast('Removed','success');
    loadStatus();
  }catch(e){toast(e.message,'error');}
}

async function loadPackets(){try{const data=await fetchJSON('/api/packets');if(data.packets){renderPackets(data.packets);} }catch(e){/* ignore */}}
setInterval(loadPackets,3000);
setInterval(loadStatus,5000);

loadStatus();
loadPackets();
// apply icon masks so external SVGs inherit currentColor
document.addEventListener('DOMContentLoaded', ()=>{
  document.querySelectorAll('.icon').forEach(el=>{
    const name=el.dataset.icon;
    if(!name) return;
    const url=`/static/icons/${name}.svg`;
    el.style.maskImage=`url(${url})`;
    el.style.webkitMaskImage=`url(${url})`;
    el.style.backgroundColor='currentColor';
  });
  showMode('client');
});
