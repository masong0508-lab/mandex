/* Real pointer event-handler integration regression for the Mandex browser
 * creator. Dispatches touch events through the shipped event registrations. */
'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const cut=(a,b,from=0)=>{
  const i=html.indexOf(a,from),j=html.indexOf(b,i+a.length);
  assert(i>=0&&j>i,'production event fragment '+a);
  return html.slice(i,j);
};
const strokes=cut('function eUp(){','function eBuild(){');
const handlers=cut("cv.addEventListener('pointerdown',e=>{","addEventListener('keyup',e=>{");
const after=cut("for(const ev of['pointerup','pointercancel'])","addEventListener('keydown',e=>{");
const editor=new Function(`
 const events={},captures=[],rects=[],moves=[],starts=[],snapshots=[];
 const cv={addEventListener(n,fn){(events[n]||=[]).push(fn)},
           setPointerCapture(id){captures.push(id)}};
 let mouseX=0,mouseY=0,dragging=false,sliderDrag=false,sodaHold=false,state='edit',
     portrait=false;
 const WEB_UI={gesture:null,editorHeight:480},TID={},man=null;
 const E={drag:null,pointer:null,starters:false};
 const audioInit=()=>{},gagReset=()=>{},pt=e=>[e.x,e.y],
       isPortrait=()=>portrait,hitTest=()=>false;
 const mobileEditorToWorld=(x,y)=>[x+5,y+7];
 const eDown=(x,y)=>{starts.push([x,y]);E.drag={a:[x,y],b:[x,y],m:2}};
 const eMove=(x,y)=>{moves.push([x,y]);if(E.drag)E.drag.b=[x,y]};
 const eRect=(a,b,m)=>rects.push({a,b,m});
 const eSnap=()=>snapshots.push(1);
 const releaseWalkTouch=()=>{},addEventListener=()=>{};
 ${strokes}
 ${handlers}
 ${after}
 return {E,events,captures,rects,moves,starts,snapshots,
   portrait(value){portrait=!!value},setStarters(value){E.starters=!!value}};
`)();
const send=(name,id,x,y)=>{
  const listeners=editor.events[name]||[];
  assert(listeners.length,'missing production '+name+' listener');
  for(const callback of listeners)callback({pointerId:id,x,y});
};
send('pointerdown',1,340,180);
assert.strictEqual(editor.E.pointer,1,'first painter owns the stroke');
assert.deepStrictEqual(editor.captures,[1],'only painter captures input');
send('pointerdown',2,350,190);
send('pointermove',2,410,250);
send('pointerup',2,410,250);
assert.strictEqual(editor.starts.length,1,'foreign down ignored');
assert.strictEqual(editor.moves.length,0,'foreign move ignored');
assert.strictEqual(editor.rects.length,0,'foreign up cannot commit');
assert.strictEqual(editor.E.pointer,1,'foreign up cannot steal owner');
send('pointermove',1,375,215);
send('pointerup',1,375,215);
assert.strictEqual(editor.rects.length,1,'owner completes exactly once');
assert.strictEqual(editor.snapshots.length,1,'owner commit takes one history snapshot');
assert.deepStrictEqual(editor.rects[0].b,[375,215],'owner controls rectangle');
assert.strictEqual(editor.E.pointer,null,'owner cleared after finishing');
send('lostpointercapture',1,375,215);
assert.strictEqual(editor.rects.length,1,'late capture loss cannot double commit');
send('pointerdown',3,360,190);
send('pointercancel',4,361,191);
assert.strictEqual(editor.E.pointer,3,'foreign cancel cannot discard live stroke');
send('pointercancel',3,361,191);
assert.strictEqual(editor.rects.length,1,'owner cancellation discards rectangle');
assert.strictEqual(editor.snapshots.length,1,'cancel cannot create an undo state');
assert.strictEqual(editor.E.drag,null);
assert.strictEqual(editor.E.pointer,null);
send('pointerdown',5,365,220);
send('lostpointercapture',5,380,239);
assert.strictEqual(editor.rects.length,1,'capture loss cannot commit rectangle');
assert.strictEqual(editor.E.pointer,null);
editor.portrait(true);
send('pointerdown',6,190,180);
assert.deepStrictEqual(editor.starts.at(-1),[195,187],'portrait coordinates map to world');
send('pointermove',6,210,500);
assert.deepStrictEqual(editor.moves.at(-1),[375,215],
 'portrait toolbar must not continue drawing');
send('pointermove',6,220,190);
assert.deepStrictEqual(editor.moves.at(-1),[225,197],'portrait draws in world coordinates');
send('pointerup',6,220,190);
assert.strictEqual(editor.rects.length,2);
assert.strictEqual(editor.snapshots.length,2,'portrait commit takes one history snapshot');
const before=editor.starts.length,captureCount=editor.captures.length;
send('pointerdown',7,190,530);
assert.strictEqual(editor.starts.length,before,'portrait toolbar never starts painting');
assert.strictEqual(editor.captures.length,captureCount,'toolbar never steals capture');
editor.setStarters(true);
send('pointerdown',8,190,150);
assert.strictEqual(editor.starts.length,before,'starter picker blocks paint taps');
const start=html.indexOf('<script>'),end=html.indexOf('</script>',start);
new Function(html.slice(start+8,end));
console.log('EDITOR_POINTER_EVENTS_PASS dual_touch=1 ownership=1 commit_once=1 cancel=1 lost_capture=1 portrait=1 toolbar=1 picker=1 syntax=1');
