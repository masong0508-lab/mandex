'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function between(a,b){const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,'missing '+a);return html.slice(i,j)}
const factory=new Function('portrait','height','remaining',`
  let W=portrait?540:960,H=height,men=remaining,state='play',man={mode:'walk',wt:48.2,x:100,y:200};
  const KEYS={},TID={},WALK_T=45,clamp=(v,a,b)=>Math.max(a,Math.min(b,v)),isPortrait=()=>portrait;
  const drawn=[],actions=[],labels=[];
  let pops=0;const SFX={pop(){pops++}},goo=()=>{};
  const ctx={fillRect(){},fill(){},stroke(){}};
  const rrect=(x,y,w,h)=>drawn.push({x,y,w,h});
  const txt=(s,x,y)=>labels.push({s,x,y});
  const btn=(x,y,w,h,s,fn)=>actions.push({x,y,w,h,s,fn});
  ${between('function walkQuit(){','// hit everything in a box')}
  ${between('function releaseWalkTouch(','function manStep(')}
  return {paint:drawWalkHUD,down:walkTouch,up:releaseWalkTouch,buttons:walkBtns,drawn,actions,labels,keys:KEYS,ids:TID,man,pops:()=>pops};
`);
for(const [p,h] of [[true,720],[true,960],[true,1240],[false,540]])for(const men of [0,3]){
 const t=factory(p,h,men);t.paint();
 const q=t.actions.find(b=>b.s===(men?'NEXT MAN':'END RUN'));
 assert(q,'touch-only player needs a safe end-of-run action');
 assert(q.x>=0&&q.y>=0&&q.x+q.w<=(p?540:960)&&q.y+q.h<=h);
 for(const b of t.buttons()){
  assert(q.x+q.w<=b.x||b.x+b.w<=q.x||q.y+q.h<=b.y||b.y+b.h<=q.y,'quit hitbox cannot overlap held inputs');
 }
 assert(t.labels.some(b=>b.s==='49s'),'actual seconds including earned bonus shown');
 const left=t.buttons()[0];t.down(left.x+5,left.y+5,1);assert(t.keys.TL);
 t.down(q.x+4,q.y+4,1);assert.strictEqual(t.man.mode,'walk');
 q.fn();assert.strictEqual(t.man.mode,'done');assert.strictEqual(t.pops(),1);
 assert(!t.keys.TL&&Object.keys(t.ids).length===0,'finish clears held virtual inputs');
 q.fn();assert.strictEqual(t.pops(),1,'stale/repeated quit action harmless');
}
console.log('WEB_WALK_FINISH_PASS layouts=4 final_life=1 seconds=1 touch_isolation=1');
