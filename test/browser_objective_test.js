/* Mandex objective UI: compare browser behavior with portable C rules. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const web=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const begin=web.indexOf('const OBJECTIVE_UI=');
const end=web.indexOf('// =====================================================================',begin);
assert(begin>=0&&end>begin,'production objective helpers exist');
const impl=web.slice(begin,end);
function probe(targets,men,mode,x,state='play'){
  const funcs=new Function('targets','man','men','state','V',
    impl+'\nreturn {snapshot:objectiveSnapshot,label:objectiveLabel};')(
    targets,{mode,x},men,state,8);
  const snap=funcs.snapshot();
  return {phase:snap.phase,remaining:snap.remaining,total:snap.total,
    direction:snap.direction,label:funcs.label(snap)};
}
const three=[{alive:true,x:400},{alive:true,x:360},{alive:true,x:320}];
assert.deepStrictEqual(probe(three,3,'aim',100),
  {phase:'evict',remaining:3,total:3,direction:0,label:'EVICT LANDLORDS'});
three[0].alive=three[1].alive=false;
assert.strictEqual(probe(three,2,'walk',100).direction,1,'right-facing last target');
assert.strictEqual(probe(three,2,'walk',500).direction,-1,'left-facing last target');
assert.strictEqual(probe(three,2,'walk',320).direction,0,'aligned last target');
assert.strictEqual(probe(three,0,'none',-999).phase,'no_men','out-of-men hint');
three[2].alive=false;
assert.strictEqual(probe(three,0,'none',-999).phase,'secure',
  'last eviction waits for the real end-state transition');
assert.strictEqual(probe(three,0,'none',-999,'win').phase,'complete');
assert.strictEqual(probe(three,0,'none',-999,'lose').phase,'failed');
// Execute the real browser edge clamp in isolation; the production helper
// reuses a single record and never has to allocate a marker every frame.
const edge=new Function(impl+'\nreturn objectiveScreenEdge;')();
let cue=edge(100,240,360,640,138,56);
assert.strictEqual(cue.dir,0,'visible character below portrait HUD gets no edge cue');
cue=edge(-30,240,360,640,138,56);
assert.strictEqual(cue.dir,1);assert.strictEqual(cue.x,42);
cue=edge(390,240,360,640,138,56);
assert.strictEqual(cue.dir,2);assert.strictEqual(cue.x,318);
cue=edge(180,-40,360,640,138,56);
assert.strictEqual(cue.dir,3);assert.strictEqual(cue.y,153);
cue=edge(180,700,360,640,138,56);
assert.strictEqual(cue.dir,4);assert.strictEqual(cue.y,569);
assert.strictEqual(edge(-100,710,360,640,138,56).dir,1,
  'diagonal last target remains inside the safe inset');
assert.strictEqual(edge(80,80,30,240,138,56).dir,0,
  'unusable tiny viewport does not draw edge cue');
assert(web.includes('drawHUD();drawObjectiveEdge();drawToasts(frameDt)'),
  'draw the cue in screen coordinates above the action but below toasts');
assert(web.includes('const top=hudTop()+17;'),'edge cue follows actual HUD height');
assert(web.includes("if(o.phase!=='last'||!o.target)return;"),'avoid cues after clear');
assert(web.includes('objective=objectiveSnapshot(),lb=objective.remaining'),'HUD uses objective helper');
const hudStart=web.indexOf('function hudTop(){'),hudEnd=web.indexOf('function star(',hudStart);
const hudSource=web.slice(hudStart,hudEnd);
const paintHUD=new Function('W','H','fuses','combo',`
 const state='play',objectiveSnapshot=()=>({phase:'evict',remaining:3,total:3}),objectiveLabel=()=> 'EVICT LANDLORDS';
 const CL={name:'Long custom level name',custom:true},level=0,men=17,shownScore=999999999,man={mode:'aim'},testing=false;
 const tntLiveFuses=fuses,comboLeft=1,OPT=()=>({tool:'none'}),PWTOOLS=[],pw={sel:null,left:{}};
 const toolName=()=> 'NONE',backFromPlay=()=>{},openSettings=()=>{},shotReadiness=()=> 'DRAG BACK TO AIM',banner=0,CAM={on:false},dragging=false;
 const ctx={fillRect(){}},records=[],btn=(x,y,w,h)=>records.push({x,y,w,h}),uiText=(label,x,y,w,h)=>records.push({label,x,y,w,h});
 ${hudSource}
 drawHUD();return {records,top:hudTop()};
`);
for(const [w,h] of [[540,720],[540,960],[540,1440],[960,540]])for(const fuses of [0,12])for(const combo of [0,25]){
 const {records,top}=paintHUD(w,h,fuses,combo);
 for(const r of records)assert(r.x>=0&&r.y>=0&&r.x+r.w<=w&&r.y+r.h<=h,'HUD item inside viewport');
 const alerts=records.filter(r=>r.label&&(/TNT|streak|Goal:/.test(r.label)));
 for(let i=0;i<alerts.length;i++)for(let j=i+1;j<alerts.length;j++){const a=alerts[i],b=alerts[j];assert(a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y,'alerts do not overlap')}
 for(const r of alerts)assert(r.y+r.h<=top,'safe HUD inset includes every alert');
}
// Execute the actual walking HUD and its shared button layout. Canvas/text
// spies observe production coordinates; no copied layout formulas can make
// this pass while the painted timer or instructions still overlap the HUD.
const walkBegin=web.indexOf('const WALK_BUTTONS=');
const walkEnd=web.indexOf('function manStep(',walkBegin);
assert(walkBegin>=0&&walkEnd>walkBegin,'production walking HUD block exists');
const paintWalkHUD=new Function('W','H','portrait',`
  const KEYS={},man={mode:'walk',wt:22.5},state='play',WALK_T=45;
  const clamp=(v,a,b)=>Math.max(a,Math.min(b,v)),isPortrait=()=>portrait;
  const fills=[],buttons=[],texts=[];
  const ctx={fillRect(x,y,w,h){fills.push({x,y,w,h})},fill(){},stroke(){}};
  const rrect=(x,y,w,h)=>buttons.push({x,y,w,h});
  const uiText=(text,x,y,w,h)=>texts.push({text,x,y,w,h});
  ${web.slice(walkBegin,walkEnd)}
  drawWalkHUD();return {fills,buttons,texts};
`);
for(const [w,h,portrait] of [[540,720,true],[540,960,true],[540,1169,true],[540,1440,true],[960,540,false]]){
 const drawn=paintWalkHUD(w,h,portrait),timer=drawn.fills[0],label=drawn.texts.find(t=>t.text.startsWith('WALK TIME'));
 assert(timer&&label,'walking timer and explicit time label');
 assert.strictEqual(drawn.buttons.length,5,'all five controls');
 const top=Math.min(...drawn.buttons.map(b=>b.y));
 assert(timer.y>=164&&timer.y+timer.h<=top,'timer clear of HUD and controls');
 assert(label.y>=timer.y&&label.y+label.h<=timer.y+timer.h,'time label fits timer');
 for(const b of drawn.buttons)assert(b.x>=0&&b.y>=0&&b.x+b.w<=w&&b.y+b.h<=h,'walking button within canvas');
 for(let i=0;i<drawn.buttons.length;i++)for(let j=i+1;j<drawn.buttons.length;j++){const a=drawn.buttons[i],b=drawn.buttons[j];assert(a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y,'walking controls disjoint')}
}
assert(web.includes("const lastTargetVisible=state==='play'&&objectiveSnapshot().phase==='last';"),
  'a single surviving landlord is identified in the real world renderer');
assert(web.includes("if(lastTargetVisible && t.collapseRolled!==2)"),
  'rare escaped-collapse survivor gold marker supersedes the last-target locator');
assert(web.includes("txt('LAST',t.x,t.y-63,12"),
  'the locator follows the actual landlord coordinate, not an unrelated HUD point');
const start=web.indexOf('<script>'),stop=web.indexOf('</script>',start);
assert(start>=0&&stop>start);
new Function(web.slice(start+8,stop));
console.log('OBJECTIVE_WEB_PASS target_counts=1 last_directions=3 secure_delay=1 mobile_layout=1 walking_hud_heights=4');
