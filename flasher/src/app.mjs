import { ESPLoader, Transport } from 'esptool-js';
import { md5 } from 'js-md5';
import { validateDevice, validateManifest, verifyBytes } from './safety.mjs';
const $=id=>document.getElementById(id);
let catalog, loader, transport, selected, files, busy=false;
const log=line=>{ $('log').textContent=($('log').textContent+String(line)+'\n').slice(-16000); };
function status(title,text,tag='INSTALLATION') { $('status-title').textContent=title; $('status').textContent=text; $('status-tag').textContent=tag; }
function choice() { return `${document.querySelector('input[name=protocol]:checked').value}/n${$('memory').value}r8-gpio${$('led').value}`; }
function update() {
  const p=catalog?.profiles.find(p=>p.id===choice());
  $('connect').disabled=busy || !catalog || !$('board-match').checked || !navigator.serial || !isSecureContext;
  $('release-note').textContent=p ? (p.protocol==='meshcore' ? 'MeshCore preview · Protocol tests pass; testing on physical radios is still needed.' : 'Meshtastic · Earlier builds worked with Heltec V3. This release adds radio discovery and setup improvements.') : 'Loading firmware choices…';
}
async function disconnect() {
  try { if(transport) await transport.disconnect(); } catch(e){log(e.message);}
  transport=null;loader=null;files=null;selected=null;
  $('install').hidden=true;$('disconnect').hidden=true;$('connect').hidden=false;$('choices').disabled=false;
  busy=false;update();
}
async function connect() {
  if(busy) return;
  const profile=catalog?.profiles.find(p=>p.id===choice());
  try { validateManifest(profile); } catch(e){status('Catalog unavailable',e.message,'NEEDS ATTENTION');return;}
  busy=true;update();$('choices').disabled=true;$('next').hidden=true;$('progress').hidden=true;$('detected').textContent='';
  try {
    // Keep the port chooser directly in the click gesture; browsers require it.
    const port=await navigator.serial.requestPort();
    status('Connecting…','Keep your board plugged in while its chip and memory are checked.');
    transport=new Transport(port,false);
    loader=new ESPLoader({transport,baudrate:460800,terminal:{clean:()=>{},write:log,writeLine:log},debugLogging:false});
    await loader.main();
    const device={chip:loader.chip.CHIP_NAME,flash:await loader.detectFlashSize(),features:await loader.chip.getChipFeatures(loader)};
    validateDevice(device,profile);
    selected=profile;
    files=[];
    for(const part of profile.parts){
      const response=await fetch('./'+part.path,{cache:'no-store'});
      if(!response.ok) throw new Error('The firmware download was unavailable. Connect again when your Internet connection is ready.');
      const bytes=new Uint8Array(await response.arrayBuffer());await verifyBytes(bytes,part);
      files.push({address:part.address,data:bytes});
    }
    $('detected').textContent=`${device.chip} · ${device.flash} flash · 8 MB PSRAM · selected LED GPIO${profile.ledGPIO}`;
    status('Ready to install',`${profile.label}. The firmware files passed their integrity checks. Install when you are ready.`,'BOARD CHECKED');
    $('connect').hidden=true;$('install').hidden=false;$('install').disabled=false;$('disconnect').hidden=false;
  } catch(e){const cancelled=e.name==='NotFoundError';status(cancelled?'No board selected':'Connection needs attention',cancelled?'Choose Connect USB board when you are ready.':e.message,'NOT INSTALLED');log(e.message);await disconnect();}
  busy=false;update();
}
async function install() {
  if(busy || !loader || !selected || !files) return;
  busy=true;$('install').disabled=true;$('disconnect').disabled=true;$('progress').hidden=false;$('progress').value=0;
  status('Installing MESHBBS…','Keep the USB cable connected and leave this tab open.');
  try {
    validateManifest(selected);
    // Never erase NVS: it contains the owner credential and saved bulletin boards.
    await loader.writeFlash({fileArray:files,flashSize:`${selected.flashMB}MB`,flashMode:'dio',flashFreq:'80m',eraseAll:false,compress:true,
      calculateMD5Hash:image=>md5(image),reportProgress:(i,written,total)=>{ $('progress').value=Math.round((i+written/total)/files.length*100); }});
    let reset=true;
    try {await loader.after('hard_reset');} catch(e){reset=false;log('Flash verified; automatic reset unavailable: '+e.message);}
    $('progress').value=100;
    status('MESHBBS installed',reset?'The firmware was written and verified. Give the board a moment to start, then finish setup in the app.':'The firmware was written and verified. Tap RST once, then finish setup in the app.','INSTALL COMPLETE');
    $('next').hidden=false;
    await disconnect();
    $('connect').textContent='Connect another board';
  } catch(e){status('Install did not finish','Check the cable, put the board in the bootloader if needed, then use Connect USB board to try again. '+e.message,'NEEDS ATTENTION');log(e.message);await disconnect();}
  finally{busy=false;$('disconnect').disabled=false;update();}
}
$('connect').addEventListener('click',connect);$('install').addEventListener('click',install);
$('disconnect').addEventListener('click',async()=>{await disconnect();status('Board disconnected','You can select another board or connect again.');});
$('choices').addEventListener('change',event=>{
  if(event.target.id==='memory' || event.target.id==='led') $('board-match').checked=false;
  update();
});
$('copy').addEventListener('click',async()=>{try{await navigator.clipboard.writeText($('log').textContent);$('copy').textContent='Copied';}catch{status('Copy unavailable','Select and copy the text under Installation details.');}});
window.addEventListener('beforeunload',e=>{if(busy){e.preventDefault();e.returnValue='';}});
try{
  const r=await fetch('./firmware-manifest.json',{cache:'no-store'});if(!r.ok)throw new Error('Firmware catalog not found.');
  catalog=await r.json();for(const p of catalog.profiles)validateManifest(p);
  $('apk-link').href='./MESHBBS-0.5.0.apk';
  if(!navigator.serial || !isSecureContext)status('Open in desktop Chrome or Edge','USB installation needs a supported browser over HTTPS or localhost. You can still download the firmware and app from the project releases.','BROWSER CHECK');
  update();
}catch(e){status('Firmware catalog unavailable','Reload the page or use the release download on GitHub.','NEEDS ATTENTION');log(e.message);}
