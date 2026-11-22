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
    WW:960,G:900,RUBBER:7,KEYS:{},chunks:[],targets:[],MAT:[],
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
  const walkStep=new Function('e','with(e){'+source+'\nreturn walkStep;}')(e);
  return {e,tick(keys={},dt=DT){
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
m=stomp.tick({KeyS:true});
check(m.stomp&&m.vy>=720&&m.jumpHold===0,'stomp overrides jump cutting');

const pad=make({mat:7});
pad.e.man.gr=false;pad.e.man.vy=64;
m=pad.tick();
check(m.vy===-470&&m.jumpHold===0&&m.ledgeGrace===0,'ordinary pad launch unchanged');
m=pad.tick();
check(m.vy< -180,'released jump key must not weaken a rubber pad launch');

check(html.includes('m.ledgeGrace=0;m.jumpHold=0;m.R=14'),
      'enterWalk resets timers for every player life');
const sa=html.indexOf('<script>'),sb=html.indexOf('</script>',sa);
new Function(html.slice(sa+8,sb));
console.log('BROWSER_JUMP_FORGIVENESS_PASS cases='+checks+
  ' short_hop=1 hold_jump=1 ledge=1 no_double=1 stomp=1 pad=1 syntax=1');
