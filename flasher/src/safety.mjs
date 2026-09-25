export const offsets = [0, 0x8000, 0x10000];
export function validateDevice(device, profile) {
  if (device.chip !== 'ESP32-S3') throw new Error('This installer needs an ESP32-S3 BBS board. Your mesh radio uses its own firmware.');
  if (device.flash !== `${profile.flashMB}MB`) throw new Error(`This board has ${device.flash || 'unknown'} flash. Select the matching memory size before installing.`);
  if (!device.features.some(s => s.startsWith('Embedded PSRAM 8MB'))) throw new Error('Could not confirm the required 8 MB PSRAM. Use a listed N8R8 or N16R8 module. No firmware was written.');
}
export function validateManifest(profile) {
  if (!profile || ![8,16].includes(profile.flashMB) || ![38,48].includes(profile.ledGPIO)
      || !['meshtastic','meshcore'].includes(profile.protocol) || profile.chip !== 'ESP32-S3'
      || !Array.isArray(profile.parts) || profile.parts.length !== 3) throw new Error('The firmware catalog is invalid.');
  for(let i=0;i<3;i++) {
    const part=profile.parts[i];
    if(part.address !== offsets[i] || !/^[a-f0-9]{64}$/.test(part.sha256) || !Number.isInteger(part.size) || part.size<=0
        || !/^firmware\/[a-z0-9/-]+\.bin$/.test(part.path)
        || part.size > (i===0 ? 0x8000 : i===1 ? 0x1000 : 0x300000)) throw new Error('The firmware layout is invalid.');
  }
}
export async function verifyBytes(bytes, part, cryptoApi = globalThis.crypto) {
  if(bytes.length!==part.size) throw new Error('A firmware download is incomplete. Please connect again.');
  const digest=new Uint8Array(await cryptoApi.subtle.digest('SHA-256',bytes));
  const hex=Array.from(digest,b=>b.toString(16).padStart(2,'0')).join('');
  if(hex!==part.sha256) throw new Error('The firmware integrity check failed. Nothing was written. Download a fresh release.');
}
