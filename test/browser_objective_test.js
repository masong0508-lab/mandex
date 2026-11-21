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
assert(web.includes("const lastTargetVisible=state==='play'&&objectiveSnapshot().phase==='last';"),
  'a single surviving landlord is identified in the real world renderer');
assert(web.includes("if(lastTargetVisible && t.collapseRolled!==2)"),
  'rare escaped-collapse survivor gold marker supersedes the last-target locator');
assert(web.includes("txt('LAST',t.x,t.y-63,12"),
  'the locator follows the actual landlord coordinate, not an unrelated HUD point');
const start=web.indexOf('<script>'),stop=web.indexOf('</script>',start);
assert(start>=0&&stop>start);
new Function(web.slice(start+8,stop));
console.log('OBJECTIVE_WEB_PASS target_counts=1 last_directions=3 secure_delay=1 mobile_layout=1');
