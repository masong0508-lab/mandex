/* Actual production source: refresh adaptation, viewport bounds and
 * exact conservative fracture thresholds. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const cut=(a,b)=>{const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,a);return html.slice(i,j)};
const make=new Function(cut('const WEB_PROFILES=','function chip(')+';return {sample:webPerfSample,tier:webEffectiveTier,reset:resetWebPerfSample};');
const auto={};
function feed(h,hz,seconds,work,o=auto){
  for(let i=0;i<Math.round(hz*seconds);i++)h.sample(o,1/hz,work(i),true);
}
const a=make();
feed(a,120,1,()=>10);assert.strictEqual(a.tier(auto),1,'120Hz overload detected');
feed(a,120,1,()=>10);assert.strictEqual(a.tier(auto),2);
feed(a,60,6.5,i=>i%2?3:16);assert.strictEqual(a.tier(auto),1,'capped render alternation recovers');
feed(a,30,6.5,()=>8);assert.strictEqual(a.tier(auto),0,'30Hz display recovers');
const b=make();feed(b,60,1,()=>19);assert.strictEqual(b.tier(auto),1,'60Hz overload below old threshold');
feed(b,60,1,()=>19);assert.strictEqual(b.tier(auto),2);
feed(b,60,3,()=>3);b.sample(auto,.5,1,true);feed(b,60,3.5,()=>3);assert.strictEqual(b.tier(auto),2,'suspend resets recovery');
feed(b,60,3,()=>3);assert.strictEqual(b.tier(auto),1,'recovery resumes');
const c=make();feed(c,60,5,()=>4);assert.strictEqual(c.tier(auto),0,'healthy 60Hz stays full quality');
const custom={performance:'quality'};feed(c,60,5,()=>30,custom);assert.strictEqual(c.tier(custom),0,'manual override');
const visible=new Function(cut('function worldVisualVisible(','function drawCases(')+';return worldVisualVisible;')();
let checks=0;
for(const z of [.55,1,1.8])for(const shake of [0,16,64])for(const [w,h] of [[540,960],[960,540]]){
  const x0=400.25-shake,x1=400.25+w/z+shake,y0=-80.5-shake,y1=-80.5+h/z+shake;
  for(const r of [14,128]){
    assert(visible(x0-r,y0,r,x0,x1,y0,y1),'left touching edge');
    assert(visible(x1+r,y1,r,x0,x1,y0,y1),'right touching edge');
    assert(visible(x0,y0-r,r,x0,x1,y0,y1),'top touching edge');
    assert(visible(x1,y1+r,r,x0,x1,y0,y1),'bottom touching edge');
    assert(!visible(x0-r-.001,y0,r,x0,x1,y0,y1),'left offscreen');
    assert(!visible(x1+r+.001,y1,r,x0,x1,y0,y1),'right offscreen');
    assert(!visible(x0,y0-r-.001,r,x0,x1,y0,y1),'top offscreen');
    assert(!visible(x1,y1+r+.001,r,x0,x1,y0,y1),'bottom offscreen');
    checks+=8;
  }
}
const projectile=new Function('cases','ctx',cut('function worldVisualVisible(','// =====================================================================\n//  DRAW')+';return drawCases;');
const cases=[{x:10,y:50,rot:.3},{x:114,y:50,rot:.8},{x:114.001,y:50,rot:1.1},{x:-30,y:200,rot:2}];
const drawn=[],ctx=new Proxy({translate(x,y){drawn.push([x,y])}},{get(t,k){return k in t?t[k]:()=>{}},set(t,k,v){t[k]=v;return true}});
const paint=projectile(cases,ctx);
paint(0,100,0,100);assert.deepStrictEqual(drawn,[[10,50],[114,50]],'grazing projectiles retain order');
drawn.length=0;paint();assert.strictEqual(drawn.length,cases.length,'no camera unchanged');
assert.strictEqual(cases.length,4,'no simulation mutation');
assert(html.includes('if(actorCull&&!worldVisualVisible(t.x+tx,t.y+ty,128'),'include wobble position');
assert(html.includes('drawHUD();drawObjectiveEdge();drawToasts(frameDt)'),'objective cue preserved');
const mat=cut('const MAT=[null,','const WOOD=');
const joint=cut('function jointBreakLimit(','// Rebuild a row-major');
const oracle=new Function(mat+'let hp=[0,0];'+joint+';return {MAT,min:WOB_MIN_JOINT,limit:(a,b,x,y)=>{hp[0]=x;hp[1]=y;return jointBreakLimit(0,1,a,b)}};')();
let n=0,skips=0;
for(let i=1;i<oracle.MAT.length;i++)for(let j=1;j<oracle.MAT.length;j++){
  for(const u of [0,.24999,.25,.25001,.49999,.5,.50001,1])for(const v of [0,.24999,.25,.25001,.49999,.5,.50001,1]){
    const lim=oracle.limit(i,j,oracle.MAT[i].hp*u,oracle.MAT[j].hp*v);
    for(const strain of [0,.04,.08,oracle.min,oracle.min+1e-7,lim-1e-7,lim,lim+1e-7,3,6,100]){
      assert.strictEqual(strain>oracle.min&&strain>lim,strain>lim,'exact joint fracture decision');
      if(strain<=oracle.min)skips++;
      n++;
    }
  }
}
new Function(cut('<script>','</script>').slice(8));
console.log('WEB_VIEWPORT_PERF_PASS edge_checks='+checks+' fracture_checks='+n+' early_rejections='+skips+' refresh_recovery=1 projectile_order=1');
