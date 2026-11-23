/* Smoke actual web production TNT functions, not a rewritten copy.
   Tests deterministic safe/armed impacts, accurate blast hitboxes and
   the walking punch's rounded-corner check. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function section(start,end){
  const a=html.indexOf(start),b=html.indexOf(end,a+start.length);
  assert(a>=0&&b>a,'missing production section '+start);return html.slice(a,b);
}
const source=section('const TNT_FUSE_TICKS=24;','function processExpl(){');
const recompute=section('function recompute(ch){','function newChunk(cells){');
const walkHit=section('function walkHit(cx,cy,hw,hh,dmg){','function parryCase(c){');
const harness=String.raw`
const V=8,COLS=120,ROWS=60,RC=4,TNT=5,RUBBER=7,SAND=8;
const MAT=[null,{d:.6},{d:1},{d:1.6},{d:.5},{d:.8},{d:2},{d:.7},{d:1.4}];
let chunks=[],explQ=[],tntLiveFuses=0,time=0,level=0,score=0;
const grid=new Uint8Array(COLS*ROWS),hp=new Float32Array(COLS*ROWS),cases=[],targets=[],events=[];
const idx=(x,y)=>y*COLS+x,inb=(x,y)=>x>=0&&x<COLS&&y>=0&&y<ROWS;
const chip=(...a)=>events.push(['chip',...a]),ring=(...a)=>events.push(['ring',...a]);
const popText=(...a)=>events.push(['text',...a]);
const damageCell=(...a)=>0,wake=(ch)=>{ch.rest=false},parryCase=()=>{};
const killTarget=()=>{};
`;
const tests=String.raw`
function make(x=200,y=200,id=0){
  const a={m:TNT,h:8,dead:false,fuse:0,lx:0,ly:0,wx:x,wy:y};
  const ch={id,x,y,rad:V,tntImpact:0,tntWarned:false,cells:[a],m:1,I:1,rest:false};
  recompute(ch);chunks=[ch];return [ch,a];
}
let tests=0;
function check(cond,msg){assert(cond,msg);tests++}
let [ch,a]=make();
check(ch.hasTnt&&!ch.tntFused,'detached TNT starts unarmed');
for(let k=0;k<90;k++)updateMovingTnt();
check(!a.fuse&&!a.dead&&!explQ.length,'motion alone never explodes the charge');
level=1;ch.tntImpact=29*V;ch.tntHit=a;updateMovingTnt();
check(!a.fuse&&events.some(e=>e[0]==='text'&&e[3]==='TNT SURVIVED!'),
      'medium hit may show an actual safe result');
time=.4;ch.tntImpact=29*V;ch.tntHit=a;updateMovingTnt();
check(a.fuse===23&&tntLiveFuses===1,'medium hit may later ignite a short fuse');
[ch,a]=make(245,248);
ch.tntImpact=56*V;ch.tntHit=a;updateMovingTnt();
check(a.fuse===23&&!explQ.length,'hard hit arms without instant detonation');
for(let k=0;k<23;k++)updateMovingTnt();
check(a.dead&&explQ.length===1&&explQ[0][0]===30&&explQ[0][1]===31,
      'fuse explodes at moved voxel position after 24 ticks');
movedTntBoom(a);
check(explQ.length===1,'spent TNT cannot trigger twice');
[ch,a]=make(600,240);ch.rad=400;a.wx=316;a.wy=240;
blastMovingTnt(316,240);
check(a.fuse===8,'blast detects TNT at far end of a long moving body');
[ch,a]=make(316,240);ch.rad=500;a.wx=752;a.wy=240;
blastMovingTnt(316,240);
check(!a.fuse,'blast ignores a distant TNT voxel whose chunk centre is near');
[ch,a]=make();armMovingTnt(ch,a,8);
check(events.some(e=>e[0]==='text'&&e[3]==='TNT FUSE!'),'fuse has clear in-game feedback');

[ch,a]=make(200,200);const other={...a,wx:340,wy:200,fuse:0};
ch.cells.push(other);recompute(ch);
ch.tntHit=other;ch.tntImpact=56*V;updateMovingTnt();
check(!a.fuse&&other.fuse===23,'in one chunk only the actual struck TNT voxel ignites');
[ch,a]=make(200,200);
const later={...a,wx:208,wy:200,fuse:0};
ch.cells.push(later);recompute(ch);
armMovingTnt(ch,a,16);
ch.tntHit=later;ch.tntImpact=56*V;updateMovingTnt();
check(a.fuse===15&&later.fuse===23&&tntLiveFuses===2,
      'a second TNT voxel ignites on impact even if the first fuse already burns');
[ch,a]=make(200,200);
armMovingTnt(ch,a,16);
ch.tntHit=a;ch.tntImpact=29*V;
const safeBefore=events.filter(e=>e[0]==='text'&&e[3]==='TNT SURVIVED!').length;
updateMovingTnt();
check(a.fuse===15&&events.filter(e=>e[0]==='text'&&e[3]==='TNT SURVIVED!').length===safeBefore,
      'an already armed voxel never displays a misleading safe landing');
[ch,a]=make(200,200);
ch.tntImpact=56*V;ch.tntHit=null;updateMovingTnt();
check(!a.fuse,'distant steel hit cannot ignite an uncontacted TNT voxel');
[ch,a]=make(200,200);const next={...a,wx:208,wy:200,fuse:0};
ch.cells.push(next);recompute(ch);
armMovingTnt(ch,a,8);armMovingTnt(ch,next,8);updateMovingTnt();
check(tntLiveFuses===2,'HUD counts actual burning TNT charges, not only chunks');
const wood={m:1,h:40,dead:false,wx:113,wy:113};
const piece={x:100,y:100,rad:80,cells:[wood],rest:false};
chunks=[piece];const outside=walkHit(100,100,10,10,16);
check(outside===0&&wood.h===40,'punch rectangle rejects diagonal corner near misses');
wood.wx=112;wood.wy=112;const inside=walkHit(100,100,10,10,16);
check(inside===16&&wood.h===24,'same moving voxel is hittable inside rounded corner');
return {tests,explosions:explQ.length};
`;
const run=new Function('assert',harness+recompute+source+walkHit+tests);
const result=run(assert);
console.log('BROWSER_MOBILE_TNT_PASS tests='+result.tests+' precise_hitboxes=1');
