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

const DEVICE_META={
  0x00:{name:"Other",cat:'F'},
  0x01:{name:"Oil meter",cat:'F'},
  0x02:{name:"Electricity meter",cat:'1'},
  0x03:{name:"Gas meter",cat:'7'},
  0x04:{name:"Heat meter (return)",cat:'6'},
  0x05:{name:"Steam meter",cat:'F'},
  0x06:{name:"Warm water meter",cat:'9'},
  0x07:{name:"Water meter",cat:'8'},
  0x08:{name:"Heat cost allocator",cat:'4'},
  0x09:{name:"Compressed air meter",cat:'F'},
  0x0A:{name:"Cooling meter (return)",cat:'5'},
  0x0B:{name:"Cooling meter (flow)",cat:'5'},
  0x0C:{name:"Heat meter (flow)",cat:'6'},
  0x0D:{name:"Combined heat/cooling meter",cat:'6'},
  0x0E:{name:"Bus/system component",cat:'E'},
  0x0F:{name:"Unknown device type",cat:'F'},
  0x10:{name:"Irrigation water meter",cat:'-'},
  0x11:{name:"Water data logger",cat:'-'},
  0x12:{name:"Gas data logger",cat:'-'},
  0x13:{name:"Gas converter",cat:'-'},
  0x14:{name:"Calorific value device",cat:'F'},
  0x15:{name:"Hot water meter",cat:'9'},
  0x16:{name:"Cold water meter",cat:'8'},
  0x17:{name:"Dual hot/cold water meter",cat:'9'},
  0x18:{name:"Pressure device",cat:'F'},
  0x19:{name:"A/D converter",cat:'F'},
  0x1A:{name:"Smoke alarm device",cat:'F'},
  0x1B:{name:"Room sensor",cat:'F'},
  0x1C:{name:"Gas detector",cat:'F'},
  0x1D:{name:"Carbon monoxide alarm",cat:'F'},
  0x1E:{name:"Heat alarm device",cat:'F'},
  0x1F:{name:"Sensor device",cat:'F'},
  0x20:{name:"Breaker (electricity)",cat:'F'},
  0x21:{name:"Valve (gas/water)",cat:'F'},
  0x22:{name:"Reserved switching 0x22",cat:'-'},
  0x23:{name:"Reserved switching 0x23",cat:'-'},
  0x24:{name:"Reserved switching 0x24",cat:'-'},
  0x25:{name:"Customer display unit",cat:'E'},
  0x26:{name:"Reserved customer 0x26",cat:'-'},
  0x27:{name:"Reserved customer 0x27",cat:'-'},
  0x28:{name:"Waste water meter",cat:'F'},
  0x29:{name:"Garbage meter",cat:'F'},
  0x2A:{name:"Reserved CO₂",cat:'F'},
  0x2B:{name:"Reserved env. 0x2B",cat:'-'},
  0x2C:{name:"Reserved env. 0x2C",cat:'-'},
  0x2D:{name:"Reserved env. 0x2D",cat:'-'},
  0x2E:{name:"Reserved env. 0x2E",cat:'-'},
  0x2F:{name:"Reserved env. 0x2F",cat:'-'},
  0x30:{name:"Reserved system 0x30",cat:'E'},
  0x31:{name:"Communication controller",cat:'E'},
  0x32:{name:"Unidirectional repeater",cat:'E'},
  0x33:{name:"Bidirectional repeater",cat:'E'},
  0x34:{name:"Reserved system 0x34",cat:'E'},
  0x35:{name:"Reserved system 0x35",cat:'E'},
  0x36:{name:"Radio converter (system)",cat:'E'},
  0x37:{name:"Radio converter (meter)",cat:'E'},
  0x38:{name:"Wired adapter",cat:'E'},
  0x39:{name:"Reserved system 0x39",cat:'E'},
  0x3A:{name:"Reserved system 0x3A",cat:'E'},
  0x3B:{name:"Reserved system 0x3B",cat:'E'},
  0x3C:{name:"Reserved system 0x3C",cat:'E'},
  0x3D:{name:"Reserved system 0x3D",cat:'E'},
  0x3E:{name:"Reserved system 0x3E",cat:'E'},
  0x3F:{name:"Reserved system 0x3F",cat:'E'},
  0xFF:{name:"Wildcard (search only)",cat:'-'}
};

const hex = (n,len) => `0x${Number(n||0).toString(16).toUpperCase().padStart(len,'0')}`;
const deviceTypeName = (code) => {
  if(code===undefined||code===null) return 'Unknown';
  const num=Number(code);
  const meta=DEVICE_META[num];
  return meta ? meta.name : `Unknown (${hex(num,2)})`;
};
const deviceCategory = (code) => {
  const num=Number(code);
  const cat=DEVICE_META[num]?.cat;
  return cat||'-';
};
const catLabel = (cat)=>{
  const map={
    '1':"Electricity",
    '4':"Heat cost alloc.",
    '5':"Cooling",
    '6':"Heat",
    '7':"Gas",
    '8':"Water",
    '9':"Warm/Hot water",
    'E':"System/infra",
    'F':"Other",
    '-':"Unknown"
  };
  return map[cat]||"Unknown";
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
  0x72:"TPL long header",
  0x7A:"TPL short header",
  0x78:"No TPL header",
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
const shortLabel = (txt, fallback) =>{
  if(!txt) return fallback||'-';
  // take token before space or "(" to keep it terse
  const m = txt.match(/^([A-Z0-9\\-]+)/i);
  return m ? m[1].toUpperCase() : (fallback||txt);
};
const cFieldClass=(c)=>{
  const v=Number(c)||0;
  if([0x44,0x47,0x48,0x08,0x18,0x28,0x38].includes(v)) return 'data';
  if([0x00,0x10,0x20,0x30].includes(v)) return 'ok';
  if([0x01,0x11,0x21,0x31,0x40].includes(v)) return 'err';
  if([0x46].includes(v)) return 'ctrl';
  return 'gray';
};
const ciFieldClass=(ci)=>{
  const v=Number(ci)||0;
  if([0x72,0x7A,0x73,0x7B,0x6B,0x6A,0x69].includes(v)) return 'data';
  if([0x70,0x71].includes(v)) return 'ctrl';
  return 'gray';
};
const statusFlags = (s) => {
  const flags=[];
  if(s & 0x04) flags.push('Low batt');
  if(s & 0x08) flags.push('Error');
  if(s & 0x10) flags.push('Temp err');
  return flags.length ? flags.join(', ') : 'OK';
};

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
    const cShort=shortLabel(cName,'C');
    const ciShort=shortLabel(ciName,'CI');
    const gw=p.gateway&&p.gateway.length?p.gateway:'-';
    tr.innerHTML=`
      <td>${gw}</td>
      <td>
        <div class="dev-name" title="${cName}">${cShort}</div>
        <div class="muted mono">${hex(p.control||0,2)}</div>
      </td>
      <td>
        <div class="dev-name" title="${ciName}">${ciShort}</div>
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
    tr.onclick=()=>showPacketModal(p);
    tbody.appendChild(tr);
  });
  setCard('card-monitor','info',`${count} packets`,'sensors');
}

function closePacketModal(){
  const modal=document.getElementById('pkt-modal');
  if(modal) modal.classList.add('hidden');
}

function showPacketModal(p){
  const modal=document.getElementById('pkt-modal');
  const body=document.getElementById('pkt-details');
  const title=document.getElementById('pkt-title');
  if(!modal||!body||!title) return;
  const manufStr=manufCodeToString(p.manuf);
  const manufHex=hex(p.manuf||0,4);
  const devName=deviceTypeName(p.dev_type);
  const cat=deviceCategory(p.dev_type);
  const catText=`${cat} (${catLabel(cat)})`;
  const idStr=p.id||'';
  const fabBlock=idStr ? idStr.slice(0,2) : '-';
  const fabNumber=idStr ? idStr.slice(2) : '-';
  const cName=cFieldName(p.control);
  const ciName=ciFieldName(p.ci);
  const statusVal = (p.status===null||p.status===undefined) ? null : p.status;
  const cfgVal = (p.cfg===null||p.cfg===undefined) ? null : p.cfg;
  title.textContent=`Packet ${p.id||''}`;
  body.innerHTML=`
    <div class="modal-grid">
      <div class="modal-section">
        <h4>Raw</h4>
        <div class="list">
          <div class="list-item"><span class="muted">Manufacturer</span><span class="mono">${manufHex}</span></div>
          <div class="list-item"><span class="muted">Fabrication block</span><span class="mono">${fabBlock}</span></div>
          <div class="list-item"><span class="muted">Fabrication number</span><span class="mono">${fabNumber}</span></div>
          <div class="list-item"><span class="muted">Device type</span><span class="mono">${hex(p.dev_type||0,2)}</span></div>
          <div class="list-item"><span class="muted">C-Field</span><span class="mono">${hex(p.control||0,2)}</span></div>
          <div class="list-item"><span class="muted">CI</span><span class="mono">${hex(p.ci||0,2)}</span></div>
          <div class="list-item"><span class="muted">Access Number</span><span class="mono">${(p.acc===null||p.acc===undefined)?'-':p.acc}</span></div>
          <div class="list-item"><span class="muted">Status</span><span class="mono">${statusVal===null?'-':hex(statusVal,2)}</span></div>
          <div class="list-item"><span class="muted">Config</span><span class="mono">${cfgVal===null?'-':hex(cfgVal,4)}</span></div>
          <div class="list-item"><span class="muted">RSSI</span><span class="mono">${(p.rssi??0).toFixed(1)} dBm</span></div>
          <div class="list-item"><span class="muted">Payload length</span><span class="mono">${p.payload_len??0}</span></div>
        </div>
      </div>
      <div class="modal-section">
        <h4>Parsed</h4>
        <div class="list">
          <div class="list-item"><span class="muted">Gateway</span><span>${p.gateway||'-'}</span></div>
          <div class="list-item"><span class="muted">Manufacturer</span><span>${manufStr}</span></div>
          <div class="list-item"><span class="muted">Fabrication block</span><span class="mono">${fabBlock}</span></div>
          <div class="list-item"><span class="muted">Fabrication number</span><span class="mono">${fabNumber}</span></div>
          <div class="list-item"><span class="muted">Version</span><span class="mono">${p.version??'-'}</span></div>
          <div class="list-item"><span class="muted">Device</span><span>${devName}</span></div>
          <div class="list-item"><span class="muted">OBIS Category</span><span>${catText}</span></div>
          <div class="list-item"><span class="muted">C-Field</span><span>${cName}</span></div>
          <div class="list-item"><span class="muted">CI</span><span>${ciName}</span></div>
          <div class="list-item"><span class="muted">TPL Header</span><span class="mono">${p.hdr||'none'}</span></div>
          <div class="list-item"><span class="muted">Access Number</span><span class="mono">${(p.acc===null||p.acc===undefined)?'-':p.acc}</span></div>
          <div class="list-item"><span class="muted">Status</span><span>${statusVal===null?'-':statusFlags(statusVal)}</span></div>
          <div class="list-item"><span class="muted">Config</span><span>${cfgVal===null?'-':hex(cfgVal,4)}</span></div>
          <div class="list-item"><span class="muted">Whitelisted</span><span>${p.whitelisted?'Yes':'No'}</span></div>
        </div>
      </div>
    </div>
  `;
  modal.classList.remove('hidden');
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
