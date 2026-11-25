/* Direct shot selection and actual launch accounting. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function section(a,b){const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,'missing '+a);return html.slice(i,j)}
const tools=section('const TOOLS=[','function valveOpen()');
const launch=section('function launch(){','// ---- BURPBURSTER');
const picker=section('function drawPowerPicker(){','function star(');
const run=new Function('assert',tools+launch+picker+`
  let man,pw,men=3,state='play',dragging=false,SX=128,SY=362,hint=1,unlocked=false,stats={},o={tool:'turbo'},hits=[],W=540,H=720,portrait=true;
  let shots=0;const OPT=()=>o,ST=()=>stats,valveOpen=()=>unlocked,
    ring=()=>{},popText=()=>{},pdTouch=()=>{},onShot=()=>{shots++},SFX={pop(){},launch(){},wooo(){}},
    flightFaceFor=(v,f)=>v<0?-1:1,flightPitch=()=>0,burpMult=()=>1,
    pullVec=()=>[SX-man.x,SY-man.y],
    isPortrait=()=>portrait,ctx={fillRect(){}},txt=()=>{},
    btn=(x,y,w,h,label,fn)=>hits.push({x,y,w,h,label,fn});
  function fresh(){man={mode:'aim',x:SX,y:SY,inf:0,bel:0,stun:0,pwr:null,face:1};pw={tool:'none',sel:null,left:{beans:1,towel:1,splat:1,burp:1,turbo:1,quake:1,breaker:1}};}
  fresh();toolArm();assert(pw.sel==='turbo'&&pw.left.turbo===1,'saved default arms without spending');
  assert(selectShotTool('breaker')&&pw.sel==='breaker'&&o.tool==='turbo','manual selection is independent of preference');
  assert(selectShotTool('none')&&pw.sel===null,'normal shot disarms');
  assert(!selectShotTool('valve'),'locked valve cannot arm');
  unlocked=true;assert(selectShotTool('valve'),'unlocked valve selectable');
  man.seated=true;man.vlv=true;man.x=36;
  assert(selectShotTool('breaker')&&!man.seated&&!man.vlv&&man.x===SX,'switch away from valve restores sling');
  man.x=SX-5;launch();assert(pw.left.breaker===1&&pw.sel==='breaker'&&shots===0,'short pull free');
  man.x=SX-70;launch();assert(pw.left.breaker===0&&man.pwr==='breaker'&&pw.sel===null&&men===2&&shots===1,'valid launch spends once');
  launch();assert(shots===1&&men===2,'duplicate launch rejected');
  man.mode='aim';assert(!selectShotTool('breaker'),'used power unavailable');
  pw.sel='breaker';man.x=SX-70;launch();
  assert(man.pwr===null&&pw.left.breaker===0&&shots===2,'stale selection cannot spend negative stock');
  fresh();assert(selectShotTool('burp')&&pw.sel===null,'soda requires chug');
  pw.sel='burp';man.x=SX-70;launch();assert(pw.left.burp===0&&man.pwr==='burp','charged soda valid launch');
  fresh();man.stun=1;assert(!selectShotTool('turbo'),'stunned selection blocked');man.stun=0;
  for(const dims of [[540,720],[540,960],[540,1240],[960,540]]){
    W=dims[0];H=dims[1];portrait=W===540;drawPowerPicker();
    assert(hits.length===10,'nine choices and close');
    for(let i=0;i<hits.length;i++){
      const a=hits[i];assert(a.w>=40&&a.h>=35&&a.x>=0&&a.y>=0&&a.x+a.w<=W&&a.y+a.h<=H,'picker hitbox outside canvas');
      for(let j=0;j<i;j++){const b=hits[j];assert(a.x>=b.x+b.w||b.x>=a.x+a.w||a.y>=b.y+b.h||b.y>=a.y+a.h,'picker hitboxes overlap')}
    }
  }
  return true;
`);
assert(run(assert),'power picker and launch');
console.log('BROWSER_POWER_PICKER_PASS stock=guarded preference=independent layout_cases=4');
