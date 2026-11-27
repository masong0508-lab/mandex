/* Extract and test the real browser collapse, fracture and rubble solver. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function section(a,b){const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,'production section missing: '+a);return html.slice(i,j)}
const support=section('const SUPPORT_QUEUE=[]','function recompute(ch){');
const joint=section('function jointBreakLimit(','function wobStep(dt){');
const collision=section('const CHUNK_HASH=new Map()','function cleanChunks(dt){');
const fixture="\nconst V=8,ROWS=60,RC=4,G=0,TNT=5,SAND=8,RUBBER=7;\nconst MAT=[null,{hp:20,d:.6,lim:6,ymax:.9},{hp:40,d:1,lim:9,ymax:.3},\n{hp:85,d:1.6,lim:14,ymax:.26},{hp:5,d:.5,lim:2,ymax:.12},\n{hp:8,d:.8,lim:2,ymax:.4},{hp:150,d:2,lim:16,ymax:1.3},\n{hp:60,d:.7,lim:8,ymax:9},{hp:12,d:1.4,lim:8,ymax:99}];\nlet COLS=120,grid,hb,hp,anch,brk,ox,oy,ovx,ovy;\nlet needSupport=false,paperDirty=false,chunks=[],staticContacts=0;\nconst DX=[1,-1,0,0],DY=[0,0,1,-1];\nconst idx=(x,y)=>y*COLS+x,inb=(x,y)=>x>=0&&y>=0&&x<COLS&&y<ROWS;\nconst clamp=(v,a,b)=>Math.max(a,Math.min(b,v));\nconst detached=[],paperHit=()=>{},wobZero=()=>{},clearLinks=()=>{};\nconst newChunk=cells=>{detached.push(cells)};\nconst impactDamage=()=>{},wobKick=()=>{},lordShock=()=>{},movedTntBoom=()=>{};\nconst resolveStatic=()=>{staticContacts++},wake=ch=>{ch.rest=false;ch.sleep=0};\nconst SFX={thud:()=>{}};\nfunction resize(cols){COLS=cols;const n=COLS*ROWS;\n grid=new Uint8Array(n);hb=new Uint8Array(n);hp=new Float32Array(n);\n anch=new Uint8Array(n);brk=new Uint8Array(n);ox=new Float32Array(n);\n oy=new Float32Array(n);ovx=new Float32Array(n);ovy=new Float32Array(n);\n}\nfunction put(x,y,mat=1){grid[idx(x,y)]=mat;hp[idx(x,y)]=MAT[mat].hp}\nfunction body(id,x,y,rest){\n const ch={id,x,y,vx:0,vy:0,w:0,ang:0,cs:1,sn:0,rest,\n           cx:0,cy:0,cn:0,m:10,I:100,age:0,tntImpact:0};\n ch.cells=[{m:6,dead:false,h:150,lx:0,ly:0,wx:x,wy:y,ch}];\n return ch;\n}\n";
const cases="\nlet checks=0;\nfunction check(ok,msg){assert(ok,msg);checks++}\nresize(120);put(0,59);for(let y=10;y<30;y++)for(let x=45;x<75;x++)put(x,y);\nsupportPass();\ncheck(detached.length===1&&detached[0].length===512,'first 512 unsupported voxels drop');\ncheck(needSupport,'oversized component keeps support work scheduled');\nlet remaining=0;for(const m of grid)remaining+=m!==0;\ncheck(remaining===89,'staged detachment retains all later cells');\nfor(let k=0;k<8&&needSupport;k++)supportPass();\nlet fallen=0;for(const comp of detached)fallen+=comp.length;\ncheck(fallen===600,'all unsupported cells eventually detach');\ncheck(!needSupport,'large support pass settles after remainder falls');\nlet left=0;for(const m of grid)left+=m!==0;\ncheck(left===1,'ground cell stays grounded, no unsupported cells hover');\nresize(120);put(40,59);put(40,58);supportPass();\ncheck(detached.reduce((n,c)=>n+c.length,0)===600,'grounded column stays upright');\nresize(120);put(30,30);put(31,30);\nconst i=idx(30,30),j=idx(31,30);\nconst normal=jointBreakLimit(i,j,1,1);\nhp[j]=10;const weak=jointBreakLimit(i,j,1,1);\nhp[j]=5;const critical=jointBreakLimit(i,j,1,1);\ncheck(normal>weak&&weak>critical,'damaged voxel joints weaken gradually');\nresize(360);put(0,59);\nfor(let y=10;y<30;y++)for(let x=315;x<345;x++)put(x,y);\nlet count=detached.length;supportPass();\ncheck(detached.length===count+1&&detached[count].length===512,'360-column finale splits huge rubble');\nfor(let k=0;k<8&&needSupport;k++)supportPass();\nfallen=0;for(let k=count;k<detached.length;k++)fallen+=detached[k].length;\ncheck(fallen===600,'wide-stage remainder drops without clipping');\nresize(120);\nconst a=body(1,100,100,false),b=body(2,104,100,true);\na.vx=65;chunks=[a,b];stepChunks(0);\ncheck(!b.rest,'hard collision wakes sleeping rubble');\nconst c=body(3,100,100,false),d=body(4,104,100,true);\nc.vx=15;chunks=[c,d];stepChunks(0);\ncheck(d.rest,'gentle tap does not wake resting rubble');\ncheck(CHUNK_HASH.size===0,'existing optimized hash still clears between steps');\ncheck(staticContacts===0,'test contact scenes avoid static geometry');\nreturn checks;\n";
const n=new Function('assert',fixture+support+joint+collision+cases)(assert);
const i=html.indexOf('<script>'),j=html.indexOf('</script>',i);assert(i>=0&&j>i,'standalone game script missing');
new Function(html.slice(i+8,j));
console.log('BROWSER_COLLAPSE_RESPONSE_PASS checks='+n+' widths=120,360 syntax=1');

/* The prior solver is frozen here for independent old/new gameplay parity. */
const oldTargets="function updTargets(dt){\n  for(const t of targets){if(!t.alive)continue;t.t+=dt;if(t.inv>0)t.inv-=dt;lordAI(t,dt);\n    const gr=solidAt(t.x,t.y+t.r+1)||solidAt(t.x-4,t.y+t.r+1)||solidAt(t.x+4,t.y+t.r+1);\n    if(gr){if(t.vy>430){if(!collapseSurvive(t))killTarget(t,true);continue}if(t.vy>300&&t.inv<=0){t.stun=.8;t.inv=.8;popText(t.x,t.y-38,'OOF!','#ffd23f',16)}t.vy=0}else{t.vy+=G*dt;t.y+=t.vy*dt}\n    if(t.collapseRolled===2&&t.inv>0)continue; // brief post-escape grace; ordinary hazards resume afterward\n    if(solidAt(t.x,t.y)){   // buried by settling rubble: dig out if there is air within 3 cells above (a light collapse), else crushed\n      let up=0;for(let d=2;d<=V*3;d+=2)if(!solidAt(t.x,t.y-d)&&!solidAt(t.x,t.y-d-t.r)){up=d;break}\n      if(up){t.y-=up;t.vy=0;t.stun=1;t.inv=1;popText(t.x,t.y-38,'PHEW!','#7dff5a',16)}else{if(!collapseSurvive(t))killTarget(t,true);continue}}\n    /* A fallen room may contain hundreds of voxels. The collision radius\n       is constant for this landlord: calculate it once, use an AABB to reject\n       almost every far-away cell before even multiplying the distances. The\n       precise circle hit and rubble velocity still decide actual impacts. */\n    const radius=t.r+RC,rad2=radius*radius;\n    for(const ch of chunks){\n      const cdx=ch.x-t.x,cdy=ch.y-t.y,cr=ch.rad+t.r+4;\n      if(Math.abs(cdx)>cr||Math.abs(cdy)>cr||cdx*cdx+cdy*cdy>cr*cr)continue;\n      let dead=false;\n      for(const a of ch.cells){\n        if(a.dead)continue; // no invisible impacts from shattered rubble\n        const dx=a.wx-t.x,dy=a.wy-t.y;\n        if(Math.abs(dx)>=radius||Math.abs(dy)>=radius)continue;\n        if(dx*dx+dy*dy<rad2){\n          const sp=Math.hypot(ch.vx-ch.w*(a.wy-ch.y),ch.vy+ch.w*(a.wx-ch.x));\n          if(sp>70){\n            if(sp>150){if(!collapseSurvive(t))dead=true;break}\n            if(t.inv<=0){t.inv=.8;t.stun=.9;t.vx=0;popText(t.x,t.y-38,'OOF!','#ffd23f',16);shake=Math.max(shake,4);SFX.thud(.5)}\n          }\n        }\n      }\n      if(t.collapseRolled===2&&t.inv>0)break;\n      if(dead){killTarget(t);break}\n    }}}\n\n";
const oldSand="function sandStep(){let moved=false;const par=sandT&1;\n  for(let y=ROWS-2;y>=0;y--)for(let x=0;x<COLS;x++){const i=y*COLS+x;if(grid[i]!==SAND)continue;\n    let dst=-1;if(!grid[i+COLS])dst=i+COLS;\n    else for(let k=0;k<2&&dst<0;k++){const nx=x+((k^par)?1:-1);if(nx>=0&&nx<COLS&&!grid[y*COLS+nx]&&!grid[(y+1)*COLS+nx])dst=(y+1)*COLS+nx}\n    if(dst<0)continue;\n    grid[dst]=SAND;hp[dst]=hp[i];anch[dst]=0;grid[i]=0;hp[i]=0;anch[i]=0;wobZero(i);wobZero(dst);clearLinks(i);clearLinks(dst);moved=true}\n  sandOn=moved;if(moved)needSupport=true}\n";
const oldCleanup="function cleanChunks(dt){\n  for(let k=chunks.length-1;k>=0;k--){const ch=chunks[k];let lost=false;\n    for(const a of ch.cells)if(a.dead){lost=true;score+=5}\n    if(lost){\n      // Preserve live fragment identity and order without allocating a\n      // replacement array on each impact-heavy frame.\n      let n=0;for(const a of ch.cells)if(!a.dead)ch.cells[n++]=a;\n      ch.cells.length=n;\n      if(!n){chunks.splice(k,1);continue}recompute(ch)\n    }\n    if(ch.hitCd>0)ch.hitCd-=dt;\n    if(ch.tntFused){ch.sleep=0;continue} // the fuse cannot vanish when rubble welds\n    if(ch.x<-250||ch.x>WW+250||ch.y>WH+300){chunks.splice(k,1);continue}\n    const sp=Math.hypot(ch.vx,ch.vy)+Math.abs(ch.w)*18;\n    if(ch.rest){\n      if(!live()||sp>30||(ch.restT+=dt)>2.4+(ch.id%7)*.3)wake(ch);else{ch.vx=ch.vy=ch.w=0;continue}}\n    if(sp>40&&live())for(const o of chunks)if(o.rest){\n      const dx=o.x-ch.x,dy=o.y-ch.y,reach=o.rad+ch.rad+V*2;\n      if(Math.abs(dx)>=reach||Math.abs(dy)>=reach)continue;\n      if(Math.hypot(dx,dy)<reach)wake(o);\n    }\n    if(sp<8&&ch.age>.15){ch.sleep+=dt;if(ch.sleep>.5){\n      if(live()){ch.rest=true;ch.restT=0;ch.vx=ch.vy=ch.w=0}else{settleRaw(ch,false);chunks.splice(k,1)}}}else ch.sleep=0}\n  if(chunks.length>140&&live()){\n    const rs=chunks.filter(c=>c.rest).sort((a,b)=>a.cells.length-b.cells.length);\n    for(const c of rs){if(chunks.length<=120)break;settleRaw(c,false);chunks.splice(chunks.indexOf(c),1)}}}\n\n";
const targetHarness=String.raw`
const V=8,RC=4,G=900;
let targets=[],chunks=[],shake=0,reads=0,events=[];
const lordAI=()=>{},solidAt=(x,y)=>y>=200,collapseSurvive=()=>false;
const popText=(...p)=>events.push(p),SFX={thud:(...p)=>events.push(p)};
const killTarget=t=>{t.alive=false};
function trial(vx,vy,w,rest,dead){
  reads=0;events=[];shake=0;
  const t={x:200,y:192,r:7,alive:true,t:0,inv:0,stun:0,vy:0};
  const cells=Array.from({length:512},(_,i)=>({wx:i===511?200:175,wy:192,dead}));
  const ch={x:184,y:192,rad:32,vx,vy,w,rest,get cells(){reads++;return cells}};
  targets=[t];chunks=[ch];updTargets(1/60);
  return {result:JSON.stringify([t,events,shake]),alive:t.alive,reads};
}`;
const targetBefore=new Function(targetHarness+oldTargets+'\nreturn trial;')();
const targetAfter=new Function(targetHarness+section('function updTargets(dt){','// =====================================================================\n//  UPDATE')+'\nreturn trial;')();
let impactCases=0;
for(const vx of [0,65,71,151,240])for(const vy of [0,80])for(const w of [0,4,12])for(const rest of [false,true])for(const dead of [false,true]){
  const a=targetBefore(vx,vy,w,rest,dead),b=targetAfter(vx,vy,w,rest,dead);
  assert.strictEqual(b.result,a.result,'landlord impact state changed');
  if(vx===0&&vy===0&&w===0)assert(a.reads===1&&b.reads===0,'stationary slab still scanned');
  impactCases++;
}
assert(!targetAfter(0,0,12,false,false).alive,'rotating slab must still hit');
assert(!targetAfter(240,0,0,true,false).alive,'sleep flag must not suppress a fresh impulse');

const sandHarness=String.raw`
const ROWS=60,SAND=8;let COLS,grid,hp,anch,brk,ox,oy,ovx,ovy,sandT,sandOn,needSupport,seed;
const DX=[1,-1,0,0],DY=[0,0,1,-1];
function rnd(){seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296}
function wobZero(i){ox[i]=oy[i]=ovx[i]=ovy[i]=0}
function clearLinks(i){
  const x=i%COLS,y=(i/COLS)|0;brk[i]=0;
  for(let d=0;d<4;d++){const X=x+DX[d],Y=y+DY[d];if(X>=0&&X<COLS&&Y>=0&&Y<ROWS)brk[Y*COLS+X]&=~(1<<(d^1))}
}
function trial(cols,density,s){
  COLS=cols;seed=s;const n=cols*ROWS;
  grid=new Uint8Array(n);hp=new Float32Array(n);anch=new Uint8Array(n);brk=new Uint8Array(n);
  ox=new Float32Array(n);oy=new Float32Array(n);ovx=new Float32Array(n);ovy=new Float32Array(n);
  for(let i=0;i<n;i++)if(rnd()<density){grid[i]=rnd()<.6?SAND:1;hp[i]=rnd()*20;anch[i]=1;brk[i]=rnd()*16|0;ox[i]=oy[i]=ovx[i]=ovy[i]=.25}
  const trace=[];sandOn=true;needSupport=false;
  for(sandT=0;sandT<8;sandT++){sandStep();trace.push([Array.from(grid),Array.from(hp),Array.from(anch),Array.from(brk),Array.from(ox),Array.from(oy),Array.from(ovx),Array.from(ovy),sandOn,needSupport]);}
  return JSON.stringify(trace);
}`;
const sandBefore=new Function(sandHarness+oldSand+'\nreturn trial;')();
const sandAfter=new Function(sandHarness+section('function sandStep(){','// the floor under a landlord')+'\nreturn trial;')();
let sandCases=0;
for(const cols of [120,360])for(const density of [.01,.25,.75,.99])for(const seed of [7,91]){
  assert.strictEqual(sandAfter(cols,density,seed),sandBefore(cols,density,seed),'sand diagonal or edge state changed');
  sandCases++;
}
const cleanHarness=String.raw`
const V=8,WW=960,WH=480;
let chunks,score,settled,recomputed;
const live=()=>true,wake=ch=>{ch.rest=false},settleRaw=ch=>settled.push(ch.id);
const recompute=ch=>{recomputed.push(ch.id);ch.mass=ch.cells.length};
function trial(mask){
  const cells=Array.from({length:8},(_,i)=>({id:i,dead:!!(mask&(1<<i))}));
  const original=cells.slice();
  const ch={id:1,cells,x:200,y:200,vx:0,vy:0,w:0,age:0,hitCd:0,rad:16};
  chunks=[ch];score=settled=0;settled=[];recomputed=[];
  cleanChunks(1/60);
  return {state:JSON.stringify([chunks,score,settled,recomputed]),identity:!chunks.length||chunks[0].cells.every(c=>original[c.id]===c)};
}`;
const cleanBefore=new Function(cleanHarness+oldCleanup+'\nreturn trial;')();
const cleanAfter=new Function(cleanHarness+section('function cleanChunks(dt){','// =====================================================================\n//  THE MAN:')+'\nreturn trial;')();
for(let mask=0;mask<256;mask++){
  const a=cleanBefore(mask),b=cleanAfter(mask);
  assert.strictEqual(a.state,b.state,'rubble cleanup score, order or mass changed');
  assert(b.identity,'live cell identity changed');
}
console.log('BROWSER_RUBBLE_BUDGET_PASS impacts='+impactCases+' sand_worlds='+sandCases+' sand_steps=8 cleanup_masks=256');
