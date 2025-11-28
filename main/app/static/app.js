const qs=(k)=>encodeURIComponent(k);

function toast(msg,type="info"){
  const t=document.getElementById('toast');
  t.textContent=msg;
  t.className=`toast ${type}`;
  t.classList.add('show');
  setTimeout(()=>t.classList.remove('show'),2500);
}

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
    div.innerHTML=`<span class="tag">${e.manuf} / ${e.id}</span><button class="danger" style="padding:6px 10px" onclick="delWhitelist('${e.manuf}','${e.id}')">Del</button>`;
    box.appendChild(div);
  });
}

function renderPackets(list){
  const box=document.getElementById('pkt-list');
  box.innerHTML='';
  if(!list||!list.length){
    box.innerHTML='<div class="list-item"><span class="muted">No packets yet</span></div>';
    return;
  }
  list.slice(0,20).forEach(p=>{
    const div=document.createElement('div');
    div.className='list-item';
    div.innerHTML=`<div><div class="tag">${p.id}</div><div class="muted">RSSI ${p.rssi.toFixed(1)} | len ${p.payload_len}</div></div><div class="tag">${p.whitelisted?'whitelisted':'new'}</div>`;
    box.appendChild(div);
  });
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
  document.getElementById('hostname-pill').textContent=data.hostname||'oms-gateway';
  document.getElementById('hostname').value=data.hostname||'';
  document.getElementById('backend-url').value=data.backend.url||'';
  document.getElementById('wifi-ssid').value=data.wifi.ssid||'';
  document.getElementById('wifi-pass').value='';
  document.getElementById('wifi-status').textContent=data.wifi.connected?`Connected (${data.wifi.ip})`:'Not connected';
  document.getElementById('backend-status').textContent=data.backend.url?`Configured${data.backend.reachable?', reachable':''}`:'Not set';
  document.getElementById('cs-level').value=data.radio.cs_level;
  document.getElementById('sync-mode').value=data.radio.sync_mode;
  renderWhitelist(data.whitelist);
}

async function loadStatus(){
  try{const data=await fetchJSON('/api/status');fillStatus(data);}catch(e){toast('Failed to load status','error');}
}

async function saveWifi(){
  try{
    const ssid=document.getElementById('wifi-ssid').value;
    const pass=document.getElementById('wifi-pass').value;
    await postExpectOk(`/api/wifi?ssid=${qs(ssid)}&pass=${qs(pass)}`);
    toast('Wi-Fi saved','success');
    loadStatus();
  }catch(e){toast(e.message,'error');}
}
async function saveBackend(){
  try{
    const url=document.getElementById('backend-url').value;
    await postExpectOk(`/api/backend?url=${qs(url)}`);
    toast('Backend saved','success');
    loadStatus();
  }catch(e){toast(e.message,'error');}
}
async function testBackend(){
  try{
    const url=document.getElementById('backend-url').value;
    const r=await fetch(`/api/backend/test?url=${qs(url)}`);
    document.getElementById('backend-status').textContent=r.ok?'Reachable':'Unreachable';
    toast(r.ok?'Backend reachable':'Backend unreachable', r.ok?'success':'error');
  }catch(e){toast('Test failed','error');}
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
    const manuf=document.getElementById('wl-manuf').value;
    const id=document.getElementById('wl-id').value;
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

loadStatus();
loadPackets();
