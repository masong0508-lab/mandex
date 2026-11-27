'use strict';
/* Validate all authored trials and both generated runtime representations. */
const assert=require('assert'),fs=require('fs'),path=require('path');
const root=path.join(__dirname,'..');
const pack=require('../tools/trial_pack.js');
const levels=pack.generateTrials();
const out=pack.emitTrials(levels);
assert.strictEqual(levels.length,600);
assert.strictEqual(new Set(levels.map(l=>l.rows.join(''))).size,600);
assert.strictEqual(levels.reduce((n,l)=>n+l.rows.join('').length,0),4320000);
assert(out.commandBytes<600*7200/5,'compact command payload must fit handheld budget');
for(let d=0;d<30;d++){
 const source=fs.readFileSync(path.join(root,'content/trials/district-'+(d+1)+'.json'),'utf8');
 assert.strictEqual(source,out.districts[d],'uncompressed editable source matches generator');
}
assert.strictEqual(fs.readFileSync(path.join(root,'core/trial_levels.h'),'utf8'),out.header,'native data matches source');
const html=fs.readFileSync(path.join(root,'web/index.html'),'utf8');
assert(html.includes(out.web),'embedded web data matches source');
const a=html.indexOf('// BEGIN MANDEX TRIAL DATA'),b=html.indexOf('// END MANDEX TRIAL DATA',a);
assert(a>=0&&b>a);
const build=new Function('LEVELS','rect','tgt','interiors','pads','tube','V',html.slice(a,b));
for(let i=0;i<levels.length;i++){
 const l=levels[i],grid=new Uint8Array(7200),targets=[],rooms=[],p=[],pipes=[],list=Array(60).fill(null);
 const rect=(x,y,w,h,m)=>{for(let j=y;j<y+h;j++)grid.fill(m,j*120+x,j*120+x+w)};
 build(list,rect,(x,y,kind,range)=>targets.push([x,y,kind,range]),rooms,p,(c,x,y,bx,by,a)=>pipes.push([x/8,y/8,bx/8,by/8]),8);
 const q=list[60+i];assert(q&&q.name===l.name&&q.men===l.men);
 q.build();
 assert.strictEqual(Array.from(grid).join(''),l.rows.join(''),'browser grid '+i);
 assert.strictEqual(targets.length,l.targets.length,'browser targets '+i);
 assert.strictEqual(rooms.length,l.rooms.length,'browser rooms '+i);
 assert.strictEqual(p.length,l.pads.length,'browser pads '+i);
 assert.strictEqual(pipes.length,l.tubes.length,'browser tubes '+i);
}
console.log('TRIAL_PACK_PASS levels='+levels.length+' raw_cells=4320000 commands='+out.commandBytes);


// Traverse real packed geometry using the production browser walker collider
// and tube transfer, not a separate approximation of the character size.
const walkingSource=html.slice(html.indexOf('function wBlocked('),html.indexOf('function enterWalk()'));
let exteriorDoors=0,skybridges=0,lifts=0,walkSamples=0;
for(const l of levels.slice(540)){
  const groups=new Map();
  for(const r of l.rooms){if(!groups.has(r[0]))groups.set(r[0],[]);groups.get(r[0]).push(r)}
  const houses=[...groups.values()];
  const grid=Uint8Array.from(l.rows.join(''),Number);
  const env={WHW:7,WHH:19,V:8,COLS:120,ROWS:60,WW:960,RC:4,chunks:[],tubes:[],time:0,shake:0,
    clamp:(v,lo,hi)=>Math.max(lo,Math.min(hi,v)),ring(){},popText(){},SFX:{whoosh(){}},
    solidAt(x,y){const gx=Math.floor(x/8),gy=Math.floor(y/8);return y>=480||(gx>=0&&gx<120&&gy>=0&&gy<60&&grid[gy*120+gx]!==0)}};
  const {walkClear,walkTubeStep}=new Function('e','with(e){'+walkingSource+'\nreturn {walkClear,walkTubeStep};}')(env);
  for(let h=0;h<houses.length;h++){
    const [x,y,w,height]=houses[h][0],floor=y+height,side=h===0?x-2:x+w;
    for(let j=floor-5;j<floor;j++)assert.strictEqual(l.rows[j].slice(side,side+2),'00','open exterior door '+l.id);
    for(let px=side*8-4;px<=(side+2)*8+4;px+=2){
      assert(walkClear(px,floor*8-19.25),'walker blocked in entrance '+l.id);walkSamples++;
    }
    exteriorDoors++;
  }
  const A=houses[0][0],B=houses[1][0],a=A[0]+A[2],b=B[0]-2;
  const lift=l.tubes[l.tubes.length-1];
  assert(lift&&lift[0]===A[0]-7&&lift[1]===57,'missing ground lift '+l.id);
  const [sx,sy,ex,ey]=lift,deck=ey+3;
  assert(deck<58&&ex>a&&ex<b+2,'lift must reach an elevated inter-building deck '+l.id);
  for(let x=a;x<b+2;x++)assert(grid[deck*120+x]>0,'deck hole '+l.id);
  for(let px=a*8;px<=(b+2)*8;px+=2){
    assert(walkClear(px,deck*8-19.25),'blocked skybridge '+l.id);walkSamples++;
  }
  const m={mode:'walk',x:sx*8,y:sy*8,walkJam:0,tubeCd:0,walkSafe:true,safeX:sx*8,safeY:sy*8};
  assert(walkClear(m.x,m.y)&&walkClear(ex*8,ey*8),'blocked lift mouth '+l.id);
  env.tubes=[{ax:m.x,ay:m.y,bx:ex*8,by:ey*8,r:26,ang:0,sp:480,col:'#fff'}];
  walkTubeStep(m,1/60);
  assert(m.x===ex*8&&m.y===ey*8&&m.mode==='walk'&&m.tubeCd>.8,'real lift transfer '+l.id);
  skybridges++;lifts++;
}
assert.strictEqual(exteriorDoors,135);assert.strictEqual(skybridges,60);assert.strictEqual(lifts,60);
console.log('METROPOLITAN_TRAVERSAL_PASS doors='+exteriorDoors+' skybridges='+skybridges+' lifts='+lifts+' walking_samples='+walkSamples);
