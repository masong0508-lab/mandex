'use strict';
const fs=require('fs'),assert=require('assert');
const s=fs.readFileSync(require('path').join(__dirname,'../web/index.html'),'utf8');
function extract(a,b){let x=s.indexOf(a),y=s.indexOf(b,x+a.length);assert(x>=0&&y>x);return s.slice(x,y)}
const src=extract('const WEB_PROFILES=','function chip(')+extract('const WEB_SIM_DT=','let last=performance.now()');
const h=new Function(src+'return {webProfile,webRenderCap,webRenderScale,webFxBudget,webLightingDetail,webPerfSample,webEffectiveTier,webSimSteps,cycleWebProfile,cycleWebFX,webFXLabel};')();
for(const p of ['quality','balanced','low']){const o={performance:p};assert.equal(h.webRenderCap(o),p==='quality'?60:30);assert.equal(h.webRenderScale(o),p==='quality'?1:p==='balanced'?.75:.6);assert.equal(h.webFxBudget(o),p==='quality'?1400:p==='balanced'?360:220);}
const auto={};for(let i=0;i<20;i++)h.webPerfSample(auto,.05,30,true);assert.equal(h.webEffectiveTier(auto),1);for(let i=0;i<20;i++)h.webPerfSample(auto,.05,30,true);assert.equal(h.webEffectiveTier(auto),2);
for(let i=0;i<400;i++)h.webPerfSample(auto,.017,5,true);assert.equal(h.webEffectiveTier(auto),1);
for(let i=0;i<400;i++)h.webPerfSample(auto,.017,5,true);assert.equal(h.webEffectiveTier(auto),0);
let total=0;for(let i=0;i<120;i++)total+=h.webSimSteps(1/120);assert.equal(total,60);assert(h.webSimSteps(10)<=4);
const o={performance:'low',renderCap:60,renderScale:1,ecoFX:false,lighting:'full'};assert.equal(h.webRenderCap(o),60);assert.equal(h.webRenderScale(o),1);assert.equal(h.webFxBudget(o),1400);assert(h.webLightingDetail(o));
assert(s.includes('if(state===\'options\'){drawSettings()'));console.log('BROWSER_OPTIONS_PARITY_PASS profiles, hysteresis, overrides, fixed-step simulation');

/* Exact chapter gating, persisted option, and real art asset builders. */
const artSource=extract('function artUnlocked(){','// the world is normally');
let commands=0,canvases=[];
const gradient={addColorStop(){commands++}};
const canvasContext=()=>new Proxy({createLinearGradient:()=>gradient,createRadialGradient:()=>gradient},{
  get(t,k){return k in t?t[k]:(...args)=>{if(args.some(v=>typeof v==='number'&&!Number.isFinite(v)))throw Error('nonfinite artwork coordinate');commands++}},
  set(t,k,v){t[k]=v;return true}
});
const makeCanvas=(w,h)=>{const c={width:w,height:h,getContext:()=>canvasContext()};canvases.push(c);return c};
const art=new Function('makeCanvas',`
let PD={stars:{},opt:{}},CH1=20,SPR=[],bgA,bgB,WW=960,WH=540,GY=480,V=8,CL={},_vig=null,outfitCur={},time=0;
let rebuilds=0,notices=[];
const OPT=()=>PD.opt;
const storyDone=()=>{let n=0;for(let i=0;i<CH1;i++)if((PD.stars[i]||0)>0)n++;return n};
const toast=(...v)=>notices.push(v),applyOutfit=()=>{rebuilds++};
const document={createElement:()=>makeCanvas(0,0)};
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
const hsh=(x,y)=>((x*374761393+y*668265263)>>>0);
const MAT=Array.from({length:9},(_,i)=>({c:'#bca085'}));
function buildBG(){if(celArt())buildCelBackground()}
`+artSource+`
return {PD,notices,celArt,artUnlocked,cycleArtStyle,buildSprites,buildCelBackground,
  setWidth:w=>WW=w,atlas:()=>SPR,background:()=>[bgA,bgB],rebuilds:()=>rebuilds};
`)(makeCanvas);
assert(!art.artUnlocked()&&!art.cycleArtStyle());
for(let i=0;i<19;i++)art.PD.stars[i]=1;
art.PD.stars[599]=3;art.PD.unlockAll=true;art.PD.opt.artStyle='golden-cels';
assert(!art.celArt(),'trial or cheat cannot unlock art mode');
art.PD.opt.artStyle='classic';art.PD.stars[19]=1;
assert(art.artUnlocked()&&art.cycleArtStyle()&&art.celArt());
assert(art.atlas().slice(1).every(m=>m.every(v=>v.every(c=>c.width===24&&c.height===24))));
assert(art.background().every(c=>c.width===960&&c.height===540));
art.setWidth(2880);art.buildCelBackground();
assert(art.background().every(c=>c.width===2880&&c.height===540));
const saved=JSON.parse(JSON.stringify(art.PD));assert.equal(saved.opt.artStyle,'golden-cels');
assert(art.cycleArtStyle()&&!art.celArt()&&art.rebuilds()===2);
assert(art.atlas().slice(1).every(m=>m.every(v=>v.every(c=>c.width===8&&c.height===8))));
assert(commands>1000&&s.includes("delete OPT().artStyle;refreshArt()"));
console.log('GOLDEN_CELS_PASS chapter_lock, trial_exclusion, saved_toggle, 96 painted tile variants, wide_background, classic_restore');
