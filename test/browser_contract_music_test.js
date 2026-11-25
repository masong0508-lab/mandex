/* Validate real WAV content and the production lazy-stream/fallback controller. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const dir=path.join(__dirname,'../web/audio'),manifest=JSON.parse(fs.readFileSync(path.join(dir,'manifest.json'),'utf8'));
const keys=new Set();let bytes=0,seconds=0;
function tag(b,i,n){return String.fromCharCode(...b.subarray(i,i+n))}
for(const track of manifest.tracks){
 const b=fs.readFileSync(path.join(dir,track.id+'.wav')),v=new DataView(b.buffer,b.byteOffset,b.byteLength);
 assert(tag(b,0,4)==='RIFF'&&tag(b,8,8)==='WAVEfmt '&&tag(b,36,4)==='data','valid PCM WAV header');
 assert(v.getUint32(4,true)===b.length-8&&v.getUint32(40,true)===b.length-44,'correct WAV chunk sizes');
 assert(v.getUint16(20,true)===1&&v.getUint16(22,true)===1&&v.getUint16(34,true)===16&&v.getUint32(24,true)===22050,'light mono sample format');
 assert(b.length===track.bytes,'asset size matches manifest');
 let peak=0,sum=0,hash=2166136261;
 for(let k=44;k<b.length;k+=2){const a=v.getInt16(k,true)/32768;peak=Math.max(peak,Math.abs(a));sum+=a*a;hash=Math.imul(hash^v.getUint16(k,true),16777619)>>>0}
 const rms=Math.sqrt(sum/((b.length-44)/2));
 assert(peak>.6&&peak<.95&&rms>.08&&rms<.35,'audible normalized music without clipping');
 assert(v.getInt16(44,true)===0&&v.getInt16(b.length-2,true)===0,'loop edges taper to zero');
 assert(!keys.has(hash),'distinct music, not duplicate padding');keys.add(hash);
 bytes+=b.length;seconds+=(b.length-44)/(22050*2);
}
assert(manifest.tracks.length===6&&bytes>=6500000&&seconds>150,'real new audio meets content target');
const a=html.indexOf('function contractVolume(){'),b=html.indexOf('function noise(',a);
assert(a>=0&&b>a,'actual soundtrack controller');
const run=new Function('assert',`
 let level=0,o={},contractAudio=null,contractAudioKey='',contractAudioState='idle',volume=.8;
 const CONTRACT_TRACKS=['permit-panic','concrete-shuffle','final-notice-funk','overtime-drive','night-shift-shuffle','crown-finale'];
 const OPT=()=>o,optVol=()=>volume,created=[];
 class Audio{
  constructor(){this.pauseCount=0;this.reloads=0;this.events={};created.push(this)}
  addEventListener(name,fn){this.events[name]=fn}
  play(){return {then:(yes,no)=>{this.yes=yes;this.no=no}}}
  pause(){this.pauseCount++}
  removeAttribute(name){if(name==='src')this.src=''}
  load(){this.reloads++}
 }
`+html.slice(a,b)+`
 assert(!syncContractMusic(true)&&created.length===0,'no assets on original campaign');
 level=60;assert(!syncContractMusic(true)&&created.length===1,'one lazy track while loading');
 const first=created[0];assert(first.src==='audio/permit-panic.wav'&&first.loop&&first.preload==='none','loop and relative path');
 assert(Math.abs(first.volume-.56)<1e-9,'master volume honored');
 for(let k=0;k<100;k++)syncContractMusic(true);
 assert(created.length===1,'no media churn per frame');
 first.yes();assert(syncContractMusic(true),'loaded track replaces synthesizer');
 level=61;assert(syncContractMusic(true)&&created.length===1,'same chapter region shares a track');
 level=66;assert(!syncContractMusic(true)&&created.length===2&&first.pauseCount===1&&first.src==='','switch releases prior source');
 const second=created[1];second.no();
 assert(!syncContractMusic(true)&&created.length===2&&contractAudioState==='fallback','missing or blocked asset falls back without retries');
 level=72;syncContractMusic(true);const third=created[2];third.yes();
 assert(syncContractMusic(true)&&third.src.includes('final-notice-funk'),'third distinct region loads');
 o.contractMusic=false;assert(!syncContractMusic(true)&&third.src==='','manual fallback releases memory');
 o.contractMusic=true;syncContractMusic(true);const fourth=created[3];
 stopContractAudio();fourth.yes();
 assert(contractAudio===null&&contractAudioState==='idle','stale play callback cannot resurrect stopped media');
 assert(!syncContractMusic(false)&&created.length===4,'hidden or muted state does not load');
 o.contractMusic=true;level=76;syncContractMusic(true);created[4].yes();assert(syncContractMusic(true)&&created[4].src.includes('overtime-drive'),'overtime track');
 level=79;syncContractMusic(true);created[5].yes();assert(syncContractMusic(true)&&created[5].src.includes('night-shift-shuffle'),'night track');
 level=82;syncContractMusic(true);created[6].yes();assert(syncContractMusic(true)&&created[6].src.includes('crown-finale'),'finale track');
 level=84;assert(!syncContractMusic(true)&&created[6].src==='','custom/noncampaign fallback');
 return created.length;
`);
const opens=run(assert);
console.log('BROWSER_CONTRACT_MUSIC_PASS bytes='+bytes+' seconds='+seconds.toFixed(2)+' distinct='+keys.size+' lazy_opens='+opens);
