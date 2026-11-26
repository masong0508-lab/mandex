'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
function expansionRegression(html,assert,section){
 const cut=(a,b)=>{const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,'source section '+a);return html.slice(i,j)};
 if(!section||section==='starters'){
   const create=new Function(`
     const ROWS=60,EMIN=28,COLS=120;const E={g:new Uint8Array(7200),t:[],men:5,undo:[],redo:[],starters:true,id:1,name:'Original'};
     const toast=()=>{},buttons=[],panel=()=>{},txt=()=>{},btn=(x,y,w,h,n,fn)=>buttons.push({x,y,w,h,n,fn});
     ${cut('function encodeCells(','function encodeSharedLevel(')}
     ${cut('function eState(){','function eCell(')}
     return {E,buttons,patterns:E_STARTERS,pattern:eStarterPattern,apply:eApplyStarter,undo:eUndo,draw:drawStarterMenu,validate:validateSharedLevel};
   `)();
   assert(create.patterns.length===6,'six starter structures');
   const seen=new Set();
   for(let kind=0;kind<6;kind++){
     const a=create.pattern(kind),b=create.pattern(kind);
     assert(JSON.stringify(a.level)===JSON.stringify(b.level),'deterministic starter');
     assert(JSON.stringify(a.level)===JSON.stringify(create.validate(a.level)),'shareable starter');
     seen.add(JSON.stringify(a.level.c));
     if(kind>=3){
       for(const [cx,row] of a.level.t){
         for(let y=row-5;y<row;y++)for(let x=cx-2;x<=cx+2;x++)assert(!a.grid[y*120+x],'new starter landlord clearance');
         assert(row===60||a.grid[row*120+cx]>0,'new starter landlord has floor');
       }
       assert(a.grid.slice(59*120).some(x=>x!==0),'new starter grounded foundation');
     }
     create.apply(kind);assert(create.E.id===-1,'new design cannot overwrite old project');create.undo();assert(create.E.id===1,'undo restores project identity');
   }
   assert(seen.size===6,'six distinct structures');create.draw(0,0,416);
   assert(create.buttons.length===7,'all six designs plus cancel accessible');
   for(const b of create.buttons)assert(b.x>=0&&b.x+b.w<=416&&b.y+b.h<=256,'desktop starter buttons fit');
 }
 if(!section||section==='particles'){
   const run=new Function(`
     const parts=[],texts=[],draws=[];let time=0;
     const ctx=new Proxy({}, {get(t,k){if(k in t)return t[k];return (...a)=>draws.push([k,...a])},set(t,k,v){t[k]=v;return true}});
     const txt=(...a)=>draws.push(['txt',...a]);
     ${cut('function drawParts(','function drawTexts(){')}
     return {parts,draws,draw:drawParts};
   ` .replace("undefined","undefined"));
   const p=run();
   p.parts.push({k:0,x:50,y:50,s:4,l:.5,l0:1,c:'red'},{k:0,x:10000,y:50,s:4,l:.5,l0:1,c:'red'},{k:2,x:140,y:50,s:3,vx:1800,vy:0,l:.5,l0:1});
   p.draw();const all=p.draws.slice();p.draws.length=0;
   p.draw(0,100,0,100);const trimmed=p.draws.slice();
   assert(all.filter(x=>x[0]==='fillRect').length===2,'unbounded title effects unchanged');
   assert(trimmed.filter(x=>x[0]==='fillRect').length===1,'offscreen particle skipped');
   assert(trimmed.some(x=>x[0]==='lineTo'&&x[1]<100),'long spark crossing edge remains visible');
   assert(p.parts.length===3&&p.parts[1].x===10000,'render culling never changes simulation');
 }
 if(!section||section==='animation'){
   const make=new Function(`
     const V=8,G=800,WHH=19,WHW=7,WW=960,RUBBER=7,KEYS={},targets=[],grid=new Uint8Array(7200);
     const idx=(x,y)=>y*120+x,inb=()=>false,clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
     const wBlocked=(x,y)=>y>250,rnd=()=>1,SFX={pop(){},thud(){},whoosh(){}};
     const dust=()=>{},popText=()=>{},ring=()=>{},walkQuit=()=>{},walkPunch=()=>{},walkSlam=()=>{},killTarget=()=>{};
     let time=0,man={mode:'walk',x:120,y:245,vy:420,vx:0,gr:false,wt:600,face:1,turn:0,gait:0,stomp:false,pCd:0,pT:0,padCd:0,ledgeGrace:0,jumpHold:0};
     ${cut('function walkStep(dt){','/* Match the console sprite turnaround:')}
     ${cut('function walkTurnAngle(','function drawWalkMan(m){')}
     return {man,tick:walkStep,squash:()=>typeof walkLandingSquash==='function'?walkLandingSquash(man):0,pitch:()=>typeof walkAirPitch==='function'?walkAirPitch(man):null};
   `)();
   make.tick(1/60);assert(make.man.gr&&make.man.vy===0,'physical hard landing still resolves');
   const start=make.squash();assert(start>0&&start<=1,'hard landing starts bounded squash');
   for(let i=0;i<12;i++)make.tick(1/60);
   assert(make.squash()===0,'landing recovers to neutral');
   make.man.gr=false;make.man.vy=-1;const before=make.pitch();make.man.vy=1;const after=make.pitch();
   assert(before!==null&&after!==null&&Math.abs(after-before)<.01,'smooth air pitch through apex');
 }
 return 'WEB_EXPANSION_PASS '+(section||'starters particles animation');
}
console.log(expansionRegression(fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8'),assert));
