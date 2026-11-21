/* Regression: captured mobile thumbs, shared keys and slide between
 * movement directions must survive orientation swaps and browser blur. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const a=html.indexOf('function releaseWalkTouch(');
const b=html.indexOf('function drawWalkHUD(',a);
assert(a>=0&&b>a,'real gameplay walking touch helpers are present');
const factory=new Function('portrait',
  'let W=portrait?540:960,H=portrait?960:540;const KEYS={},TID={};'+
  'const isPortrait=()=>portrait;'+html.slice(a,b)+
  '\nreturn {keys:KEYS,ids:TID,buttons:walkBtns,down:walkTouch,up:releaseWalkTouch};');
for(const portrait of [false,true]){
  const t=factory(portrait),boxes=t.buttons();
  for(const [i,x]of boxes.entries()){
    assert(x.x>=0&&x.y>=0&&x.x+x.w<=(portrait?540:960)&&x.y+x.h<=(portrait?960:540),
      'all touch controls fit viewport '+portrait+' '+i);
  }
  function down(i,id){const b=boxes[i];return t.down(b.x+b.w/2,b.y+b.h/2,id)}
  assert(down(0,101)&&down(0,102),'two fingers both hold LEFT');
  t.up(101);assert(t.keys.TL,'first finger does not release second finger');
  assert(down(1,102),'sliding thumb from left to right is recognized');
  assert(!t.keys.TL&&t.keys.TR,'previous virtual direction released on slide');
  assert(down(3,103)&&down(4,104),'PUNCH and JUMP can be held simultaneously');
  t.up(102);assert(!t.keys.TR&&t.keys.TP&&t.keys.TJ,'right release preserves other buttons');
  t.up(103);assert(!t.keys.TP&&t.keys.TJ,'punch release does not cancel jump');
  t.up(104);assert(!t.keys.TJ&&Object.keys(t.ids).length===0,'all controls release cleanly');
}
assert(html.includes('cv.setPointerCapture(e.pointerId)'),'held walking controls capture the pointer');
assert(html.includes("'lostpointercapture'"),'release outside canvas is handled');
assert(html.includes("'orientationchange'"),'rotating the screen releases stale held buttons');
assert(html.includes('document.hidden'),'backgrounding the tab resets held controls');
assert(html.includes('cameraMaxX=Math.max(0,WW-vw)'),'portrait camera never has a negative pan bound');
console.log('WEB_MULTITOUCH_PASS layouts=2 held_duplicate=1 slide=1 jump_punch=1 capture=1');
