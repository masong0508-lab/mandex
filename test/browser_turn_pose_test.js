/* Browser and handheld share a 12-tick back-view reversal, no new pose
 * bitmap. Verify profile continuity and original standalone script syntax. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const begin=html.indexOf('function walkTurnAngle(');
const end=html.indexOf('function drawWalkMan(',begin);
assert(begin>=0&&end>begin,'live browser turnaround helpers exist');
const helper=new Function('const V=8;'+html.slice(begin,end)+'\nreturn {angle:walkTurnAngle,face:walkTurnFace,lunge:walkPunchLunge};')();
const eps=1e-9;
for(const [remaining,expected] of [[.2,0],[.15,1.2],[.1,2.4],[.05,1.2],[0,0],[-1,0],[1,0]]){
  assert(Math.abs(helper.lunge(remaining,0)-expected)<eps,
    'browser lunge follows console anticipation/recovery duration');
}
assert(helper.lunge(.1,6)===0,'turning cannot slide the grounded punch pose');
assert(html.includes('visualFace=walkPose(m)')&&html.includes('drawSpin(q.th,q)'),
  'real walking sprite uses the articulated pose');
assert(Math.abs(helper.angle(12)-Math.PI/2)<eps,'first frame is the original side profile');
assert(Math.abs(helper.angle(6)-Math.PI)<eps,'turning reaches exact rear view at midpoint');
assert(Math.abs(helper.angle(0)-Math.PI/2)<eps,'turn returns to the opposite side view');
for(const initial of[-1,1]){
  const facing=-initial;
  let previous=Math.PI/2;
  for(let t=12;t>=1;t--){
    const angle=helper.angle(t);
    assert(angle>=Math.PI/2-eps&&angle<=Math.PI+eps,'turn never flips through the front view');
    assert(Math.abs(angle-previous)<.45,'turn rotates smoothly between neighboring 15-degree poses');
    assert.strictEqual(helper.face(facing,t),t>6?initial:facing,'mirror switches only behind the torso');
    previous=angle;
  }
  assert.strictEqual(helper.face(facing,0),facing,'final facing follows responsive controls');
}
// Execute the real movement function, including its input decoding, turn
// update and immediate punch direction. A simple empty room/floor keeps the
// test about countersteering without duplicating the turn implementation.
const walkStart=html.indexOf('function walkStep(');
const walkEnd=html.indexOf('/* Match the console sprite turnaround:',walkStart);
assert(walkStart>=0&&walkEnd>walkStart,'real walking simulation exists');
const walk=new Function(`
  const V=8,G=800,WHH=16,WHW=8,WW=960,RUBBER=8;
  let man,grounded,time=0,punchFace=0;const KEYS={},targets=[];
  const clamp=(v,a,b)=>Math.max(a,Math.min(b,v)),rnd=()=>1;
  const wBlocked=(x,y)=>grounded&&y>200,inb=()=>false;
  const SFX={pop(){},whoosh(){},boing(){}};
  function dust(){} function popText(){} function ring(){}
  function walkQuit(){throw Error('unexpected walking timeout')}
  function walkPunch(){punchFace=man.face;man.pCd=.3;man.pT=.2}
  function walkSlam(){throw Error('unexpected slam')}
  function killTarget(){throw Error('unexpected target')}
  ${html.slice(html.indexOf('function walkAnimTick('),html.indexOf('function walkFootSample('))}
  ${html.slice(walkStart,walkEnd)}
  return {
    reset(face,onFloor){
      grounded=onFloor;punchFace=0;
      man={x:480,y:200,vx:0,vy:0,wt:1000,face,turn:0,
           gr:onFloor,stomp:false,pCd:0,pT:0,padCd:0};
      return man;
    },
    tick(ax,punch=false){
      KEYS.ArrowLeft=ax<0;KEYS.ArrowRight=ax>0;KEYS.KeyX=punch;
      walkStep(1/60);return {man,punchFace};
    }
  };
`)();
function yaw(m){
  const a=helper.angle(m.turn),f=helper.face(m.face,m.turn);
  return f<0?2*Math.PI-a:a;
}
let samples=0;
function tickContinuous(m,ax,punch=false){
  const before=yaw(m),result=walk.tick(ax,punch),after=yaw(m);
  const delta=Math.abs(Math.atan2(Math.sin(after-before),Math.cos(after-before)));
  assert(delta<.407,'countersteering advances at most one normal turn step, never snaps sides');
  assert(m.turn>=0&&m.turn<12,'turn counter remains bounded');
  if(ax)assert.strictEqual(m.face,ax,'gameplay facing responds immediately');
  if(punch)assert.strictEqual(result.punchFace,ax,'punch uses new gameplay facing on the reversal tick');
  samples++;
}
for(const initial of [-1,1])for(const grounded of [false,true]){
  for(let elapsed=1;elapsed<=12;elapsed++){
    const m=walk.reset(initial,grounded);
    for(let i=0;i<elapsed;i++)tickContinuous(m,-initial);
    tickContinuous(m,initial,true);
    for(let i=0;i<12;i++)tickContinuous(m,0);
    assert.strictEqual(m.turn,0,'released controls complete the reversed turn');
    assert.strictEqual(helper.face(m.face,m.turn),initial,'turn settles on requested side');
  }
  for(let dwell=1;dwell<=7;dwell++){
    const m=walk.reset(initial,grounded);
    for(let i=0;i<84;i++)tickContinuous(m,Math.floor(i/dwell)%2?initial:-initial);
    for(let i=0;i<12;i++)tickContinuous(m,0);
    assert.strictEqual(m.turn,0,'repeated direction changes still settle');
  }
  const m=walk.reset(initial,grounded);
  for(let i=0;i<12;i++)tickContinuous(m,initial);
  assert.strictEqual(m.turn,0,'holding current facing does not start a turn');
}
assert(html.includes('m.turn=0;m.gait=0;m.vx=m.vy=0'),'new walk clears its old turn and stride phase');
const start=html.indexOf('<script>'),stop=html.indexOf('</script>',start);
assert(start>=0&&stop>start,'single-file browser game script exists');
new Function(html.slice(start+8,stop));
console.log('WEB_TURN_PASS side_back_side=1 directions=2 steps=12 interrupted_samples='+samples+' no_new_buffers=1');
