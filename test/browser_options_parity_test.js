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
