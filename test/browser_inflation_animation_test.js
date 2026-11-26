'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function get(a,b){const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,a);return html.slice(i,j)}
const src=get('const INFLATE_VISUAL_MAX=', 'const burpMult=');
const tests=new Function('assert',`
const clamp=(x,a,b)=>Math.max(a,Math.min(b,x)),stom=m=>(m.inf||0)+(m.bel||0),CT_BK=.55,CT_TB=1.35;
`+src+`
let checks=0;
const ok=(v,msg)=>{assert(v,msg);checks++};
let prior=0;
for(let i=0;i<=2000;i++){
 const raw=i/100,target=inflationTarget(raw),g=frontInflationGeometry(target);
 ok(target>=prior&&target<=INFLATE_VISUAL_MAX,'monotonic bounded volume');
 ok(g.rx>=22&&g.ry>=17&&g.cy+g.ry<53,'body geometry above feet');
 prior=target;
}
for(const raw of [0,.01,.2,.55,.9,1,1.15,2.2,1e6,1e9,Infinity,NaN,-1]){
 const m={inf:raw,bel:0},a=inflationSample(m,0);
 ok(Number.isFinite(a.fill)&&a.fill>=0&&a.fill<=INFLATE_VISUAL_MAX,'finite visible fill');
}
for(const hz of [30,60,120]){
 const m={inf:0,bel:0},dt=1/hz;let last=0;
 for(let i=1;i<=3*hz;i++){
  m.inf=i*dt/3.6;const before=m.inf;inflationTick(m,dt);const a=inflationSample(m,i*dt);
  ok(m.inf===before,'presentation never writes gameplay fill');
  ok(Math.abs(a.fill-last)<.075,'continuous growth');
  ok(a.pressure>=0&&a.pressure<=1,'bounded pressure');
  last=a.fill;
 }
 for(let i=0;i<hz;i++)inflationTick(m,dt);
 ok(m.infPump<.01,'pump settles after release');
 m.bel=.3;for(let i=0;i<hz;i++)inflationTick(m,dt);
 const before=inflationSample(m,4).fill;
 for(let i=1;i<=hz;i++){m.bel=.3*(1-i/hz);inflationTick(m,dt)}
 ok(inflationSample(m,5).fill<before,'deflation follows real release');
}
for(const ph of [0,1,2,3,4,5]){
 const c={ph,t:ph===2?.7:1.55,keep:0},m={inf:.6,bel:.15,chug:c};
 const a=inflationSample(m,2),q=inflationDrinkPose(c,a,2);
 ok(q.inflate===a.fill&&q.arms.L.length===6&&q.arms.R.length===6,'one body volume across phases');
 ok(!!q.prop===(ph===2||ph===4),'bottle follows actual drinking phase');
 if(q.prop)ok(q.prop.h==='R'&&q.prop.level>=0&&q.prop.level<=1,'attached bottle');
}
return checks;`)(assert);
assert(html.includes("m.mode==='aim'&&(m.seated||m.vlv)"),'stable seat path');
assert(html.includes('drawSpin(c.th||0,o)'),'continuous soda turn');
assert(html.includes('inflationTick(man,dt)'),'visual sampler wired to fixed update');
new Function(html.slice(html.indexOf('<script>')+8,html.indexOf('</script>',html.indexOf('<script>'))));
console.log('INFLATION_ANIMATION_PASS checks='+tests+' fill_continuity=1 pressure_cap=1 deflation=1 pose_continuity=1');
