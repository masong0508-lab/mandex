'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
function cameraRegression(html, assert){
  const cut=(a,b)=>{const i=html.indexOf(a),j=html.indexOf(b,i+a.length);if(i<0||j<=i)throw Error("missing "+a+" / "+b);return html.slice(i,j)};
  const make=new Function(`
    const events={},globalEvents={},captures=[];
    const cv={addEventListener(n,fn){(events[n]||=[]).push(fn)},setPointerCapture(id){captures.push(id)},getBoundingClientRect(){return {left:0,top:0,width:W,height:H}}};
    const addEventListener=(n,f)=>{(globalEvents[n]||=[]).push(f)};
    const document={addEventListener(){},hidden:false};
    let W=960,H=540,WW=2880,WH=540,GY=480,SX=128,SY=362,VX=36,VY=420,PULL=92,VKEEP=44,VGRAB=40,VPULL=76,COLS=360,state='play',dragging=false,sodaHold=false,sliderDrag=false,mouseX=0,mouseY=0,launches=0,dives=0;
    let man={mode:'aim',x:SX,y:SY,seated:false,vlv:false,G:null};
    const CL={},targets=[],KEYS={},TID={},pw={left:{burp:1}},clamp=(n,a,b)=>Math.max(a,Math.min(b,n));
    const audioInit=()=>{},gagReset=()=>{},hitTest=()=>false,toolOn=()=>false,valveOpen=()=>false,launch=()=>{launches++},dive=()=>{dives++},walkTouch=()=>false,releaseWalkTouch=()=>{},pdSave=()=>{},SFX={boing(){},plop(){},valveCrack(){}};
    ${cut('const CAM=','// the last pass over the picture')}
    ${cut('function pt(e){','function retry(')}
    ${cut("cv.addEventListener('pointerdown',e=>{","// =====================================================================\n//  META:")}
    function send(name,id,x,y,extras={}){for(const f of events[name]||[])f({pointerId:id,clientX:x,clientY:y,button:0,pointerType:'touch',preventDefault(){},...extras})}
    function globalSend(name){for(const f of globalEvents[name]||[])f({})}
    return {send,globalSend,CAM,man,tick:camTick,world:toWorld,screen:toScreen,mode:(on)=>cameraMode(on),stats:()=>({dragging,launches,dives}),state:v=>{state=v}};
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
  return 'WEB_MANUAL_CAMERA_PASS aim_owner=1 lost_capture=1 pan=1 pinch=1 wheel=1 bounds=1 blur=1';
}
console.log(cameraRegression(fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8'),assert));
