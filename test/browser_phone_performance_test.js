/* Run actual inline production functions; no DOM/browser dependency. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function section(a,b){const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,'missing '+a);return html.slice(i,j)}
const perf=section('const WEB_PROFILES=','function chip(');
const scheduler=section('function webNextPaint(','document.addEventListener(\'visibilitychange\',()=>{\n  last=');
const fixed=section('const WEB_SIM_DT=','let last=performance.now()');
const controls=section('function P(o){','function sweat(');
const f=new Function(perf+scheduler+fixed+controls+`
  let parts=[];
  return {webProfile,webEffectiveTier,webRenderScale,webRenderCap,webFxBudget,webLightingDetail,cycleWebLighting,webPerfSample,cycleWebProfile,cycleWebCap,cycleWebScale,cycleWebFX,
    next:webNextPaint,tick:webSimSteps,resetSim:()=>{webSimCarry=0},
    tier:()=>webPerfTier,partCap:n=>{webFxCap=n;parts=[]},parts:()=>parts,particle:P};
`)(),o={};
assert(f.webRenderScale(o)===1&&f.webRenderCap(o)===60&&f.webFxBudget(o)===1400,'quality starting tier');
for(let i=0;i<20;i++)f.webPerfSample(o,.05,32,true);
assert(f.tier()===1,'sustained overload reduces tier');
for(let i=0;i<20;i++)f.webPerfSample(o,.05,32,true);
assert(f.tier()===2&&f.webRenderScale(o)===.6&&f.webRenderCap(o)===30&&f.webFxBudget(o)===220,'second tier');
assert(!f.webLightingDetail(o),'low tier uses lite lighting');o.lighting='full';assert(f.webLightingDetail(o),'full lighting override');delete o.lighting;
for(let i=0;i<350;i++)f.webPerfSample(o,.016,5,true);
assert(f.tier()===2,'brief recovery does not raise quality');
for(let i=0;i<40;i++)f.webPerfSample(o,.016,5,true);
assert(f.tier()===1,'sustained recovery raises tier');
for(let i=0;i<400;i++)f.webPerfSample(o,.016,5,true);
assert(f.tier()===0,'healthy timing restores quality');
f.webPerfSample(o,.5,500,true);assert(f.tier()===0,'background stall alone cannot downgrade');
o.performance='low';assert(f.webRenderCap(o)===30&&f.webRenderScale(o)===.6,'manual low preset');
o.renderScale=1;o.renderCap=60;o.ecoFX=false;
assert(f.webRenderCap(o)===60&&f.webRenderScale(o)===1&&f.webFxBudget(o)===1400,'overrides win');
for(let i=0;i<90;i++)f.webPerfSample(o,.05,100,true);
assert(f.tier()===0,'manual profile does not adapt');
o.performance='corrupt';o.renderScale=99;o.renderCap=999;
assert(f.webProfile(o)==='auto'&&f.webRenderScale(o)===1&&f.webRenderCap(o)===60,'invalid saved values safe');
delete o.ecoFX;f.cycleWebFX(o);assert(o.ecoFX===true);f.cycleWebFX(o);assert(o.ecoFX===false);f.cycleWebFX(o);assert(o.ecoFX===undefined);
f.partCap(4);for(let i=0;i<4;i++)f.particle({k:1,id:i});f.particle({k:2});f.particle({k:5,id:9});
assert(f.parts().length===4&&f.parts().some(p=>p.id===9),'impact ring protected at cap');
let cadence=0;
for(const hz of [24,30,60,75,90,120,144])for(const cap of [30,60]){
  f.resetSim();let draws=0,sims=0,next=-Infinity;
  for(let i=1;i<=hz*10;i++){
    const t=i*1000/hz;sims+=f.tick(1/hz);
    if(t+1>=next){next=f.next(t,cap,next,draws===0);draws++}
  }
  assert(sims===600,'render pacing altered fixed update clock');
  assert(Math.abs(draws-10*Math.min(hz,cap))<=2,'render target incorrect: '+hz+'/'+cap+' '+draws);
  cadence++;
}
const layout=section('const CAM=','function camTick('),pointer=section('function pt(e){','function retry(');
const transform=section('function webScreenTransform(){','const toWorld=');
const scaled=new Function('assert',perf+layout+pointer+transform+`
  const cv={width:960,height:540,style:{},getBoundingClientRect(){return {left:12,top:20,width:270,height:480}}};
  const window={innerWidth:390,innerHeight:844,visualViewport:{width:390,height:844}};
  const document={body:{getBoundingClientRect:()=>({width:390,height:844})}};
  let o={performance:'low'},W=960,H=540,COLS=120,state='play',CL={cols:120},_vig=null,dragging=false,sliderDrag=false,E={},TID={},KEYS={},hits=[];
  const OPT=()=>o,clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
  let transformArgs;const ctx={setTransform(...a){transformArgs=a}};
  function setWorld(c){COLS=c}
  let checks=0;
  for(const scale of [.6,.75,1])for(const portrait of [true,false]){
    o.renderScale=scale;window.innerWidth=portrait?390:844;window.innerHeight=portrait?844:390;window.visualViewport.width=window.innerWidth;window.visualViewport.height=window.innerHeight;
    layoutTick();webScreenTransform();
    assert(cv.width===Math.round(W*scale)&&cv.height===Math.round(H*scale),'physical buffer scales');
    assert(W===(portrait?540:960),'logical layout unchanged');
    assert(JSON.stringify(pt({clientX:147,clientY:260}))===JSON.stringify([W/2,H/2]),'pointer maps at each scale');
    assert(transformArgs[0]===cv.width/W&&transformArgs[3]===cv.height/H,'screen transform');
    checks++;
  }
  return checks;
`)(assert);
const settings=section('const SETTINGS_TABS=','/* Portrait creator:');
const screens=new Function('assert',perf+settings+`
  let W=540,H=720,portrait=true,muted=false,armReset=0,armProg=0,frameMeterVisible=false;
  let o={},hits=[],drawn=[],saved=0,resetCount=0,eraseCount=0;
  const OPT=()=>o,isPortrait=()=>portrait,WEB_UI={page:{options:0}},performance={now:()=>100},
    SFX={pop(){}},pdTouch=()=>{},pdSave=()=>{saved++},ctx={fillRect(){}},
    btn=(x,y,w,h,label,fn)=>hits.push({x,y,w,h,label,fn}),txt=(s)=>drawn.push(s),
    SHAKES=[['OFF',0],['LOW',.5],['FULL',1]],GUIDES=['OFF','SHORT','FULL'],
    toolName=()=>'NONE',toolCycle=()=>{},goMenu=()=>{},optVol=()=>o.vol==null?1:o.vol,
    audioInit=()=>{},setMuted=v=>{muted=v},clamp=(x,a,b)=>Math.max(a,Math.min(b,x)),applyVol=()=>{},
    optsReset=()=>{resetCount++},progReset=()=>{eraseCount++},toast=()=>{};
  let cases=0;
  for(const dims of [[540,720],[540,960],[540,1240],[960,540]]){
    W=dims[0];H=dims[1];portrait=W===540;
    for(let page=0;page<4;page++){
      WEB_UI.page.options=page;hits=[];drawn=[];drawSettings();
      assert(hits.length>=7,'navigation and options exist');
      for(let i=0;i<hits.length;i++){
        const a=hits[i];assert(a.w>=40&&a.h>=35&&a.x>=0&&a.y>=0&&a.x+a.w<=W+.1&&a.y+a.h<=H+.1,'settings control clipped');
        for(let j=0;j<i;j++){const b=hits[j];assert(a.x>=b.x+b.w||b.x>=a.x+a.w||a.y>=b.y+b.h||b.y>=a.y+a.h,'settings buttons overlap')}
      }
      cases++;
    }
  }
  WEB_UI.page.options=1;hits=[];drawSettings();hits[5].fn();
  assert(o.performance==='quality'&&saved>0,'profile persists');
  hits=[];drawSettings();hits[6].fn();hits[7].fn();hits[8].fn();hits[10].fn();hits[11].fn();
  assert(o.renderCap===30&&o.renderScale===1&&o.ecoFX===true&&o.showPerf===true&&o.lighting==='full','manual settings wired');
  WEB_UI.page.options=3;hits=[];drawSettings();hits[5].fn();
  assert(resetCount===0&&eraseCount===0,'reset requires confirmation');
  hits=[];drawSettings();hits[5].fn();assert(resetCount===1&&eraseCount===0,'confirmed settings reset');
  hits=[];drawSettings();hits[6].fn();assert(eraseCount===0,'erase requires confirmation');
  hits=[];drawSettings();hits[6].fn();assert(eraseCount===1,'confirmed progress reset');
  return cases;
`)(assert);
console.log('WEB_PHONE_PERFORMANCE_PASS cadence='+cadence+' scale_orientation='+scaled+' settings_layouts='+screens+' adaptive_hysteresis=1 effect_budget=1');
