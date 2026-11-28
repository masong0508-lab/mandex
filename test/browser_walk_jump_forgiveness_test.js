/* Browser production walking: variable-height jumps, ledge forgiveness
 * and pad/stomp nonregressions. Uses the actual walkStep and wBlocked source. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const a=html.indexOf('function wBlocked('),b=html.indexOf('function walkStep(');
const end=html.indexOf('/* Match the console sprite turnaround:',b);
assert(a>=0&&b>a&&end>b,'production walking functions missing');
const source=html.slice(a,end);
const DT=1/60;
function make({ground=true,mat=6}={}){
  const e={WHW:7,WHH:19,WALK_T:45,V:8,RC:4,COLS:120,ROWS:60,
    WW:960,G:900,RUBBER:7,KEYS:{},chunks:[],targets:[],tubes:[],MAT:[],
    time:0,shake:0,grid:new Uint8Array(7200),hp:new Float32Array(7200),
    parts:[],events:[],man:{
      mode:'walk',walked:true,face:1,x:160,y:320-19-.24,vx:0,vy:0,
      gr:ground,wt:45,pT:0,pCd:0,padCd:0,stomp:false,
      ledgeGrace:0,jumpHold:0,turn:0,R:14
    }};
  e.clamp=(v,lo,hi)=>Math.max(lo,Math.min(hi,v));
  e.idx=(x,y)=>y*120+x;
  e.inb=(x,y)=>x>=0&&y>=0&&x<120&&y<60;
  e.solidAt=(x,y)=>{
    const gx=Math.floor(x/8),gy=Math.floor(y/8);
    return y>=480||(e.inb(gx,gy)&&e.grid[e.idx(gx,gy)]!==0);
  };
  const record=(name)=>(...p)=>e.events.push([name,...p]);
  for(const n of ['goo','ring','dust','popText','chip','spark','wobKick','wake','killTarget'])
    e[n]=record(n);
  e.SFX={};
  for(const n of ['splat','pop','crunch','whoosh','boom','thud','boing'])e.SFX[n]=record(n);
  e.damageCell=()=>0;e.rnd=()=>.5;
  if(ground)for(let x=12;x<=35;x++){
    e.grid[e.idx(x,40)]=mat;e.hp[e.idx(x,40)]=150;
  }
  const {walkStep,wBlocked,walkTubeStep}=new Function('e','with(e){'+source+'\nreturn {walkStep,wBlocked,walkTubeStep};}')(e);
  return {e,blocked:wBlocked,tube:walkTubeStep,tick(keys={},dt=DT){
    for(const k of Object.keys(e.KEYS))delete e.KEYS[k];
    Object.assign(e.KEYS,keys);
    e.time+=dt;
    walkStep(dt);
    return {...e.man};
  }};
}
let checks=0;
function check(cond,message){assert(cond,message);checks++;}
const tap=make(),tapStart=tap.e.man.y;
let m=tap.tick({Space:true});
check(m.vy<0&&!m.gr&&m.jumpHold>0&&m.ledgeGrace===0,'fresh jump must launch');
m=tap.tick();
check(m.vy>=-180&&m.vy<0&&m.jumpHold===0,'release must shorten ascent once');
check(m.y<tapStart,'short hop must leave the floor');

function apex(held){
  const s=make();let min=s.e.man.y;
  for(let i=0;i<50;i++){
    const m=s.tick(i<held?{Space:true}:{});
    min=Math.min(min,m.y);
  }
  return min;
}
check(apex(20)<apex(1)-24,'held jump must visibly clear higher platforms');
const held=make();
let prior=held.tick({Space:true}).vy;
for(let i=0;i<5;i++){
  const current=held.tick({Space:true}).vy;
  check(current>prior&&current<0,'held jump must follow the original gravity');
  prior=current;
}
check(held.e.man.ledgeGrace===0,'held jump never gains a second grace jump');
const grace=make({ground:false});
grace.e.man.ledgeGrace=.06;grace.e.man.vy=64;
m=grace.tick({Space:true});
check(m.vy<0&&m.ledgeGrace===0&&m.jumpHold>0,'coyote input works in midair');
const first=m.vy;
check(grace.tick({Space:true}).vy>first,'coyote input cannot restart midair');

const expired=make({ground:false});
expired.e.man.ledgeGrace=.02;expired.e.man.vy=32;
expired.tick();expired.tick();m=expired.tick({Space:true});
check(m.ledgeGrace===0&&m.vy>0,'expired coyote timer cannot give a double jump');

const edge=make();
for(let x=20;x<=35;x++)edge.e.grid[edge.e.idx(x,40)]=0;
edge.e.man.x=18.5*8;
let fell=false;
for(let i=0;i<30;i++){
  const s=edge.tick({KeyD:true});
  if(!s.gr){fell=true;break}
}
check(fell&&edge.e.man.ledgeGrace>0,'walking off actual floor activates grace');
check(edge.tick({Space:true}).vy<0,'jump after real ledge edge works');

const stomp=make();
stomp.tick({Space:true});
stomp.e.man.y=160; // Let the downward stomp begin before floor contact.
m=stomp.tick({KeyS:true});
check(m.stomp&&m.vy>=720&&m.jumpHold===0,'stomp overrides jump cutting');

const pad=make({mat:7});
pad.e.man.gr=false;pad.e.man.vy=64;
m=pad.tick();
check(m.vy===-470&&m.jumpHold===0&&m.ledgeGrace===0,'ordinary pad launch unchanged');
m=pad.tick();
check(m.vy< -180,'released jump key must not weaken a rubber pad launch');

check(html.includes('m.ledgeGrace=0;m.jumpHold=0;m.jumpBuffer=0'),
      'enterWalk resets timers for every player life');
const buffered=make();buffered.e.man.gr=false;buffered.e.man.y-=1.6;buffered.e.man.vy=96;
m=buffered.tick({Space:true});check(m.jumpBuffer>0,'pre-landing tap buffered');
m=buffered.tick();check(m.vy<0&&m.jumpBuffer===0,'buffered release launches once after floor contact');
const noRepeat=make();for(let i=0;i<120;i++)m=noRepeat.tick({Space:true});
check(m.gr&&m.vy===0,'held jump does not auto-repeat');
const jam=make();jam.e.man.walkSafe=true;jam.e.man.safeX=120;jam.e.man.safeY=320-19-.24;jam.e.man.x=640;jam.e.man.y=240;
for(let y=15;y<45;y++)for(let x=65;x<96;x++)jam.e.grid[jam.e.idx(x,y)]=6;
for(let i=0;i<20;i++)m=jam.tick();check(m.x<240,'bounded jam rescue finds last safe platform');
const voxel=make({ground:false});
const c={x:320,y:240,rad:160,cells:[{wx:320,wy:240,dead:false}]};
voxel.e.chunks=[c];
const cell=c.cells[0],xs=[313,320,327],ys=[221,230.5,240,249.5,258.99];
for(let dx=-16;dx<=16;dx+=.5)for(let dy=-28;dy<=28;dy+=.5){
  cell.wx=c.x+dx;cell.wy=c.y+dy;
  const expected=xs.some(x=>ys.some(y=>Math.abs(cell.wx-x)<4&&Math.abs(cell.wy-y)<4));
  check(voxel.blocked(c.x,c.y)===expected,'exact live rubble sample parity');
}
cell.wx=c.x;cell.wy=c.y;cell.dead=true;
check(!voxel.blocked(c.x,c.y),'dead debris no longer blocks walking');
const ceiling=make({ground:false});
ceiling.e.man.x=16;ceiling.e.man.y=19;
ceiling.e.man.walkSafe=true;ceiling.e.man.safeX=200;ceiling.e.man.safeY=160;
for(let y=0;y<10;y++)for(let x=0;x<15;x++)ceiling.e.grid[ceiling.e.idx(x,y)]=6;
ceiling.e.tubes=[{ax:16,ay:19,bx:560,by:160,r:26,ang:0,sp:480}];
m=ceiling.tick();check(m.y===19&&m.x===16&&m.walkJam>0,'ceiling jam cannot escape above map');
ceiling.tube(ceiling.e.man,DT);check(ceiling.e.man.x===16,'jammed walker cannot activate tube');
for(let i=0;i<20;i++)m=ceiling.tick();
check(m.x===200&&m.y>=19&&!ceiling.blocked(m.x,m.y),'ceiling jam restores a valid safe point');
// A blocked landing must not hide a second reachable lift mouth.
const route=make({ground:false}),rm=route.e.man;
const originX=rm.x,originY=rm.y;
rm.walkSafe=true;rm.safeX=120;rm.safeY=240;
for(let y=8;y<=32;y++)for(let x=49;x<=57;x++)route.e.grid[route.e.idx(x,y)]=6;
const lift=(ax,bx)=>({ax,ay:originY,bx,by:160,r:26,ang:0,sp:480,col:'#fff'});
const blockedLift=lift(originX,424),openLift=lift(originX+8,720);
route.e.tubes=[blockedLift,openLift];
const timeBefore=rm.wt;
route.tube(rm,DT);
check(rm.x===720&&rm.y===160&&rm.mode==='walk'&&rm.tubeCd===.85,'blocked first exit must not hide a second clear lift');
check(rm.wt===timeBefore&&rm.safeX===120&&rm.safeY===240,'transfer preserves time and last grounded recovery point');
check(blockedLift.pulseAt===undefined&&openLift.pulseAt===route.e.time,'only the successful lift pulses');
const allBlocked=make({ground:false}),am=allBlocked.e.man;
for(let y=8;y<=32;y++)for(let x=49;x<=57;x++)allBlocked.e.grid[allBlocked.e.idx(x,y)]=6;
allBlocked.e.tubes=[lift(originX,424),lift(originX+8,432)];
allBlocked.tube(am,DT);
check(am.x===originX&&am.y===originY&&am.tubeCd===.20,'all blocked exits preserve position and use short retry cooldown');
check(allBlocked.e.events.length===0,'failed transfers do not emit effects');
const priority=make({ground:false}),pm=priority.e.man;
priority.e.tubes=[lift(originX,600),lift(originX+8,720)];
priority.tube(pm,DT);
check(pm.x===600&&pm.tubeCd===.85,'first clear authored lift retains priority');
priority.tube(pm,DT);
check(pm.x===600&&pm.tubeCd<.85,'cooldown prevents immediate second transfer');

const sa=html.indexOf('<script>'),sb=html.indexOf('</script>',sa);
new Function(html.slice(sa+8,sb));
console.log('BROWSER_JUMP_FORGIVENESS_PASS cases='+checks+
  ' short_hop=1 hold_jump=1 ledge=1 no_double=1 stomp=1 pad=1 syntax=1');
