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
assert(web.includes('const top=W<700?138:76;'),
  'reserve portrait combo/mission HUD and desktop top bar');
assert(web.includes('if(o.phase!==\'last\'||!o.target)return;'),
  'avoid cues before the last landlord and after a cleared level');
assert(web.includes('const objective=objectiveSnapshot(),lb=objective.remaining;'),
  'production browser HUD uses objective helper, not old disconnected target counts');
assert(web.includes("const oy=P?66:38;"),'objective strip is positioned for portrait and landscape');
assert(web.includes("const cy=P?100:39;"),
  'portrait streak must start below the persistent mission tracker');
for(const width of [320,360,390,480,699,700,960]){
  const portrait=width<700,goalX=7,goalW=Math.min(304,width-14);
  const goalY=portrait?66:38,goalH=28;
  const streakX=width-170,streakY=portrait?100:39,streakW=158,streakH=30;
  const overlapX=Math.max(goalX,streakX)<Math.min(goalX+goalW,streakX+streakW);
  const overlapY=Math.max(goalY,streakY)<Math.min(goalY+goalH,streakY+streakH);
  assert(!(overlapX&&overlapY),'goal and streak overlap at '+width+'px viewport');
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
  const txt=(text,x,y,size)=>texts.push({text,x,y,size});
  ${web.slice(walkBegin,walkEnd)}
  drawWalkHUD();return {fills,buttons,texts};
`);
for(const height of [720,960,1169,1240]){
  const drawn=paintWalkHUD(540,height,true),timer=drawn.fills[0];
  const hint=drawn.texts.find(t=>t.text.startsWith('HOLD LEFT / RIGHT'));
  assert(timer&&hint,'portrait paints its walking timer and hint');
  assert.strictEqual(drawn.buttons.length,5,'actual walk HUD paints all five controls');
  // Reserve a full font height above the baseline, a descender below it,
  // and two pixels for the outlined text. Header/objective/streak end at134.
  const hintTop=hint.y-hint.size-2,hintBottom=hint.y+hint.size*.3+2;
  const buttonTop=Math.min(...drawn.buttons.map(b=>b.y));
  assert(timer.y>=134,'portrait walking timer overlaps header/objective/streak at '+height);
  assert(hintTop>=134,'portrait walking hint overlaps header/objective/streak at '+height);
  assert(timer.y+timer.h<=hintTop,'portrait walking timer overlaps its hint at '+height);
  assert(hintBottom<=buttonTop,'portrait walking hint overlaps touch controls at '+height);
  assert(timer.y+timer.h<=buttonTop,'portrait walking timer overlaps touch controls at '+height);
  for(const b of drawn.buttons){
    assert(b.x>=0&&b.y>=0&&b.x+b.w<=540&&b.y+b.h<=height,
      'portrait touch control escapes canvas at '+height);
  }
}
const landscapeWalk=paintWalkHUD(960,540,false);
assert.strictEqual(landscapeWalk.fills[0].y,40,'landscape walking timer retains its baseline');
assert.strictEqual(landscapeWalk.texts.find(t=>t.text.startsWith('A/D MOVE')).y,66,
  'landscape walking instructions retain their baseline');
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
