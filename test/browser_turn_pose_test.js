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
assert(html.includes('ctx.translate(visualFace<0?-punchLunge:punchLunge,0)'),
  'the real browser character paint, not the physics body, receives the lunge');
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
assert(html.includes('if(ax&&ax!==m.face){m.face=ax;m.turn=12}'),'visual rotation triggered by actual direction flip');
assert(html.includes('m.turn=0;m.vx=m.vy=0'),'new walk does not inherit an old turn');
const start=html.indexOf('<script>'),stop=html.indexOf('</script>',start);
assert(start>=0&&stop>start,'single-file browser game script exists');
new Function(html.slice(start+8,stop));
console.log('WEB_TURN_PASS side_back_side=1 directions=2 steps=12 no_new_buffers=1');
