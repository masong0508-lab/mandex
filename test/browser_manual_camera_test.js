'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
function cameraRegression(html, assert){
  const cut=(a,b)=>{const i=html.indexOf(a),j=html.indexOf(b,i+a.length);if(i<0||j<=i)throw Error("missing "+a+" / "+b);return html.slice(i,j)};
  const make=new Function(`
    const events={},globalEvents={},captures=[];
    const cv={style:{},width:960,height:540,addEventListener(n,fn){(events[n]||=[]).push(fn)},setPointerCapture(id){captures.push(id)},getBoundingClientRect(){return {left:0,top:0,width:W,height:H}}};
    const addEventListener=(n,f)=>{(globalEvents[n]||=[]).push(f)};
    const window={innerWidth:390,innerHeight:844,visualViewport:{width:390,height:844}};
    const document={addEventListener(){},hidden:false,body:{getBoundingClientRect(){return {width:window.innerWidth,height:window.innerHeight}}}};let _vig=null;
    let W=960,H=540,WW=2880,WH=540,GY=480,SX=128,SY=362,VX=36,VY=420,PULL=92,VKEEP=44,VGRAB=40,VPULL=76,COLS=360,state='play',dragging=false,sodaHold=false,sliderDrag=false,mouseX=0,mouseY=0,launches=0,dives=0,uiHits=0;
    let man={mode:'aim',x:SX,y:SY,seated:false,vlv:false,G:null};
    const CL={},targets=[],KEYS={},TID={},pw={left:{burp:1}},clamp=(n,a,b)=>Math.max(a,Math.min(b,n));
    const audioInit=()=>{},gagReset=()=>{},hitTest=(x,y)=>{if(state==='menu'||(x>800&&y<50)){uiHits++;return true}return false},toolOn=()=>false,valveOpen=()=>false,launch=()=>{launches++},dive=()=>{dives++},pdSave=()=>{},SFX={boing(){},plop(){},valveCrack(){}};
    ${cut('const CAM=','// the last pass over the picture')}
    ${cut('function releaseWalkTouch(id){','function drawWalkHUD(')}
    ${cut('function pt(e){','function retry(')}
    ${cut("cv.addEventListener('pointerdown',e=>{","// =====================================================================\n//  META:")}
    function send(name,id,x,y,extras={}){for(const f of events[name]||[])f({pointerId:id,clientX:x,clientY:y,button:0,pointerType:'touch',preventDefault(){},...extras})}
    function globalSend(name){for(const f of globalEvents[name]||[])f({})}
    return {send,globalSend,CAM,man,tick:camTick,layout:layoutTick,viewport:h=>{window.visualViewport.height=h;layoutTick();return cv.style.height},world:toWorld,screen:toScreen,mode:(on)=>cameraMode(on),stats:()=>({dragging,launches,dives,uiHits}),state:v=>{state=v},portrait:()=>{W=540;H=960;WEB_UI.portrait=true},input:()=>({keys:{...KEYS},owners:{...TID},gesture:WEB_UI.gesture})};
  `);
  const h=make();h.send('pointerdown',1,128,362);assert(h.stats().dragging,'owner starts aiming');h.send('pointermove',1,70,362);const x=h.man.x;
  h.send('pointerdown',2,800,100);h.send('pointermove',2,700,100);assert(h.man.x===x,'foreign pointer cannot move sling');h.send('pointerup',2,700,100);assert(h.stats().launches===0&&h.stats().dragging,'foreign release cannot launch');h.send('pointercancel',2,700,100);assert(h.stats().dragging,'foreign cancel cannot clear aim');h.send('pointerup',1,70,362);assert(h.stats().launches===1&&!h.stats().dragging,'owner releases exactly once');
  h.man.x=128;h.man.y=362;h.send('pointerdown',3,128,362);h.send('pointermove',3,60,362);h.send('lostpointercapture',3,60,362);assert(!h.stats().dragging&&h.man.x===128,'lost capture safely restores sling');h.send('pointerup',3,60,362);assert(h.stats().launches===1,'lost capture cannot launch');
  h.mode(true);assert(h.CAM.on&&h.CAM.manual,'manual inspection enables landscape camera');
  h.send('pointerdown',4,500,300);h.send('pointermove',4,350,300);assert(h.CAM.x>100,'pan responds immediately');
  h.send('pointerdown',5,550,300);const anchor=h.world(450,300);h.send('pointermove',5,750,300);assert(h.CAM.z>1,'two fingers zoom');const now=h.world(550,300);assert(Math.abs(now[0]-anchor[0])<1e-7&&Math.abs(now[1]-anchor[1])<1e-7,'pinch keeps midpoint world anchor');
  const old=h.CAM.x;h.send('pointerup',5,750,300);h.send('pointermove',4,350,300);assert(Math.abs(h.CAM.x-old)<1e-7,'lifting one finger does not jump');h.send('pointermove',4,-1e5,300);assert(h.CAM.x<=2880-960/h.CAM.z+1e-7,'pan clamps to world');h.send('pointercancel',4,350,300);
  h.tick(.05);assert(h.CAM.manual,'auto tracking cannot override manual view');assert(h.stats().launches===1&&h.stats().dives===0,'camera gestures do not fire gameplay actions');
  h.send('wheel',0,450,280,{deltaY:-120,deltaMode:0,pointerType:'mouse'});assert(h.CAM.z<=1.8&&h.CAM.z>1,'wheel zoom bounded');
  h.mode(false);assert(!h.CAM.manual,'follow restores auto camera');h.send('pointerdown',6,128,362);h.globalSend('blur');assert(!h.stats().dragging,'blur clears input');h.send('pointerup',6,128,362);assert(h.stats().launches===1,'blur cannot cause launch');
  const gesture=make();gesture.mode(true);gesture.send('pointerdown',11,500,300);gesture.send('pointerdown',12,900,20);gesture.send('pointerup',12,900,20);assert(gesture.stats().uiHits===0,'second camera finger cannot activate HUD');gesture.send('pointercancel',11,500,300);
  const walk=make();walk.portrait();walk.man.mode='walk';
  walk.send('pointerdown',1,30,880);assert(walk.input().keys.TL,'left held');
  walk.send('pointermove',1,92,880);assert(!walk.input().keys.TL,'leaving button must release left');
  assert(Object.hasOwn(walk.input().owners,'1'),'gap retains pointer ownership');
  walk.send('pointermove',1,110,880);assert(walk.input().keys.TR,'slide back onto right button');
  walk.send('pointerdown',2,110,880);walk.send('pointermove',1,92,880);
  assert(walk.input().keys.TR,'another thumb still holds right');
  walk.send('pointerup',2,110,880);assert(!walk.input().keys.TR,'last right owner released');
  walk.send('pointermove',1,30,880);assert(walk.input().keys.TL,'gap pointer can return again');
  walk.send('pointermove',99,110,880);assert(!walk.input().keys.TR,'foreign move cannot press right');
  walk.mode(true);assert(!walk.input().keys.TL&&Object.keys(walk.input().owners).length===0,'camera mode clears movement owners');
  walk.mode(false);walk.send('pointermove',1,30,880);assert(!walk.input().keys.TL,'old finger cannot resume after camera mode');
  walk.send('pointerdown',3,30,880);walk.send('pointermove',3,92,880);walk.send('lostpointercapture',3,92,880);
  assert(Object.keys(walk.input().owners).length===0,'lost capture cleans gap owner');
  walk.send('pointerdown',4,110,880);walk.globalSend('blur');assert(!walk.input().keys.TR&&Object.keys(walk.input().owners).length===0,'blur clears touch owner');
  const menu=make();menu.portrait();menu.state('menu');
  menu.send('pointerdown',10,100,100);menu.send('pointerdown',11,200,100);
  assert(menu.input().gesture.id===10,'second thumb must not steal menu gesture');
  menu.send('pointerup',11,200,100);assert(menu.stats().uiHits===0,'foreign release must not activate menu');
  menu.send('pointerup',10,100,100);assert(menu.stats().uiHits===1,'original owner activates menu exactly once');
  menu.send('pointerdown',12,100,100);menu.send('lostpointercapture',12,100,100);
  assert(!menu.input().gesture,'lost menu capture clears gesture');
  menu.send('pointerup',12,100,100);assert(menu.stats().uiHits===1,'lost capture cannot activate menu');
  const aim=make();aim.portrait();aim.CAM.on=true;aim.CAM.z=1;
  const [ax,ay]=aim.screen(aim.man.x,aim.man.y);
  aim.send('pointerdown',31,ax+20,ay-5);
  assert(aim.stats().dragging,'off-centre thumb owns portrait aim');
  aim.send('pointermove',31,ax+20,ay-5);
  assert(aim.man.x===128&&aim.man.y===362,'zero pointer travel must not move character toward thumb');
  aim.CAM.z=1.5;aim.send('pointermove',31,ax-40,ay+15);
  assert(Math.abs(aim.man.x-68)<1e-7&&Math.abs(aim.man.y-382)<1e-7,'captured scale controls relative drag');
  aim.send('pointercancel',31,ax-40,ay+15);
  assert(aim.man.x===128&&aim.man.y===362&&aim.stats().launches===0,'cancel restores original sling');
  aim.CAM.z=.55;const [bx,by]=aim.screen(128,362);
  aim.send('pointerdown',32,bx+65,by);
  assert(!aim.stats().dragging,'portrait pickup radius remains bounded when zoomed out');
  const resize=make();resize.layout();const [rx,ry]=resize.screen(128,362);
  resize.send('pointerdown',41,rx,ry);resize.send('pointermove',41,rx-30,ry);
  assert(resize.stats().dragging&&resize.man.x<128,'resize fixture has an owned pull');
  const css=resize.viewport(340);
  assert(!resize.stats().dragging&&resize.man.x===128&&resize.stats().launches===0,'CSS-only viewport change cancels old pull without launch');
  assert(parseFloat(css)<=340,'keyboard viewport bounds CSS height even with stale body rectangle');
  resize.send('pointerup',41,rx-30,ry);assert(resize.stats().launches===0,'stale resize release never launches');
  return 'WEB_MANUAL_CAMERA_PASS aim_owner=1 lost_capture=1 pan=1 pinch=1 wheel=1 bounds=1 blur=1 walk_gap=1 menu_owner=1 relative_aim=1 fixed_pickup=1 viewport_cancel=1';
}
console.log(cameraRegression(fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8'),assert));
