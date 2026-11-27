/* Actual browser tool chooser, stock, modal pointer ownership and layouts. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const cut=(a,b)=>{const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,a);return html.slice(i,j)};
const create=new Function(`
  let W=540,H=720,state='play',dragging=false,SX=128,SY=362,hits=[];
  const man={mode:'aim',x:SX,y:SY},pw={sel:null,left:{beans:1,towel:1,splat:1,turbo:1,quake:1,burp:1}};
  const PD={unlockAll:false,opt:{tool:'none'}},OPT=()=>PD.opt;
  let cleared=0,saves=0,pops=0,resets=0;
  const storyDone=()=>cleared,pdTouch=()=>{},pdSave=()=>{saves++},SFX={pop(){pops++}};
  const WEB_UI={toolPicker:false,toolPointer:null};
  const PLAY_INPUT={aim:null,soda:null,pan:new Map()};
  const cv={setPointerCapture(){}};
  const resetPlayInput=()=>{resets++;WEB_UI.toolPicker=false;WEB_UI.toolPointer=null;};
  const ev=id=>({pointerId:id,button:0,preventDefault(){}});
  ${cut('const TOOLS=','function drawValve(')}
  ${cut('const TOOL_PICKER_DETAILS=','function star(')}
  return {PD,man,pw,WEB_UI,PLAY_INPUT,ev,openToolPicker,closeToolPicker,toolPickerSelect,toolPickerStatus,
    toolPickerLayout,toolPickerHit,toolPickerDown,toolPickerUp,toolPickerCancel,toolPickerKey,toolPickerAllowed,
    size:(w,h)=>{W=w;H=h},clears:v=>cleared=v,drag:v=>dragging=v,
    stats:()=>({saves,pops,resets}),ids:()=>['none'].concat(toolList().map(t=>t.id))};
`);
const a=create();let count=0;
for(const [w,h] of [[540,720],[540,960],[540,1240],[960,540]]){
  a.size(w,h);const p=a.toolPickerLayout(w,h);
  assert(p.x>=0&&p.y>=0&&p.x+p.w<=w&&p.y+p.h<=h,'panel fits');
  assert.deepStrictEqual(p.cards.map(c=>c.id),a.ids(),'all tools');
  const rects=[...p.cards,p.close];
  for(let i=0;i<rects.length;i++){
    const c=rects[i];
    assert(c.x>=p.x&&c.y>=p.y&&c.x+c.w<=p.x+p.w&&c.y+c.h<=p.y+p.h,'card fits');
    for(let j=i+1;j<rects.length;j++){
      const d=rects[j];
      assert(c.x+c.w<=d.x||d.x+d.w<=c.x||c.y+c.h<=d.y||d.y+d.h<=c.y,'no overlapping hit boxes');
    }
    count++;
  }
}
a.size(540,720);assert(a.openToolPicker());
const p=a.toolPickerLayout(540,720),q=p.cards.find(c=>c.id==='quake'),x=q.x+q.w/2,y=q.y+q.h/2;
a.toolPickerDown(a.ev(1),x,y);a.toolPickerDown(a.ev(2),x,y);a.toolPickerUp(a.ev(2),x,y);
assert(a.WEB_UI.toolPicker&&a.pw.sel===null,'foreign touch cannot choose');
a.toolPickerUp(a.ev(1),x,y);
assert(!a.WEB_UI.toolPicker&&a.pw.sel==='quake'&&a.pw.left.quake===1,'choose once without spending');
assert.strictEqual(a.stats().saves,1);
assert(a.openToolPicker());a.pw.left.beans=0;
assert.strictEqual(a.toolPickerStatus('beans'),'USED');
assert(!a.toolPickerSelect('beans'),'spent disabled');
assert(!a.toolPickerSelect('valve'),'locked disabled');
assert(!a.toolPickerSelect('__proto__'),'unknown ID rejected');
assert.strictEqual(a.pw.sel,'quake','invalid choices preserve power');
a.clears(3);assert(a.toolPickerSelect('valve'));assert.strictEqual(a.pw.sel,null);
a.man.seated=true;a.man.vlv=true;a.man.x=10;
assert(a.openToolPicker());assert(a.toolPickerSelect('turbo'));
assert(!a.man.seated&&!a.man.vlv&&a.man.x===128,'changing valve restores sling');
assert(a.openToolPicker());a.toolPickerDown(a.ev(3),x,y);a.toolPickerCancel(3);a.toolPickerUp(a.ev(3),x,y);
assert(a.WEB_UI.toolPicker&&a.pw.sel==='turbo','cancelled touch cannot choose');
a.toolPickerDown(a.ev(4),x,y);a.toolPickerUp(a.ev(4),x+35,y);
assert(a.WEB_UI.toolPicker&&a.pw.sel==='turbo','slide cannot choose');
assert(a.toolPickerKey({code:'Escape',preventDefault(){}}));assert(!a.WEB_UI.toolPicker);
a.drag(true);assert(!a.openToolPicker(),'held pull blocks picker');a.drag(false);
a.pw.sel='burp';assert(!a.openToolPicker(),'charged soda protected');a.pw.sel=null;
a.man.mode='walk';assert(!a.openToolPicker(),'no tools during walk');a.man.mode='aim';
assert(a.openToolPicker());a.toolPickerKey({code:'Digit4',preventDefault(){}});assert(a.WEB_UI.toolPicker,'spent shortcut rejected');
a.toolPickerKey({code:'Digit5',preventDefault(){}});assert(!a.WEB_UI.toolPicker&&a.pw.sel==='towel','keyboard selects');
assert(html.includes('if(!WEB_UI.toolPicker)update(WEB_SIM_DT);'),'simulation pauses');
assert(html.includes('if(WEB_UI.toolPicker){const p=pt(e);toolPickerUp('),'real pointer-up routed');
assert(html.includes("if(state==='play'&&WEB_UI.toolPicker){toolPickerDown"),'real pointer-down routed');
assert(html.includes('WEB_UI.toolPicker=false;WEB_UI.toolPointer=null;\n  cancelAim()'),'gameplay reset closes picker');
new Function(cut('<script>','</script>').slice(8));
console.log('WEB_TOOL_PICKER_PASS layout_rects='+count+' touch_owner=1 drag_cancel=1 stock=1 valve=1 pause=1 keyboard=1');
