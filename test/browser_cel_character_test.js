'use strict';
/* Exercise production curves and movement-conditioned expressions without
 * modifying gameplay, relying on screenshots, or creating synthetic frames. */
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function cut(a,b){const x=html.indexOf(a),y=html.indexOf(b,x+a.length);assert(x>=0&&y>x,a);return html.slice(x,y)}
let operations=0,depth=0,paths=0,marks=[];
class Shape{
  constructor(){this.points=[];paths++}
  moveTo(...a){this.points.push(a)}
  bezierCurveTo(...a){this.points.push(a)}
  closePath(){}
}
const ctx=new Proxy({},{
  get(t,k){
    if(k==='save')return()=>depth++;
    if(k==='restore')return()=>{assert(depth>0,'unbalanced canvas state');depth--};
    if(k in t)return t[k];
    return(...a)=>{assert(a.every(v=>typeof v!=='number'||Number.isFinite(v)),'nonfinite drawing parameter: '+k);operations++;marks.push(k)};
  },
  set(t,k,v){t[k]=v;return true}
});
const run=new Function('ctx','Path2D','assert','marks',
  'let CEL_ACTIVE=true,SK="#eab17f",HAIR="#4a2d1a",OUT="#5a2f1b";'+
  'const blob=(parts)=>{for(const p of parts){assert(p.bb.every(Number.isFinite));assert(p.p.points.flat().every(Number.isFinite));}};'+
  'const ell=(...a)=>ctx.ellipse(...a.slice(0,4),0,0,7);'+
  cut('function celCharacter(){','function frontHair(){')+
  cut('function celEyeAt(','function brow(')+
  cut('function mouthOpen(','function grin(')+
  cut('const CEL_WALK_ACT=','function drawWalkMan(')+
  'return {head:celHeadShape,features:celProfileFeatures,eye:celEyeAt,walk:celWalkActing};')(ctx,Shape,assert,marks);
for(let angle=0;angle<=360;angle+=5){
  const t=angle*Math.PI/180,jx=3.8*Math.sin(t),rx=Math.hypot(14.3*Math.cos(t),13.5*Math.sin(t));
  const q=run.head(0,-44,rx,13.3,jx,-34,11.4,8.2);
  assert(q.bb[0]<=-rx&&q.bb[2]>=rx&&q.bb[1]===-57.3&&q.bb[3]>=-25.8);
  assert(q.p.points.length===5,'one closed jaw/skull contour');
  assert(q.p.points.flat().every(Number.isFinite));
}
let expressions=new Set(),cases=0;
for(const f of [0,.05,.3,.45,.7,1,1.4,2])for(const pose of ['seat','flight']){
  run.features(4,-70,13,f,pose);assert(depth===0);cases++;
}
for(const gr of [true,false])for(const stomp of [true,false])for(const vy of [-500,-200,0,250,500])for(const pT of [0,.14])for(const landFx of [0,.14])for(const step of [-1,0,1]){
  const m={gr,stomp,vy,pT,landFx},before=JSON.stringify(m),a=run.walk(m,step,false);
  assert(JSON.stringify(m)===before,'acting mutated physics');
  assert(Math.abs(a.pitch)<=10&&a.lead>=9&&a.lead<=15);
  expressions.add(a.face);cases++;
}
assert(expressions.size>=4&&operations>100&&paths===73&&depth===0);
const a=html.indexOf('<script>'),b=html.indexOf('</script>',a);new Function(html.slice(a+8,b));
console.log('GOLDEN_CELS_CHARACTER_PASS poses='+cases+' head_angles=73 expressions='+expressions.size+' balanced_canvas=1');

/* Exercise the new full soda choreography using exact production timing. */
const chug=new Function(
  'const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));'+
  cut('const stom=m=>','const burpMult=')+
  cut('const CT_A=','function startChug()')+
  cut('const CEL_CHUG=','function drawChugMan(')+
  'return {pose:celChugPose,A:CT_A,BK:CT_BK,C:CT_C,TB:CT_TB,D:CT_D,END:CT_END};')();
let chugCases=0;
const copy=o=>JSON.parse(JSON.stringify(o));
for(const keep of [0,.8,4])for(const rel of [false,true]){
  let prev=null;
  for(let i=0;i<=360;i++){
    const t=chug.END*i/360;
    const ph=t<chug.A?0:t<chug.BK?1:t<chug.C?2:t<chug.TB?3:t<chug.D?4:5;
    const th=ph===1?Math.PI*((t-chug.A)/(chug.BK-chug.A)):ph===2?Math.PI:ph===3?Math.PI+Math.PI*((t-chug.C)/(chug.TB-chug.C)):0;
    const m={inf:.2,bel:.4,chug:{t,ph,th,rel,keep:ph>=4?keep:0}};
    const before=JSON.stringify(m),p=copy(chug.pose(m,t));
    assert(before===JSON.stringify(m),'chug art changed simulation');
    assert(p.arms.L.concat(p.arms.R,[p.pressure,p.hp,p.th]).every(Number.isFinite));
    assert(p.pressure>=0&&p.pressure<=1.6);
    if(p.bottle)assert(p.bottle.level>=0&&p.bottle.level<=1&&Number.isFinite(p.bottle.angle));
    if(prev)assert(Math.hypot(p.arms.R[3]-prev.arms.R[3],p.arms.R[4]-prev.arms.R[4])<12,'hand popped across keyframes');
    prev=p;chugCases++;
  }
}
const sample={inf:.1,bel:.7,chug:{t:chug.D,ph:5,th:0,rel:true,keep:3}};
const rec=copy(chug.pose(sample,chug.D));assert(rec.bottle&&rec.bottle.level===0,'recovery lost bottle at first frame');
sample.chug.t=chug.END;assert(!chug.pose(sample,chug.END).bottle,'recovery retained bottle');
const face=new Function(cut('const GRIN=','function eye3(')+cut('function celExpressionBlend(','function spinLive(')+'return {blend:celExpressionBlend,faces:FACE3};')();
let blends=0;
for(const a of Object.keys(face.faces))for(const b of Object.keys(face.faces))for(const k of [0,.1,.5,.9,1]){
 const m=face.blend(a,b,k);assert([m.w,m.d,m.y,m.lift].every(Number.isFinite));blends++;
 if(k===0)assert(Math.abs(m.w-face.faces[a].m.w)<1e-9);
 if(k===1)assert(Math.abs(m.d-face.faces[b].m.d)<1e-9);
}
console.log('GOLDEN_CELS_MOTION_PASS chug_samples='+chugCases+' expression_blends='+blends+' smooth_grip=1');

/* A grounded walk driven by collision-resolved distance, not the clock. */
const stride=new Function(
  'const V=8;'+cut('const CEL_WALK_ACT=','function drawWalkMan(')+
  'return {rig:typeof celWalkRig==="function"?celWalkRig:null,foot:typeof celWalkFoot==="function"?celWalkFoot:null,hips:typeof celWalkHipBob==="function"?celWalkHipBob:null};')();
assert(stride.rig&&stride.foot&&stride.hips,'missing articulated grounded walking rig');
const m0={gr:true,turn:0,gait:0,vx:120},beforeM=JSON.stringify(m0);
const rig0=stride.rig(m0);
assert(JSON.stringify(m0)===beforeM&&stride.rig(m0)===rig0,'walking rig mutated gameplay or allocated per tick');
let strideSamples=0,reachSamples=0,contacts=0;
for(const speed of [0,10,50,100,200])for(let k=0;k<=512;k++){
  const m={gr:true,turn:0,gait:32*k/512,vx:speed},before=JSON.stringify(m);
  const pose=stride.rig(m);
  assert(JSON.stringify(m)===before&&pose===rig0&&pose.L&&pose.R);
  for(const foot of [pose.L,pose.R]){
    assert(Number.isFinite(foot.travel)&&Number.isFinite(foot.lift));
    assert(Math.abs(foot.travel)<=8/(2*.36)+1e-9&&foot.lift>=0&&foot.lift<=6);
    if(speed===0)assert(foot.travel===0&&foot.lift===0);
  }
  assert(pose.L.lift===0||pose.R.lift===0,'no planted foot');
  if(speed>=100&&k<512){
    const next={travel:0,lift:0};
    for(const side of [-1,1]){
      const a={travel:0,lift:0};
      stride.foot(m.gait/(2*8),side,1,a);
      stride.foot((m.gait+1e-4)/(2*8),side,1,next);
      if(a.lift===0&&next.lift===0){
        assert(Math.abs((next.travel-a.travel)*.36+1e-4)<1e-8,'stance foot slid');
        contacts++;
      }
    }
  }
  const beforePhase=JSON.stringify(pose);
  const hips=stride.hips(2,2.5,10,0,14,pose.L.travel,pose.L.lift,0);
  assert(Number.isFinite(hips)&&hips>=2);
  const dx=pose.L.travel-2.5,dz=14-10;
  assert(Math.hypot(dx,55-pose.L.lift-(24+hips),dz)<=31.15+1e-7,'leg exceeds reach');
  assert(JSON.stringify(pose)===beforePhase);
  strideSamples++;reachSamples++;
}
assert(stride.rig({...m0,gr:false})===null&&stride.rig({...m0,turn:5})===null,'turn/air must retain original pose');
assert(contacts>500);
assert(html.includes('WALK_SPIN_POSE.walk=rig')&&html.includes('celWalkHipBob(bob,')&&html.includes('walkTravel'), 'foot trajectory not connected to live IK renderer');
console.log('GOLDEN_CELS_STRIDE_PASS poses='+strideSamples+' planted_contacts='+contacts+' reach='+reachSamples);
