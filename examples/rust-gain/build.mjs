// Local authoring recipe. Does not install or distribute plugins.
import {spawnSync} from 'node:child_process';
import {mkdirSync,copyFileSync,writeFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import {join} from 'node:path';
const root=fileURLToPath(new URL('.',import.meta.url));
const run=(args,env=process.env)=>{const r=spawnSync('cargo',args,{cwd:root,env:{...env,CARGO_TARGET_DIR:join(root,'target')},stdio:'inherit'});if(r.error)throw r.error;if(r.status!==0)process.exit(r.status??1);};
if(!['darwin','linux'].includes(process.platform))throw Error('This bundle recipe is qualified for macOS/Linux layout; Windows packaging is not implemented.');
run(['build','--release','--locked']);
run(['build','--release','--locked','--target','wasm32-unknown-unknown'],{...process.env,RUSTFLAGS:[process.env.RUSTFLAGS,'-C link-arg=--export-table -C link-arg=--growable-table'].filter(Boolean).join(' ')});
const dist=join(root,'dist');mkdirSync(dist,{recursive:true});
const bundle=join(dist,'portable-clack-gain.wclap');mkdirSync(bundle,{recursive:true});
copyFileSync(join(root,'target/wasm32-unknown-unknown/release/portable_clack_gain.wasm'),join(bundle,'module.wasm'));
for(const name of ['LICENSE-MIT','LICENSE-APACHE']){copyFileSync(join(root,name),join(bundle,name));copyFileSync(join(root,name),join(dist,name));}
if(process.platform==='darwin'){
  const contents=join(dist,'Portable Clack Gain.clap/Contents');mkdirSync(join(contents,'MacOS'),{recursive:true});
  copyFileSync(join(root,'target/release/libportable_clack_gain.dylib'),join(contents,'MacOS/portable-clack-gain'));
  writeFileSync(join(contents,'Info.plist'),`<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>org.example.portable-clack-gain</string>
<key>CFBundleExecutable</key><string>portable-clack-gain</string>
<key>CFBundleName</key><string>Portable Clack Gain</string>
<key>CFBundlePackageType</key><string>BNDL</string>
<key>CFBundleVersion</key><string>0.1.0</string>
<key>CFBundleShortVersionString</key><string>0.1.0</string>
</dict></plist>\n`);
}else copyFileSync(join(root,'target/release/libportable_clack_gain.so'),join(dist,'portable-clack-gain.clap'));
console.log(`Local CLAP and WebCLAP artifacts: ${dist}`);
