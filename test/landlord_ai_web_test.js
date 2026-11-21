/* Exercise production browser landlord AI, especially bat reaction distance.
 * A previous squared-distance optimization left a stale 'tm' reference
 * that JS syntax checks could not catch. */
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync(require('path').join(__dirname,'../web/index.html'),'utf8');
const a=source.indexOf('const WALKS=['),b=source.indexOf('function batBlock(t,m)',a);
assert(a>=0&&b>a,'landlord AI source range unavailable');
const scene=`
let man={mode:'fly',x:0,y:0},targets=[],chunks=[],V=8,G=900;
const SCARES=['EEK!'];let floorY=Infinity;
const solidAt=(x,y)=>y>=floorY;
const lordSay=(t,s)=>{t.lastSay=s};
const fightOn=()=>true;
`+source.slice(a,b)+`
const makeT=()=>({
  x:200,y:160,kind:'bat',alive:true,face:1,stun:0,wind:0,
  swing:0,pan:0,cd:4,blink:1,pan:0,say:0,dodge:0,
  range:40,cover:null,mv:0,t:0,heal:3
});
let t=makeT();man.x=t.x+149;man.y=t.y;
lordAI(t,1/60);
if(!(t.swing>0))throw Error('bat did not react to a flying player at 149px');
t=makeT();man.x=t.x+150;man.y=t.y;
lordAI(t,1/60);
if(t.swing>0)throw Error('bat reacted outside strict 150px range');
t=makeT();man.x=t.x+70;man.y=t.y+130;
lordAI(t,1/60);
if(!(t.swing>0))throw Error('bat range missed diagonal distance');
t=makeT();t.r=7;t.collapseRolled=2;t.dodge=1.5;t.dd=1;
man.mode='none';floorY=t.y+t.r+1;
const old=t.x;lordAI(t,1/60);
if(!(t.x>old)||t.mv!==2)throw Error('wounded collapse survivor did not flee');
t=makeT();t.collapseRolled=2;t.stun=1;t.dodge=1.5;
const frozen=t.x;lordAI(t,1/60);
if(t.x!==frozen||t.mv)throw Error('stunned survivor moved before recovering');
/* Reference is the older browser danger expression, with speed first.
   Spatial-first must return the EXACT same dodge direction. */
function lordDangerReference(t){
  for(const ch of chunks){
    const dx=t.x-ch.x,dy=t.y-ch.y;
    if(ch.vx*ch.vx+ch.vy*ch.vy<45*45||Math.abs(dx)>ch.rad+46||
       dy<-ch.rad-18||(dy>ch.rad+120&&ch.vy<=0))continue;
    if(ch.vy<-20&&dy>0)continue;
    const near=ch.rad+70;
    if(dx*dx+dy*dy<near*near)return Math.sign(dx)||(t.face||1);
  }
  return 0;
}
let seed=0x1234abcd;
const rand=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296};
for(let i=0;i<24000;i++){
  const q={x:(rand()-.5)*600,y:(rand()-.5)*350,face:i&1?1:-1};
  chunks=[];
  for(let k=0;k<4;k++)chunks.push({
    x:q.x+(rand()-.5)*420,y:q.y+(rand()-.5)*300,
    vx:(rand()-.5)*240,vy:(rand()-.5)*220,rad:rand()*80
  });
  const fast=lordDanger(q),ref=lordDangerReference(q);
  if(fast!==ref)throw Error('browser distant rubble changed dodge direction at test '+i);
}
'web landlord AI: bat proximity, wounded retreat, 24k hazard reaction parity PASSED';
`;
const result=vm.runInNewContext(scene,{});
assert(result.includes('PASSED'));
console.log(result);
