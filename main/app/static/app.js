const qs=(k)=>encodeURIComponent(k);
let currentSsid='';
let currentMode='client';

// Manufacturer code (EN 13757-3) -> 3-letter string
function manufCodeToString(code){
  const c=Number(code||0)&0x7FFF;
  const l1=((c >> 0) & 0x1F);
  const l2=((c >> 5) & 0x1F);
  const l3=((c >> 10)& 0x1F);
  const toChar=(v)=> (v>=1 && v<=26) ? String.fromCharCode(64+v) : '?';
  return `${toChar(l1)}${toChar(l2)}${toChar(l3)}`;
}

const DEVICE_TYPES={
  0x00:"Other",
  0x01:"Oil meter",
  0x02:"Electricity meter",
  0x03:"Gas meter",
  0x04:"Heat meter (return)",
  0x05:"Steam meter",
  0x06:"Warm water meter",
  0x07:"Water meter",
  0x08:"Heat cost allocator",
  0x09:"Compressed air meter",
  0x0A:"Cooling meter (return)",
  0x0B:"Cooling meter (flow)",
  0x0C:"Heat meter (flow)",
  0x0D:"Combined heat/cooling meter",
  0x0E:"Bus/system component",
  0x0F:"Unknown device type",
  0x10:"Irrigation water meter",
  0x11:"Water data logger",
  0x12:"Gas data logger",
  0x13:"Gas converter",
  0x14:"Calorific value device",
  0x15:"Hot water meter",
  0x16:"Cold water meter",
  0x17:"Dual hot/cold water meter",
  0x18:"Pressure device",
  0x19:"A/D converter",
  0x1A:"Smoke alarm device",
  0x1B:"Room sensor",
  0x1C:"Gas detector",
  0x1D:"Carbon monoxide alarm",
  0x1E:"Heat alarm device",
  0x1F:"Sensor device",
  0x20:"Breaker (electricity)",
  0x21:"Valve (gas/water)",
  0x22:"Reserved switching 0x22",
  0x23:"Reserved switching 0x23",
  0x24:"Reserved switching 0x24",
  0x25:"Customer display unit",
  0x26:"Reserved customer 0x26",
  0x27:"Reserved customer 0x27",
  0x28:"Waste water meter",
  0x29:"Garbage meter",
  0x2A:"Reserved CO₂",
  0x2B:"Reserved env. 0x2B",
  0x2C:"Reserved env. 0x2C",
  0x2D:"Reserved env. 0x2D",
  0x2E:"Reserved env. 0x2E",
  0x2F:"Reserved env. 0x2F",
  0x30:"Reserved system 0x30",
  0x31:"Communication controller",
  0x32:"Unidirectional repeater",
  0x33:"Bidirectional repeater",
  0x34:"Reserved system 0x34",
  0x35:"Reserved system 0x35",
  0x36:"Radio converter (system)",
  0x37:"Radio converter (meter)",
  0x38:"Wired adapter",
  0x39:"Reserved system 0x39",
  0x3A:"Reserved system 0x3A",
  0x3B:"Reserved system 0x3B",
  0x3C:"Reserved system 0x3C",
  0x3D:"Reserved system 0x3D",
  0x3E:"Reserved system 0x3E",
  0x3F:"Reserved system 0x3F",
  0xFF:"Wildcard (search only)"
};

const hex = (n,len) => `0x${Number(n||0).toString(16).toUpperCase().padStart(len,'0')}`;
const deviceTypeName = (code) => {
  if(code===undefined||code===null) return 'Unknown';
  const num=Number(code);
  return DEVICE_TYPES[num]||`Unknown (${hex(num,2)})`;
};

const C_FIELD_MAP={
  0x44:"SND-NR (uplink)",
  0x46:"SND-IR (install)",
  0x47:"ACC-NR (access)",
  0x48:"ACC-DMD",
  0x08:"RSP-UD",
  0x18:"RSP-UD",
  0x28:"RSP-UD",
  0x38:"RSP-UD",
  0x00:"ACK",
  0x10:"ACK",
  0x20:"ACK",
  0x30:"ACK",
  0x01:"NACK",
  0x11:"NACK",
  0x21:"NACK",
  0x31:"NACK",
  0x40:"SND-NKE",
  0x06:"CNF-IR",
  0x43:"SND-UD2",
  0x53:"SND-UD",
  0x73:"SND-UD",
  0x5A:"REQ-UD1",
  0x7A:"REQ-UD1",
  0x5B:"REQ-UD2",
  0x7B:"REQ-UD2"
};
const CI_FIELD_MAP={
  0x72:"APL long header",
  0x7A:"APL short header",
  0x78:"No APL header",
  0x70:"Application error",
  0x71:"Alarm status",
  0x50:"App reset/select",
  0x51:"Master → meter data",
  0x52:"Slave selection",
  0x5A:"Command (short hdr)",
  0x5B:"Command (long hdr)",
  0x5C:"Sync action"
};
const cFieldName=(c)=>C_FIELD_MAP[Number(c)||0]||`Unknown (${hex(c,2)})`;
const ciFieldName=(ci)=>CI_FIELD_MAP[Number(ci)||0]||`Unknown (${hex(ci,2)})`;

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
  const tbody=document.getElementById('pkt-body');
  const empty=document.getElementById('pkt-empty');
  if(!tbody||!empty) return;
  tbody.innerHTML='';
  const count = list ? list.length : 0;
  document.getElementById('card-monitor-sub').textContent=`${count} packets`;
  if(!list||!list.length){
    empty.style.display='block';
    setCard('card-monitor','warn','No packets','sensors');
    return;
  }
  empty.style.display='none';
  list.slice(0,20).forEach(p=>{
    const tr=document.createElement('tr');
    const manufHex=hex(p.manuf||0,4);
    const ci=hex(p.ci||0,2);
    const devName=deviceTypeName(p.dev_type);
    const manufStr=manufCodeToString(p.manuf);
    const cName=cFieldName(p.control);
    const ciName=ciFieldName(p.ci);
    const gw=p.gateway&&p.gateway.length?p.gateway:'-';
    tr.innerHTML=`
      <td>${gw}</td>
      <td>
        <div class="dev-name">${cName}</div>
        <div class="muted mono">${hex(p.control||0,2)}</div>
      </td>
      <td>
        <div class="dev-name">${ciName}</div>
        <div class="muted mono">${ci}</div>
      </td>
      <td>
        <div class="dev-name">${manufStr}</div>
        <div class="muted mono">${manufHex}</div>
      </td>
      <td><span class="mono">${p.id||''}</span></td>
      <td>
        <div class="dev-name">${devName}</div>
        <div class="muted mono">${hex(p.dev_type||0,2)}</div>
      </td>
      <td class="mono">${p.version??''}</td>
      <td class="mono">${(p.rssi??0).toFixed(1)} dBm</td>
      <td class="mono">${p.payload_len??0}</td>
      <td><span class="pill tiny ${p.whitelisted?'ok':'error'}">${p.whitelisted?'Yes':'No'}</span></td>`;
    tbody.appendChild(tr);
  });
  setCard('card-monitor','info',`${count} packets`,'sensors');
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
