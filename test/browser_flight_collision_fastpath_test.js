/* The ACTUAL browser manPlow() function, old vs optimized collision loop.
 * Compare hit consequences and man state, not merely standalone geometry. */
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync(require('path').join(__dirname,'../web/index.html'),'utf8');
const a=html.indexOf('function manPlow(){');
const b=html.indexOf('// =====================================================================\n//  WALK MODE',a);
assert(a>=0&&b>a,'production flight physics missing');
const fast=html.slice(a,b);
const original=`  for(const ch of chunks){if(Math.hypot(ch.x-m.x,ch.y-m.y)>ch.rad+R+4)continue;let hit=false,ax=0,ay=0,n=0;
    for(const a of ch.cells){if(a.dead||Math.hypot(a.wx-m.x,a.wy-m.y)>=R+RC)continue;`;
const optimized=`  /* Flying physics runs eight substeps each update. Reject far chunks and
     cells using two cheap axes before the original exact hypot check.
     Preserve strict '>' for chunk range and '>=' for individual contacts. */
  const cellReach=R+RC;
  for(const ch of chunks){
    const cdx=ch.x-m.x,cdy=ch.y-m.y,chunkReach=ch.rad+R+4;
    if(Math.abs(cdx)>chunkReach||Math.abs(cdy)>chunkReach||
       Math.hypot(cdx,cdy)>chunkReach)continue;
    let hit=false,ax=0,ay=0,n=0;
    for(const a of ch.cells){
      if(a.dead)continue;
      const dx=a.wx-m.x,dy=a.wy-m.y;
      if(Math.abs(dx)>=cellReach||Math.abs(dy)>=cellReach||
         Math.hypot(dx,dy)>=cellReach)continue;`;
assert(fast.includes(optimized),'fast collision code not found');
const reference=fast.replace(optimized,original);
let seed=0x3a4b5511;
const rnd=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296};
const initial=[];
const ch0={x:100,y:100,rad:160,ang:0,vx:0,vy:0,w:0,hitCd:1,cells:[]};
for(let i=0;i<240;i++)ch0.cells.push({
  wx:100+(rnd()-.5)*240,wy:100+(rnd()-.5)*240,
  h:10000,m:1,dead:i%47===0
});
ch0.cells.push(...[19.75,20,20.1,-19.75,-20,-20.1].map(x=>({
  wx:100+x,wy:100,h:10000,m:1,dead:false
})));
initial.push(ch0);
for(let k=0;k<58;k++){
  const x=400+rnd()*1600,y=-400+rnd()*1700,ch={
    x,y,rad:45,vx:0,vy:0,w:0,hitCd:1,cells:[]
  };
  for(let i=0;i<22;i++)ch.cells.push({
    wx:x+(rnd()-.5)*54,wy:y+(rnd()-.5)*54,
    h:10000,m:1,dead:false
  });
  initial.push(ch);
}
const setup=`
const V=8,RC=4,K=17,RUBBER=7,COLS=120,ROWS=60;
const grid=new Uint8Array(COLS*ROWS);
const MAT=[{hp:1,c:'#000'},{hp:10000,c:'#fff'}];
const chunks=JSON.parse(JSON.stringify(fixture)),targets=[];
const man={x:100,y:100,R:16,vx:60,vy:40,dive:false,inf:0,bel:0,hitCd:0,mode:'fly'};
let shake=0,freeze=0,stuck=0,dustCount=0,sfx=0;
const idx=(x,y)=>y*COLS+x,inb=()=>false,clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
const bounceRubber=()=>{},chip=()=>{},rnd=()=>.5;
const applyImpulse=()=>{},wobKick=()=>{},spark=()=>{};
const dust=()=>{dustCount++},stick=()=>{stuck++;man.mode='stuck'};
const SFX={crunch(){sfx++}};
`;
function run(method){
  let calls=0;
  const tracked=Object.create(Math);
  tracked.hypot=(...args)=>{calls++;return Math.hypot(...args)};
  const state=vm.runInNewContext(setup+method+`
manPlow();
JSON.stringify({man,head:chunks[0],tail:chunks[1],shake,freeze,stuck,dustCount,sfx});
`,{Math:tracked,fixture:initial});
  return {state,calls};
}
const beforeState=run(reference),afterState=run(fast);
assert.strictEqual(afterState.state,beforeState.state,'flight collision behavior changed');
assert(afterState.calls<beforeState.calls*0.35,
       'early rectangle gate did not eliminate distant hypot calls');
console.log('browser flight collision: '+beforeState.calls+' hypot calls -> '+
  afterState.calls+'; identical impacted cells, HP, momentum, and state PASSED');
