'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function get(a,b){const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,a);return html.slice(i,j)}
const source=get('function legIK(', '// the baked hand anchor')+get('const RESULT_READY=', 'function drawResultActor(')+get('function walkTurnAngle(', 'function walkPunchFxStage(');
const run=new Function('assert',`let time=0;const V=8,clamp=(x,a,b)=>Math.max(a,Math.min(b,x));`+source+`
let n=0;const ok=(v,m)=>{assert(v,m);n++},near=(a,b,m)=>ok(Math.abs(a-b)<1e-6,m);
const len=(a,b)=>Math.hypot(a[0]-b[0],a[1]-b[1],a[2]-b[2]);
for(const l1 of [4,15.3,30])for(const l2 of [3,16,24])for(const d of [0,.001,.15,4,15,31,60])for(const p of [[0,0,0],[1,0,0],[0,1,0],[0,0,1]]){
 const H=[3,-2,7],A=[3+d,-2,7],r=legIK(H,A,p,l1,l2);
 ok(r.K.every(Number.isFinite)&&r.A.every(Number.isFinite),'finite IK');
 near(len(H,r.K),l1,'upper bone');near(len(r.K,r.A),l2,'lower bone');
}
const copy=q=>[q.th,q.hip,q.walkBob,q.hr,...q.arms.L,...q.arms.R];
for(const win of [true,false])for(let variant=0;variant<3;variant++){
 const id=win?variant:3,dur=RESULT_CLIPS[id].at(-1)[0];let prev=null;
 for(let i=0;i<=Math.ceil(dur*120);i++){
  const q=resultMotionSample(win,i/120,variant),v=copy(q);
  ok(v.every(Number.isFinite),'finite result pose');
  ok(q.walkFeet.every(f=>f.y===55&&f.ang===0),'planted feet');
  if(prev)ok(Math.max(...v.map((x,k)=>Math.abs(x-prev[k])))<6,'no abrupt result pose jumps');prev=v;
  for(let j=0;j<2;j++){
   const s=j?1:-1,f=q.walkFeet[j],C=Math.cos(q.th+q.psi*Math.PI/180),S=Math.sin(q.th+q.psi*Math.PI/180);
   const H=[s*10*C+q.hip*.6,24+q.walkBob,-s*10*S],A=[f.x,f.y,f.z],ik=legIK(H,A,[1,0,0],15.3,16);
   near(ik.A[1],55,'result feet remain reachable');
  }
 }
 if(win){const a=copy(resultMotionSample(true,0,variant)),b=copy(resultMotionSample(true,dur,variant));ok(a.every((x,k)=>x===b[k]),'loop closes exactly')}
 else {const a=copy(resultMotionSample(false,dur,variant)),b=copy(resultMotionSample(false,dur+8,variant));ok(a.every((x,k)=>x===b[k]),'defeat settles')}
}
for(const face of [-1,1])for(const gr of [false,true]){
 const m={mode:'walk',face,gr,gait:8,animStride:.6,animAir:gr?0:1,hurtCd:.8,pT:0,vy:0};
 walkPose(m);ok(WALK_SPIN_POSE.face==='queasy'&&Math.abs(WALK_SPIN_POSE.hr)>0,'visible knockback reaction');
 m.pT=.2;ok(walkPose(m)===face,'attack facing preserved');ok(WALK_SPIN_POSE.arms.L[5]>45,'attack overrides hurt pose');
 m.hurtCd=0;m.pT=0;walkPose(m);near(WALK_SPIN_POSE.hr,0,'no stale head roll');
}
return n;`)(assert);
assert(html.includes('drawResultActor(win,Math.max(0,time-resultStart)'),'result painter connected');
new Function(html.slice(html.indexOf('<script>')+8,html.indexOf('</script>',html.indexOf('<script>'))));
console.log('ACTION_ANIMATION_PASS checks='+run+' victory_clips=3 defeat_clip=1 IK=1 walk_recoil=1');
