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
assert(web.includes('const objective=objectiveSnapshot(),lb=objective.remaining;'),
  'production browser HUD uses objective helper, not old disconnected target counts');
assert(web.includes("const oy=P?66:38;"),'objective strip is positioned for portrait and landscape');
const start=web.indexOf('<script>'),stop=web.indexOf('</script>',start);
assert(start>=0&&stop>start);
new Function(web.slice(start+8,stop));
console.log('OBJECTIVE_WEB_PASS target_counts=1 last_directions=3 secure_delay=1 mobile_layout=1');
