import {test} from 'node:test';import assert from 'node:assert/strict';import {webcrypto}from'node:crypto';
import{validateDevice,validateManifest,verifyBytes}from'../src/safety.mjs';
const profile={chip:'ESP32-S3',flashMB:16,ledGPIO:48,protocol:'meshtastic',parts:[0,0x8000,0x10000].map(address=>({address,path:'firmware/test/firmware.bin',size:1,sha256:'0'.repeat(64)}))};
test('Reject a wrong chip, flash size or missing PSRAM before any write',()=>{
 const d={chip:'ESP32-S3',flash:'16MB',features:['Embedded PSRAM 8MB (AP_3v3)']};validateDevice(d,profile);
 assert.throws(()=>validateDevice({...d,chip:'ESP32'},profile));assert.throws(()=>validateDevice({...d,flash:'8MB'},profile));assert.throws(()=>validateDevice({...d,features:[]},profile));
});
test('Reject NVS overlap, arbitrary URLs and malformed firmware profiles',()=>{
 validateManifest(profile);
 for(const replacement of [{address:0x9000},{size:0x300001},{path:'https://example.com/bad.bin'},{sha256:'bad'}]){
  const p=structuredClone(profile);Object.assign(p.parts[2],replacement);assert.throws(()=>validateManifest(p));
 }
});
test('Downloaded bytes must match size and SHA-256',async()=>{
 const bytes=new TextEncoder().encode('abc');const part={size:3,sha256:'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'};
 await verifyBytes(bytes,part,webcrypto);await assert.rejects(verifyBytes(new Uint8Array([1,2,3]),part,webcrypto));await assert.rejects(verifyBytes(bytes,{...part,size:4},webcrypto));
});
