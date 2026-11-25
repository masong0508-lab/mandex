'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function slice(a,b){const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,a);return html.slice(i,j);}
const source=slice('function walkTurnAngle(', 'function walkPunchFxStage(')+slice('function legIK(', '// the baked hand anchor')+slice('function ragXf(', 'function ragHit(')+slice('const rubbleMotion=', 'function drawRubbleCached(');
const test=new Function('assert', 'const V=8,time=0,clamp=(v,a,b)=>Math.max(a,Math.min(b,v));let chunks=[];'+source+`
const near=(a,b)=>assert(Math.abs(a-b)<1e-7);
let planted=0;
for(let i=0;i<96;i++){
 const m={gr:true,face:1,gait:i/96*32,animStride:1,animAir:0,animLand:0,animStomp:0};
 walkPose(m);const q=WALK_SPIN_POSE,C2=Math.cos(q.th+q.psi*Math.PI/180),S2=Math.sin(q.th+q.psi*Math.PI/180);
 for(let j=0;j<2;j++){const f=q.walkFeet[j],s=j?1:-1,H=[s*10*C2+q.hip*.6,24+q.walkBob,-s*10*S2],A=[f.x,f.y,f.z],ik=legIK(H,A,[1,0,0],15.3,16);assert(ik.K.every(Number.isFinite));if(f.y===55){near(ik.A[1],55);planted++;}}
}
assert(planted>=96);assert.deepStrictEqual(walkFootSample(0),walkFootSample(1));
for(const face of [-1,1]){
 const m={mode:'walk',face,turn:8,pT:.2};assert.strictEqual(walkPose(m),face);assert(WALK_SPIN_POSE.arms.L[5]>45);
 m.mode='fly';m.ang=.4;m.x=250;m.y=180;const X=ragXf(m);near(X.sx,.64*face);const w=rW(X,m,-12,8),r=rL(X,m,w[0],w[1]);near(r[0],-12);near(r[1],8);
}
let ch={x:20,y:30,ang:Math.PI-.1};chunks=[ch];snapshotRubble();ch.x=24;ch.y=34;ch.ang=-Math.PI+.1;rubbleAlpha=.5;let r=rubbleRenderPose(ch);near(r.x,22);near(r.y,32);near(r.ang,Math.PI);near(ch.ang,-Math.PI+.1);
return planted;`)(assert);
console.log('BROWSER_ANIMATION_CONTINUITY_PASS planted='+test+' mirrored_flight=2 rubble_wrap=1');
