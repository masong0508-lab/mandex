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
