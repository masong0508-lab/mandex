'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
function animationRegression(html,assert,section){
 const cut=(a,b)=>{const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,'missing source '+a);return html.slice(i,j)};
 const near=(a,b,msg)=>assert(Math.abs(a-b)<1e-7,msg+': '+a+' vs '+b);
 if(!section||section==='dive'){
  const boot=new Function(`
    let man={mode:'fly',vx:-400,vy:-120,ang:-.35,spin:0,face:-1,dive:false,inf:.6,bel:.1};
    let time=0,shake=0,INF=0,JIG=null,painted=0;
    const SFX={pop(){}},ring=()=>{},bgrow=x=>x,stom=m=>m.inf+m.bel;
    const jigSquash=()=>{},jigOpts=()=>null,ragOpts=()=>{};
    const ctx={save(){},restore(){},translate(){},rotate(){},scale(){}};
    const drawBall=()=>{painted=INF},drawSide=()=>{};
    ${cut('function flightFaceFor(','function tubeStep(')}
    ${cut('function dive(){','function solidAt(')}
    ${cut('function renderMan(){','const LORD_NEW=true;')}
    return {man,dive,render:renderMan,painted:()=>painted,step:dt=>advanceDivePose(man,dt)};
  `)();
  const m=boot.man;boot.dive();near(m.spin,m.ang,'dive must start at current pitch');
  near(m.vx,-160,'horizontal dive velocity unchanged');near(m.vy,780,'downward dive velocity unchanged');
  boot.render();near(boot.painted(),.7,'dive silhouette preserves inflation');
  const initial=m.spin;boot.step(.01);assert(m.spin-initial>0&&m.spin-initial<.14,'dive rotation accelerates');
  const coarse=m.spin;boot.step(.19);const after=m.spin;m.spin=initial;m.diveAge=0;
  for(let i=0;i<20;i++)boot.step(.01);near(m.spin,after,'dive rotation is timestep independent');
  assert(after>coarse,'dive continues tumbling');
 }
 if(!section||section==='sling'){
  const start=html.indexOf('function slingBandPoint(');
  assert(start>=0,'elastic sling band pose exists');
  const boot=new Function(`
    let SX=128,SY=362,time=10;
    ${cut('const SLING_BAND_POINT=','function drawSling(back)')}
    return {point:slingBandPoint,clock:v=>{time=v}};
  `)();
  const m={mode:'aim',x:50,y:405};const a=boot.point(m,true),ax=a.x,ay=a.y,b=boot.point(m,false),bx=b.x,by=b.y;
  m.mode='fly';m.recoilAt=10;m.recoilSX=128;m.recoilSY=362;m.recoilDX=78;m.recoilDY=-43;
  const first=boot.point(m,true);near(first.x,ax,'back band continuous on release');near(first.y,ay,'back band y continuous');
  const second=boot.point(m,false);near(second.x,bx,'front band continuous on release');near(second.y,by,'front band y continuous');
  boot.clock(10.1);const mid=boot.point(m,true);assert(mid.x>ax&&mid.x<180,'elastic band returns toward posts');
  boot.clock(10.5);const rest=boot.point(m,true);near(rest.x,128,'band settles at center');near(rest.y,390,'band settles below post');
  m.recoilSX=1200;boot.clock(10.05);near(boot.point(m,true).x,128,'old district recoil cannot leak into new sling');
 }
 if(!section||section==='rag'){
  const start=html.indexOf('function ragXf(');
  const api=new Function(html.slice(start,html.indexOf('function ragHit(',start))+'return {xf:ragXf,w:rW,l:rL};')();
  for(const face of [-1,1]){
   const m={mode:'fly',face,ang:.42,x:100,y:210};const x=api.xf(m);
   near(x.a,m.ang*face,'rag pose follows mirrored pitch');near(x.sx,.64*face,'rag pose follows mirrored horizontal scale');
   const p=api.w(x,m,-23,17),q=api.l(x,m,...p);near(q[0],-23,'rag inverse X');near(q[1],17,'rag inverse Y');
  }
  assert(html.includes('Math.abs(X.sx)+Math.abs(X.sy)'),'mirrored rag bone lengths stay positive');
 }
 if(!section||section==='landlord'){
  assert(html.includes('function lordAttackPose('),'continuous landlord attack pose exists');
  const f=new Function(cut('function lordAttackPose(','function cgLordBig(')+'return lordAttackPose;')();
  const sample=(windTime,swing,kind='thrower',b2=false)=>{const out={};f(2,{kind,windTime,swing,b2},out);return out};
  const dist=(a,b)=>Math.hypot(a.hx-b.hx,a.hy-b.hy);
  assert(dist(sample(0,0),sample(.6,0))<1e-7,'windup begins from idle');
  assert(dist(sample(.001,0),sample(0,.3))<1e-4,'windup joins strike without a jump');
  assert(dist(sample(.3,0),sample(.001,0))>20,'final windup drives arm into release');
  assert(sample(0,.3).hx<-30,'arm is extended exactly when projectile is released');
  assert(dist(sample(0,.3),sample(0,.12))>10,'strike has visible recovery');
  assert(dist(sample(0,.001),sample(0,0))<.01,'attack recovers smoothly');
  assert(dist(sample(.1,.001,'boss',true),sample(.1,0,'boss',true))<.01,'boss second windup joins recovery');
  assert(dist(sample(0,.35,'bat'),sample(0,0,'bat'))<1e-7,'unannounced bat swing starts from neutral');
  assert(html.includes('lordAttackPose(t,o,LORD_ARM_POSE)'),'production renderer consumes interpolated pose');
 }
 if(!section||section==='splat'){
  assert(html.includes('function splatGrowth('),'impact splat has animated growth');
  const f=new Function(cut('function splatGrowth(','function drawGooSplat(')+'return splatGrowth;')();
  const a=f(0),b=f(.06),c=f(.18);assert(a>0&&a<b&&b<c&&c===1,'splat expands gradually');near(f(2),1,'splat size settles');
  assert(html.includes('const grow=splatGrowth(t)'),'actual splat painter uses growth');
 }
 return 'WEB_ANIMATION_MOTION_PASS '+(section||'dive sling rag landlord splat');
}
console.log(animationRegression(fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8'),assert));
