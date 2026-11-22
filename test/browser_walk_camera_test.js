/* Execute the real camera: readable portrait walking, safe tracking,
 * anchored zoom transitions and captured-aim scale stability. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const start=html.indexOf('function camTick('),end=html.indexOf('let AIM0=',start);
assert(start>=0&&end>start,'production camera and coordinate transforms exist');
const production=html.slice(start,end);
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
function scene(portrait=true,height=1169,wide=false){
  return {W:portrait?540:960,H:portrait?height:540,WW:wide?2880:960,WH:540,GY:480,
    portrait,clamp,isPortrait(){return this.portrait},
    CAM:{on:portrait||wide,z:1,x:0,y:0,reset:true,snap:false,hold:0},
    CL:wide?{slings:[[128,362],[1088,362],[2048,362]]}:{},
    slingK:wide?1:0,SX:wide?1088:128,SY:362,PULL:92,dragging:false,
    man:{mode:'walk',x:wide?1330:239.882,y:460.825,vx:0,vy:0,face:1,gr:true},
    targets:wide?[{alive:true,x:1600,zone:1},{alive:true,x:2600,zone:2}]:[{alive:true,x:724,zone:0}]};
}
function boot(env,source=production){
  // Bind isPortrait explicitly; the production helper closes over WEB_UI.
  env.isPortrait=()=>env.portrait;
  const api=new Function('env','with(env){'+source+
    '\nreturn {tick:camTick,screen:toScreen,world:toWorld};}')(env);
  return {...api,env};
}
const epsilon=1e-8;
let samples=0;
function finiteAndBounded(h){
  const e=h.env,c=e.CAM;
  for(const name of ['x','y','z'])assert(Number.isFinite(c[name]),'finite camera '+name);
  assert(c.z>=.55&&c.z<=1.8,'zoom remains within existing renderer budget');
  assert(c.x>=-epsilon&&c.x<=Math.max(0,e.WW-e.W/c.z)+epsilon,'camera stays within world width');
  for(const point of [[e.man.x,e.man.y],[0,480],[e.WW,0],[123.456,-78.9]]){
    const p=h.world(...h.screen(...point));
    assert(Math.abs(p[0]-point[0])<epsilon&&Math.abs(p[1]-point[1])<epsilon,
      'screen/world conversion remains invertible through zoom');
  }
  samples++;
}
function safeWalker(h){
  finiteAndBounded(h);
  const e=h.env,c=e.CAM,p=h.screen(e.man.x,e.man.y);
  assert(p[0]>=0&&p[0]<=e.W,'walker centre remains on screen at world edges');
  assert(p[1]-40*c.z>=138-epsilon,'walker head stays below mission/streak HUD');
  assert(p[1]+22*c.z<=e.H-174+epsilon,'walker feet stay above timer and controls');
}
for(const height of [720,960,1169,1240])for(const wide of [false,true]){
  const h=boot(scene(true,height,wide)),e=h.env;
  for(const x of [9,239.882,e.WW/2,e.WW-9]){
    e.man.x=x;e.man.y=460.825;e.CAM.reset=e.CAM.snap=true;e.CAM.hold=1.1;
    const before=JSON.stringify({man:e.man,targets:e.targets});
    h.tick(0);
    assert.strictEqual(e.CAM.z,1.8,'portrait walking uses a readable local view');
    assert.strictEqual(e.CAM.hold,0,'rotation into walking cannot inherit the aim hold');
    safeWalker(h);
    assert.strictEqual(JSON.stringify({man:e.man,targets:e.targets}),before,
      'camera does not move physics actors or alter targets');
    // Climb several floors, jump off a roof, then fall at stomp speed. The
    // camera must protect headroom/feet even before its ordinary ease catches up.
    for(let frame=0;frame<180;frame++){
      e.man.y=frame<90?460.825-frame*5:10.825+Math.min(450,(frame-90)*12);
      e.man.x=clamp(e.man.x+(frame%40<20?2.25:-2.25),9,e.WW-9);
      h.tick(1/60);safeWalker(h);
    }
  }
}
// A settled aim view eases in around the existing actor screen position.
const transition=boot(scene()),e=transition.env;
e.man.mode='aim';e.man.x=e.SX;e.man.y=e.SY;transition.tick(0);
const aimZoom=e.CAM.z;
assert(Math.abs(aimZoom-5/7)<epsilon,'fixture represents the captured small-character view');
e.man.mode='walk';e.man.x=239.882;e.man.y=460.825;
let previous=transition.screen(e.man.x,e.man.y),previousZoom=e.CAM.z;
for(let i=0;i<120;i++){
  transition.tick(1/60);safeWalker(transition);
  const point=transition.screen(e.man.x,e.man.y);
  assert(e.CAM.z>=previousZoom-epsilon&&e.CAM.z-previousZoom<.1,'walking zoom eases monotonically');
  assert(Math.abs(point[0]-previous[0])<12&&Math.abs(point[1]-previous[1])<12,
    'zooming around the actor avoids a sudden screen-position jump');
  previous=point;previousZoom=e.CAM.z;
}
assert.strictEqual(e.CAM.z,1.8,'walking focus settles without a second camera state');
assert(e.CAM.z/aimZoom>2.5,'captured portrait level gains over 2.5x character scale');
// Returning to a sling zooms out smoothly. Acquiring a new pull freezes the
// current scale, so a given screen drag still means one fixed world delta.
e.man.mode='aim';e.man.x=e.SX;e.man.y=e.SY;
for(let i=0;i<8;i++){
  const old=e.CAM.z;transition.tick(1/60);finiteAndBounded(transition);
  assert(e.CAM.z<old&&old-e.CAM.z<.1,'return-to-aim zoom also eases');
}
e.dragging=true;
const locked=e.CAM.z;
for(let i=0;i<60;i++){
  e.man.x=e.SX-50;e.man.y=e.SY+20;e.targets[0].x=600+i;
  transition.tick(1/60);finiteAndBounded(transition);
  assert.strictEqual(e.CAM.z,locked,'captured aiming scale cannot drift under the pointer');
  const a=transition.world(90,240),b=transition.world(150,270);
  assert(Math.abs(b[0]-a[0]-60/locked)<epsilon&&Math.abs(b[1]-a[1]-30/locked)<epsilon,
    'drag displacement retains its original scale');
}
e.dragging=false;
for(let i=0;i<150;i++){transition.tick(1/60);finiteAndBounded(transition)}
const fit=Math.max(e.targets[0].x+70,e.SX+340)-(e.SX-90);
assert(Math.abs(e.CAM.z-clamp(e.W/fit,.55,1.5))<epsilon,'released aim returns to target overview');
// The previous production camera is an independent oracle for landscape,
// including the 360-column finale, aiming, flight, walks, reset and snap.
const reference=`function camTick(dt){
  if(!CAM.on){CAM.z=1;CAM.x=0;CAM.y=0;return}
  let mx=0;for(const t of targets)if(t.alive&&(!CL.slings||t.zone===slingK))mx=Math.max(mx,t.x);
  const fitW=Math.max(mx+70,SX+340)-(SX-90),z=CL.slings?Math.min(1.8,H/600):clamp(W/fitW,.55,1.5),vw=W/z,vh=H/z,baseY=WH-vh*.86;CAM.z=z;
  let sx=0,n=0;for(const t of targets)if(t.alive){sx+=t.x;n++}
  if(CL.slings){n=0;sx=0;for(const t of targets)if(t.alive&&t.zone===slingK){sx+=t.x;n++}}
  const cameraMaxX=Math.max(0,WW-vw);
  const bL=CL.slings?clamp((n?sx/n:640)-vw*.5,0,cameraMaxX):clamp(SX-90,0,cameraMaxX),iL=CL.slings?clamp(SX-vw*.2,0,cameraMaxX):bL,m=man;
  let tl=bL,ty=baseY,fast=4;
  if(CAM.hold>0){CAM.hold-=dt}
  else if(m.mode==='aim'){if(dragging){const k=Math.min(1,Math.hypot(m.x-SX,m.y-SY)/PULL);tl=iL+(bL-iL)*k*k*(3-2*k);fast=8}else tl=iL}
  else if(m.mode==='fly'||m.mode==='stuck'||m.mode==='walk'){tl=clamp(m.x-vw*.38,0,cameraMaxX);ty=Math.min(baseY,m.y-vh*.3);fast=5}
  if(CAM.reset){CAM.reset=false;CAM.snap=false;CAM.x=bL;CAM.y=baseY;CAM.hold=1.1;return}
  if(CAM.snap){CAM.snap=false;CAM.x=tl;CAM.y=ty;return}
  CAM.x+=(tl-CAM.x)*Math.min(1,dt*fast);CAM.y+=(ty-CAM.y)*Math.min(1,dt*3);CAM.x=clamp(CAM.x,0,cameraMaxX)
}
const toWorld=(sx,sy)=>CAM.on?[sx/CAM.z+CAM.x,sy/CAM.z+CAM.y]:[sx,sy];
const toScreen=(x,y)=>CAM.on?[(x-CAM.x)*CAM.z,(y-CAM.y)*CAM.z]:[x,y];`;
for(const wide of [false,true]){
  const live=boot(scene(false,540,wide)),old=boot(scene(false,540,wide),reference);
  for(let i=0;i<360;i++){
    for(const h of [live,old]){
      h.env.man.mode=['aim','fly','stuck','walk','done','none'][Math.floor(i/60)];
      h.env.man.x=128+i*3;h.env.man.y=460-250*Math.sin(i/60);
      h.env.dragging=i%23<10;h.env.CAM.snap=i%89===88;
      h.tick([1/60,1/30,1/90][i%3]);
    }
    assert.deepStrictEqual(live.env.CAM,old.env.CAM,'landscape preserves complete reference camera state');
    finiteAndBounded(live);
  }
}
console.log('WEB_WALK_CAMERA_PASS samples='+samples+' heights=4 worlds=2 actor_scale=2.52x drag_scale_locked=1 landscape_reference=1');
