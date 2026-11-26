'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function get(a,b){const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,a);return html.slice(i,j)}
const src=get('function walkAnimTick(', 'function walkFootSample(')+get('const RAG_ST=', '// springs (cells)')+get('const sceneMotion=', 'const rubbleCache=')+get('function lordAnimationSample(', 'function drawCachedLord(');
const test=new Function('assert',`
let time=.2,G=900,targets=[],cases=[];const clamp=(v,a,b)=>Math.max(a,Math.min(b,v)),V=8,solidAt=()=>false;
`+src+`
let checks=0;
const ok=(v,msg)=>{assert(v,msg);checks++},near=(a,b,msg)=>ok(Math.abs(a-b)<1e-7,msg+' '+a+' / '+b);
const mul=(a,b)=>[a[0]*b[0]+a[1]*b[2],a[0]*b[1]+a[1]*b[3],a[2]*b[0]+a[3]*b[2],a[2]*b[1]+a[3]*b[3]],rot=a=>[Math.cos(a),-Math.sin(a),Math.sin(a),Math.cos(a)],scale=(x,y)=>[x,0,0,y];
for(const mode of ['fly','stuck'])for(const face of [-1,1])for(const dive of [false,true])for(const st of [0,1])for(const sq of [0,.2,1,9])for(const sa of [-2,-.4,0,.9,2.7]){
 const m={mode,face,dive,diveFace:face,ang:.63,spin:-1.4,t:.09,x:250,y:160,burpT:.6,jg:{sq,sa,st,fl:5}};
 const X=ragXf(m),q=jigSquashAmount(m.jg),S=mul(mul(rot(sa),scale(st?1+q:1-q,st?1-q*.7:1+q*.8)),rot(-sa));
 const B=mode==='stuck'?mul(mul(rot(X.a),S),scale(X.sx,X.sy)):mul(mul(S,rot(X.a)),scale(X.sx,X.sy));
 for(const p of [[0,0],[12,-8],[-31,22],[55,17]]){
  const a=rW(X,m,...p);near(a[0],m.x+B[0]*p[0]+B[1]*p[1],'paint x');near(a[1],m.y+B[2]*p[0]+B[3]*p[1],'paint y');
  const b=rL(X,m,...a);near(b[0],p[0],'local x');near(b[1],p[1],'local y');
 }
 const a=rW(X,m,12,-8),b=rW(X,m,55,17);near(ragBoneLength(X,43,25),Math.hypot(a[0]-b[0],a[1]-b[1]),'painted bone length');ok(Math.abs(X.det)>.08,'nondegenerate transform');
}
for(const face of [-1,1]){
 const m={mode:'fly',face,dive:false,ang:.35,x:300,y:180,jg:{sq:.8,sa:1.1,st:0,fl:.8}};
 ragStep(m,1/60);const rg=m.rg;ok(rg&&rg.P.length===2,'flight limbs');
 m.dive=true;m.diveFace=face;m.spin=m.ang*face;m.diveBlend=0;ragStep(m,1/60);ok(m.rg===rg,'dive retains live leg chains');
 let prior=flightTuck(m);
 for(let i=0;i<120;i++){m.spin+=face*.23;m.x+=2;m.y+=.4;m.diveBlend=Math.min(1,m.diveBlend+.04);time+=1/60;ragStep(m,1/60);
  const o={};ragOpts(o,m);ok(o.legs.flat().every(Number.isFinite),'finite articulated dive');ok(o.flightTuck>=prior-1e-8,'tuck progresses continuously');prior=o.flightTuck;
  const X=ragXf(m),rest=ragFl();for(let j=0;j<2;j++){const c=m.rg.P[j],r=rest[j],l=ragBoneLength(X,r[4]-r[2],r[5]-r[3]);ok(Math.abs(Math.hypot(c[2].x-c[1].x,c[2].y-c[1].y)-l)<l*.3,'limb length bounded');}
 }
}
const light={gr:true,animAir:1},heavy={gr:true,animAir:1};walkAnimTick(light,1/60,false,0,80);walkAnimTick(heavy,1/60,false,0,640);
near(light.animAir,0,'light landing planted');near(heavy.animAir,0,'hard landing planted');ok(heavy.animLand>light.animLand,'hard landing crouches further');
for(let i=0;i<20;i++)walkAnimTick(heavy,1/60,true,0);near(heavy.animLand,0,'landing fully recovers');
heavy.gr=false;walkAnimTick(heavy,1/60,true,0);ok(heavy.animAir>0,'jump blends in');
const lord={kind:'boss',t:1,wk:3,mv:2,wind:.4,b2:1,swing:.3,pan:.7,vy:0,blink:1};
let p=lordAnimationSample(lord,false);ok(p.wind&&!p.pan&&!p.swing,'queued throw windup beats panic');near(p.windAmount,0,'second throw starts loading');lord.wind=.2;p=lordAnimationSample(lord,false);near(p.windAmount,.5,'continuous windup');
lord.wind=0;p=lordAnimationSample(lord,true);ok(p.swing>0&&!p.pan,'attack recovery visible');
lord.swing=0;lord.pan=0;lord.mv=0;const keys=new Set();for(let i=0;i<600;i++){lord.t=i/60;keys.add(lordAnimationSample(lord,true).key)}ok(keys.size===6,'bounded idle cycle animates');lord.vy=500;ok(lordAnimationSample(lord,true).fall===1,'falling pose');
const a={alive:true,x:80,y:90},c={x:100,y:110,rot:.2};targets=[a];cases=[c];snapshotSceneActors();a.x+=4;a.y+=6;c.x+=10;c.rot+=.3;
let off=sceneRenderOffset(a,.5,24);near(off.x,-2,'actor interp x');near(off.y,-3,'actor interp y');near(a.x,84,'actor physics unchanged');off=sceneRenderOffset(c,.5,40);near(off.x,-5,'case interp x');near(off.rot,-.15,'case rotation');a.x+=80;near(sceneRenderOffset(a,0,24).x,0,'teleport snaps');near(sceneRenderOffset({},0,24).x,0,'new object snaps');
return checks;`)(assert);
new Function(html.slice(html.indexOf('<script>')+8,html.indexOf('</script>',html.indexOf('<script>'))));
console.log('MOTION_QUALITY_PASS checks='+test+' affine=1 dive=1 land=1 lord=1 interpolation=1 syntax=1');
