"""Create a dated compatibility snapshot from downloaded upstream manifests."""
import configparser, json, pathlib, re
ROOT=pathlib.Path(__file__).resolve().parents[1]
MC=ROOT/'.reference/meshcore'
ref=json.loads((MC/'reference.json').read_text(encoding='utf-8'))
release=json.loads((MC/'release.json').read_text(encoding='utf-8'))
sections={}; sources={}
for p in [MC/'platformio.ini', *sorted((MC/'variants').glob('*/platformio.ini'))]:
    c=configparser.ConfigParser(interpolation=None, strict=False, inline_comment_prefixes=(';',))
    c.read(p,encoding='utf-8')
    for name in c.sections():
        sections[name]=dict(c[name]); sources[name]=p.relative_to(MC).as_posix()
def resolve(section,key,seen=()):
    if section in seen or section not in sections: return ''
    row=sections[section]; value=row.get(key)
    if value is None:
        return '\n'.join(resolve(parent.strip(),key,seen+(section,)) for parent in row.get('extends','').split(','))
    return re.sub(r'\$\{([^}.]+)\.([^}]+)\}',lambda m:resolve(m[1],m[2],seen+(section,)),value)
def nice(name):
    return name.replace('_',' ').replace('LilyGo','LILYGO').replace('heltec','Heltec').replace('rak','RAK')
radios=[]
for section in sorted(sections,key=str.lower):
    if not section.startswith('env:') or 'companion_radio_ble' not in section: continue
    env=section[4:]; flags=resolve(section,'build_flags'); board=resolve(section,'board')
    cpu='nRF52840' if 'nrf52' in resolve(section,'platform').lower() or 'NRF52' in flags else 'ESP32 family'
    pins=re.findall(r'-D\s*BLE_PIN_CODE=(\d+)',flags)
    display=re.findall(r'-D\s*DISPLAY_CLASS=(\w+)',flags)
    has_display=bool(display and display[-1]!='NullDisplayDriver')
    pin='Displayed random PIN or configured PIN' if has_display else 'Configured PIN; stock default 123456'
    if pins and pins[-1] != '123456': pin='Build-specific fixed PIN; check the radio configuration'
    assets=[a['name'] for a in release['assets'] if a['name'].startswith(env+'-v')]
    note=pin+'. Companion BLE required; check firmware variant and local radio settings.'
    label=nice(env.split('_companion_radio_ble')[0])
    if 'Heltec v3' in label: note+=' Keep the BBS close during pairing; Bluetooth range may be limited.'
    if any(w in label.lower() for w in ['deck','pager']): note+=' Standalone UI builds are a different firmware mode.'
    radios.append(dict(name=label,protocol='MeshCore',model=env,cpu=cpu,board=board,
        display='Display driver included; detection at boot still matters' if has_display else 'No display in this build',
        pin=pin,status='Upstream BLE release; BBS hardware test pending' if assets else 'Upstream BLE source target; release binary not found; untested',
        note=note,source=f'https://github.com/meshcore-dev/MeshCore/blob/{ref["commit"]}/{sources[section]}',release_assets=assets))
mt_source=json.loads((ROOT/'.reference/meshtastic-hardware-list.json.source.json').read_text())
mt_url=f'https://github.com/{mt_source["repo"]}/blob/{mt_source["commit"]}/{mt_source["path"]}'
mt=json.loads((ROOT/'.reference/meshtastic-hardware-list.json').read_text(encoding='utf-8'))
for row in mt:
    cpu=row.get('architecture','unknown'); possible=cpu.startswith(('esp32','nrf52'))
    status='Bluetooth API candidate; hardware test pending' if possible else 'Not supported by this Bluetooth BBS connection'
    if not row.get('activelySupported'): status+='; upstream lists this model inactive'
    note='Display and saved Bluetooth settings determine the PIN; no-screen default is usually 123456. Wi-Fi can disable Bluetooth on ESP32.' if possible else 'This build needs a radio with the supported Bluetooth client service. USB-only and network-only radios are outside this release.'
    name=row['displayName']
    if 'T-Deck' in name or 'T-Watch' in name or 'Pager' in name: note+=' Use Bluetooth programming mode or BaseUI if the standalone UI occupies the client API.'
    if row['hwModel']==43: note+=' Earlier Meshtastic BBS pairing and messaging worked with the owner’s Heltec V3; this release still needs regression testing.'
    if row['hwModel']==139: note+=' MeshTower V2 is an nRF52840 solar product, not the older ESP32 LoRa32 V2.'
    radios.append(dict(name=name,protocol='Meshtastic',model=row['hwModelSlug'],hw=row['hwModel'],cpu=cpu,status=status,note=note,source=mt_url))
data=dict(as_of=json.loads((ROOT/'docs/upstream-lock.json').read_text())['as_of'],meshcore=ref,meshtastic=mt_source,
    scope='Upstream targets and metadata, not a claim that every model has been tested with MESHBBS.',radios=radios)
out=ROOT/'docs/radio-catalog.json';out.write_text(json.dumps(data,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
assets=ROOT/'android/app/src/main/assets';assets.mkdir(parents=True,exist_ok=True)
(assets/'radio-catalog.json').write_bytes(out.read_bytes())
java=['package com.meshbbs.setup;', '/** Hardware IDs reported by Meshtastic; never inferred from a Bluetooth name. */','public final class RadioModels {','  private RadioModels() {}','  public static String name(int id) {','    switch(id) {']
names={}
for row in mt: names.setdefault(row['hwModel'],[]).append(row['displayName'])
for number, labels in names.items():
    label=' / '.join(dict.fromkeys(labels))
    java.append(f'      case {number}: return {json.dumps(label,ensure_ascii=True)};')
java+=['      default: return "Radio model " + id + " (not in this catalog)";','    }','  }','}']
(ROOT/'android/app/src/main/java/com/meshbbs/setup/RadioModels.java').write_text('\n'.join(java)+'\n',encoding='utf-8')
lines=['# Radio model catalog','',f'Snapshot: {data["as_of"]}. MeshCore target: {ref["tag"]}. These are upstream build and metadata records, **not hardware certification**. See [compatibility notes](RADIO-COMPATIBILITY.md) for connection requirements.','',
'| Protocol | Radio / variant | Evidence | PIN / connection notes |','| --- | --- | --- | --- |']
for e in radios: lines.append(f'| {e["protocol"]} | {e["name"]} | [{e["status"]}]({e["source"]}) | {e["note"]} |')
(ROOT/'docs/RADIO-MODELS.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print(f'Catalog: {len(radios)} records ({len(radios)-len(mt)} MeshCore BLE targets, {len(mt)} Meshtastic hardware records)')
