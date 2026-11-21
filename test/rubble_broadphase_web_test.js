/* Execute production browser target-rubble collisions with thousands of
 * boundaries, not a second hand-written collision routine. */
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const text=fs.readFileSync(require('path').join(__dirname,'../web/index.html'),'utf8');
const first=text.indexOf('function updTargets(dt){');
const last=text.indexOf('// =====================================================================\n//  UPDATE',first);
assert(first>=0&&last>first,'missing browser target update');
const src=text.slice(first,last);
const test=`
const G=0,RC=4;
let targets=[],chunks=[],shake=0,kills=0;
const solidAt=()=>false, lordAI=()=>{},collapseSurvive=()=>false;
const popText=()=>{},SFX={thud(){}};
`+src+`
function run(dx,dy,speed,dead=false){
  kills=0;
  const t={x:200,y:200,r:7,t:0,inv:0,stun:0,vy:0,vx:0,alive:true,collapseRolled:0};
  const ch={x:200,y:200,rad:75,vx:speed,vy:0,w:0,cells:[{wx:200+dx,wy:200+dy,dead}]};
  targets=[t];chunks=[ch];
  updTargets(0);
  return {dead:!t.alive,stunned:t.stun>0,kills};
}
let hits=0,misses=0;
for(let xi=-45;xi<=45;xi++){
  for(let yi=-45;yi<=45;yi++){
    const dx=xi/2,dy=yi/2,actual=run(dx,dy,200);
    const shouldDie=dx*dx+dy*dy<11*11;
    if(actual.dead!==shouldDie)throw Error('wrong browser voxel collision '+dx+','+dy);
    if(shouldDie)hits++;else misses++;
  }
}
if(run(0,0,200,true).dead)throw Error('dead chunk cell still causes an invisible hit');
if(run(0,0,150).dead)throw Error('150px/s incorrectly classified as lethal');
if(!run(0,0,150).stunned)throw Error('intermediate rubble speed did not stun');
if(run(0,0,70).stunned)throw Error('slow rubble unexpectedly stunned the landlord');
if(run(11,0,250).dead||run(0,-11,250).dead)throw Error('contact edge is exclusive');
if(!hits||!misses)throw Error('missing positive or negative collision fixtures');
'web rubble broadphase: '+(hits+misses)+' production collision cases, slow/stun/kill, no ghost hits PASSED';
`;
const outcome=vm.runInNewContext(test,{});
assert(outcome.includes('PASSED'));
console.log(outcome);
