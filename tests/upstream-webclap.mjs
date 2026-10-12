// Independent host qualification. No SynthCity runtime is loaded by this test.
import {createServer} from 'node:http';
import {readFile} from 'node:fs/promises';
import {resolve,sep} from 'node:path';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {chromium} from 'playwright';
const upstream=resolve(process.argv[2]??'.cache/webclap-host');
const revision='b42ade615bef2c989f96400a7b0b2ef15cabd396';
if(execFileSync('git',['-C',upstream,'rev-parse','HEAD'],{encoding:'utf8'}).trim()!==revision) throw Error(`Expected upstream host ${revision}`);
const dependencies=execFileSync('git',['-C',upstream,'submodule','status','--recursive','clap-audionode/wclap-js'],{encoding:'utf8'});
if(dependencies.split('\n').filter(Boolean).some(line=>line[0]!==' ')) throw Error('Initialize the pinned upstream host submodules');
if(execFileSync('git',['-C',upstream,'status','--porcelain','--untracked-files=no'],{encoding:'utf8'}).trim()) throw Error('Use an unmodified upstream host checkout');
const root=resolve(upstream,'clap-audionode');
const binary=process.argv[3]?resolve(process.argv[3]):new URL('../examples/cpp-midside/build/wasm/midside.wasm',import.meta.url);
const pluginBytes=await readFile(binary);
const profiles={
  midside:{id:'org.synthcity.examples.hetrick-midside',param:0,initial:0,changed:-5,expected:[.15,.15],stateBytes:8},
  gain:{id:'org.example.portable-clack-gain',param:1,initial:1,changed:.5,expected:[.1,.05]},
};
const profile=profiles[process.argv[4]??'midside'];
if(!profile)throw Error('Choose the midside or gain qualification fixture');
if(process.argv[5])profile.id=process.argv[5];
const server=createServer(async(req,res)=>{
  try {
    const path=new URL(req.url,'http://localhost').pathname;
    if(path==='/'){res.setHeader('Content-Type','text/html');res.end('<!doctype html><title>Independent WebCLAP qualification</title>');return;}
    let file;
    if(path==='/plugin.wasm') {res.setHeader('Content-Type','application/wasm');res.end(pluginBytes);return;}
    else {
      file=resolve(root,'.'+decodeURIComponent(path));
      if(!file.startsWith(root+sep)||! /\.(mjs|js|wasm)$/.test(file)){res.writeHead(404).end();return;}
    }
    res.setHeader('Content-Type',path.endsWith('.wasm')?'application/wasm':'text/javascript');
    res.end(await readFile(file));
  } catch {res.writeHead(404).end();}
});
await new Promise((ok,no)=>{server.once('error',no);server.listen(0,'127.0.0.1',ok);});
let browser;
try {
  browser=await chromium.launch({headless:true,args:['--autoplay-policy=no-user-gesture-required']});
  const page=await browser.newPage();page.setDefaultTimeout(20000);
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  const result=await page.evaluate(async(profile)=>{
    return await Promise.race([(async()=>{
      const {default:ClapAudioNode}=await import('/clap-audionode.mjs');
      const plugin=new ClapAudioNode('/plugin.wasm');
      const plugins=await plugin.plugins();
      const id=profile.id;
      if(!plugins.some(p=>p.id===id))throw Error('Plugin discovery failed');
      const results=[];
      for(const rate of [44100,48000,96000]) {
        const ctx=new AudioContext({sampleRate:rate});
        try {
          await ctx.resume();
          if(ctx.sampleRate!==rate)throw Error(`Requested ${rate}, got ${ctx.sampleRate}`);
          const node=await plugin.createNode(ctx,id);
          const merger=ctx.createChannelMerger(2),split=ctx.createChannelSplitter(2);
          const sources=[.2,.1].map((value,i)=>{const s=ctx.createConstantSource();s.offset.value=value;s.connect(merger,0,i);s.start();return s;});
          merger.connect(node);node.connect(split);
          const analysers=[0,1].map(i=>{const a=ctx.createAnalyser();a.fftSize=256;split.connect(a,i);return a;});
          const mute=ctx.createGain();mute.gain.value=0;analysers.forEach(a=>a.connect(mute));mute.connect(ctx.destination);
          const checkAudio=async(expected)=>{
            await new Promise(r=>setTimeout(r,100));
            analysers.forEach((a,i)=>{const samples=new Float32Array(a.fftSize);a.getFloatTimeDomainData(samples);if(samples.some(v=>!Number.isFinite(v)||Math.abs(v-expected[i])>1e-5))throw Error(`Audio mismatch at ${rate}: channel ${i}, got ${samples[128]}, expected ${expected[i]}`);});
          };
          await node.setParam(profile.param,profile.initial);await checkAudio([.2,.1]);
          const state=await node.saveState();if(!state||!state.byteLength||(profile.stateBytes&&state.byteLength!==profile.stateBytes))throw Error('Invalid saved state');
          await node.setParam(profile.param,profile.changed);await checkAudio(profile.expected);
          await node.loadState(state);await checkAudio([.2,.1]);
          const value=await node.getParam(profile.param);if(value.value!==profile.initial)throw Error(`State restore failed: ${JSON.stringify(value)}`);
          sources.forEach(s=>s.stop());node.disconnect();
          results.push({rate,discovery:true,audio:true,parameter:true,state:true});
        } finally {await ctx.close();}
      }
      return results;
    })(),new Promise((_,reject)=>setTimeout(()=>reject(Error('Independent host timed out')),18000))]);
  },profile);
  if(errors.length)throw Error(errors.join('\n'));
  console.log(JSON.stringify({host:'WebCLAP/browser-test-host',revision,plugin:profile.id,sha256:createHash('sha256').update(pluginBytes).digest('hex'),results:result},null,2));
} finally {await browser?.close();await new Promise(r=>server.close(r));}
