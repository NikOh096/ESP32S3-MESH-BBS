import {build} from 'esbuild';
import {cp,mkdir,readFile,writeFile} from 'node:fs/promises';
import {validateManifest,verifyBytes} from './src/safety.mjs';
await mkdir('dist',{recursive:true});
const catalog=JSON.parse(await readFile('public/firmware-manifest.json','utf8'));
for(const p of catalog.profiles){validateManifest(p);for(const part of p.parts)await verifyBytes(await readFile('public/'+part.path),part);}
await build({entryPoints:['src/app.mjs'],outfile:'dist/app.js',bundle:true,format:'esm',target:['chrome100','edge100'],minify:true,legalComments:'eof'});
for(const f of ['index.html','style.css'])await cp(f,'dist/'+f);
await cp('public','dist',{recursive:true});
await writeFile('dist/.nojekyll','');
console.log(`Built installer with ${catalog.profiles.length} verified firmware profiles.`);
