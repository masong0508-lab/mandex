/* Execute production attacks and AI: one hit per recovery window, and a
 * stunned opponent must lose its queued attack rather than hit on recovery. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const source=fs.readFileSync(process.env.MANDEX_WEB_SOURCE||path.join(__dirname,'../web/index.html'),'utf8');
function section(first,last){
  const a=source.indexOf(first),b=source.indexOf(last,a+first.length);
  assert(a>=0&&b>a,'production source range '+first);
  return source.slice(a,b);
}
const code=section('function solidAt(', 'function updCases(')+
  section('function wBlocked(', '/* Match the console sprite turnaround:')+section('function walkAnimTick(', 'function walkFootSample(');
const setup=String.raw`
const V=8,RC=4,WHW=7,WHH=19,WALK_T=45,G=900,WW=960,ROWS=60,COLS=120,RUBBER=7;
const SX=128,SY=362,KEYS={},chunks=[],targets=[],cases=[],events=[];
const grid=new Uint8Array(COLS*ROWS),idx=(x,y)=>y*COLS+x;
const inb=(x,y)=>x>=0&&x<COLS&&y>=0&&y<ROWS;
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v)),rnd=()=>.5;
let state='play',fight=true,time=0,shake=0,score=0,slow=0,dragging=false;
let combo=0,comboLeft=0,comboPeak=0,bankLeft=0;
const OPT=()=>({fight});
const man={mode:'walk',x:200,y:461,R:14,vx:0,vy:0,face:1,gr:true,
  wt:45,pT:0,pCd:0,padCd:0,stomp:false,turn:0,hurtCd:0,stun:0};
const SFX=new Proxy({}, {get:(_,name)=>(...args)=>events.push(['sound',name,...args])});
const spark=(...a)=>events.push(['spark',...a]),dust=(...a)=>events.push(['dust',...a]);
const ring=(...a)=>events.push(['ring',...a]),popText=(...a)=>events.push(['text',...a]);
const chip=(...a)=>events.push(['chip',...a]),P=a=>events.push(['particle',a.k]);
const onLandlord=()=>events.push(['evicted']);
const damageCell=()=>0,wobKick=()=>{},wake=()=>{},goo=()=>{};
`;
const expose=String.raw`
return {man,KEYS,events,cases,targets,kill:killTarget,ai:lordAI,walk:walkStep,
  fight:v=>{fight=v},state:()=>({score,combo,comboLeft,comboPeak,bankLeft,slow,shake}),
  target(o={}){
    const t=Object.assign({x:220,y:461,r:7,alive:true,kind:'walker',hp:2,hpMax:2,
      inv:0,stun:0,armor:0,wind:0,swing:0,pan:0,blink:1,say:0,t:0,
      face:-1,cd:2,dodge:0,dd:1,range:40,hx:220,wk:0,mv:0,cover:null,
      ckT:0,eng:1,b2:0,heal:0,collapseRolled:0,mid:man},o);
    targets.push(t);return t;
  }};
`;
const make=()=>new Function(setup+code+expose)();
const DT=1/60;
let checks=0,failed=0;
function check(name,run){
  checks++;
  try{run()}catch(e){failed++;console.error('FAIL '+name+': '+e.message.split('\n')[0])}
}
check('real punch plus overlapping body contact counts as one hit',()=>{
  const s=make(),t=s.target();s.KEYS.KeyX=true;s.walk(DT);
  assert(t.alive&&t.hp===1&&t.inv>0,'second contact consumed the final hit point');
  assert.strictEqual(s.state().score,0,'nonfatal punch awarded an eviction');
  assert(s.man.wt<45,'nonfatal punch awarded time');
  assert.strictEqual(s.events.filter(e=>e[0]==='evicted').length,0);
});
check('eight rapid flight contacts cannot spend final HP',()=>{
  const s=make(),t=s.target();s.man.mode='fly';
  for(let i=0;i<8;i++)s.kill(t,false,true);
  assert(t.alive&&t.hp===1&&t.inv===.7,'flight substeps bypassed recovery');
  assert.strictEqual(s.state().score,0);
});
check('an already wounded enemy keeps its remaining immunity',()=>{
  const s=make(),t=s.target({hp:1,inv:.35});
  s.kill(t);assert(t.alive&&t.hp===1&&t.inv===.35);
  assert.strictEqual(s.events.length,0,'blocked hit emitted damage/eviction feedback');
});
check('three-HP enemy requires three separated ordinary hits',()=>{
  const s=make(),t=s.target({hp:3,hpMax:3});
  s.kill(t);assert.strictEqual(t.hp,2);
  s.kill(t);assert.strictEqual(t.hp,2);
  t.inv=0;s.kill(t);assert(t.alive&&t.hp===1);
  s.kill(t);assert(t.alive&&t.hp===1,'final-HP recovery was bypassed');
  t.inv=0;s.kill(t);assert(!t.alive);
  assert.strictEqual(s.state().score,5000);
  assert.strictEqual(s.man.wt,48);
  s.kill(t);assert.strictEqual(s.state().score,5000,'dead target scored again');
});
check('ordinary single-HP eviction remains immediate',()=>{
  const s=make(),t=s.target({hp:1,hpMax:1});s.kill(t);
  assert(!t.alive&&s.state().score===5000&&s.man.wt===48);
});
check('environmental stagger does not shield ordinary single-HP enemies',()=>{
  const s=make(),t=s.target({hp:1,hpMax:1,inv:.8,stun:.9});s.kill(t);
  assert(!t.alive&&s.state().score===5000&&s.man.wt===48);
});
for(const hp of [1,2,3])check('hard destruction bypasses recovery at HP '+hp,()=>{
  const s=make(),t=s.target({hp,hpMax:3,inv:.7,stun:.6});
  s.kill(t,true);assert(!t.alive&&s.state().score===5000);
});
check('counterpunch interrupts a queued melee attack',()=>{
  const s=make(),t=s.target({hp:3,hpMax:3,wind:DT/2});
  s.KEYS.KeyX=true;s.walk(DT);const afterPunch=s.man.wt;
  s.ai(t,DT);
  assert(t.stun>0&&t.hp===2&&t.alive);
  assert.strictEqual(t.wind,0,'stunned enemy retained its queued swing');
  assert.strictEqual(s.man.wt,afterPunch,'enemy struck through its hit stun');
  assert.strictEqual(s.man.hurtCd,0);
});
for(const kind of ['walker','shield','sprinter'])check('stunned '+kind+' cancels windup',()=>{
  const s=make(),t=s.target({kind,wind:.5,stun:.6,mv:2});
  s.ai(t,DT);
  assert.strictEqual(t.wind,0);assert.strictEqual(t.mv,0);
  assert.strictEqual(t.cd,2,'cooldown progressed while stunned');
  assert.strictEqual(s.man.wt,45);
});
for(const kind of ['thrower','boss'])check('stunned '+kind+' cannot throw',()=>{
  const s=make();s.man.mode='aim';s.man.x=200;s.man.y=300;
  const t=s.target({kind,x:400,y:300,wind:DT/2,stun:.6,b2:1});
  s.ai(t,DT);
  assert.strictEqual(s.cases.length,0,'stunned enemy released a projectile');
  assert.strictEqual(t.wind,0);assert.strictEqual(t.b2,0);
  assert.strictEqual(t.cd,2);
});
check('expiry tick cannot discharge an old melee windup',()=>{
  const s=make(),t=s.target({stun:DT/2,wind:DT/2,cd:2});
  s.ai(t,DT);
  assert(t.stun<=0);assert.strictEqual(t.wind,0);
  assert.strictEqual(s.man.wt,45,'old attack fired on the recovery tick');
});
check('stunned healer pauses, then recovers normally',()=>{
  const s=make(),h=s.target({kind:'healer',x:260,stun:.6,heal:0});
  const ally=s.target({x:280,hp:1,hpMax:3});s.ai(h,DT);
  assert.strictEqual(ally.hp,1,'stunned healer healed an ally');
  assert.strictEqual(h.heal,0);
  h.stun=DT/2;s.ai(h,DT);
  assert.strictEqual(ally.hp,2);assert.strictEqual(h.heal,3.5);
});
check('fight-off stun cannot preserve a surprise attack',()=>{
  const s=make(),t=s.target({stun:.6,wind:DT/2,b2:1});s.fight(false);
  s.ai(t,DT);assert.strictEqual(t.wind,0);assert.strictEqual(t.b2,0);
  s.fight(true);t.stun=0;s.ai(t,DT);assert.strictEqual(s.man.wt,45);
});
check('unstunned melee still damages and knocks back',()=>{
  const s=make(),t=s.target({wind:DT/2});s.ai(t,DT);
  assert.strictEqual(s.man.wt,42);assert(s.man.hurtCd>0&&s.man.vy<0);
});
check('unstunned thrower still launches a real solved arc',()=>{
  const s=make();s.man.mode='aim';s.man.x=200;s.man.y=300;
  const t=s.target({kind:'thrower',x:400,y:300,wind:DT/2});s.ai(t,DT);
  assert.strictEqual(s.cases.length,1);
  assert(s.cases[0].vx<0&&Number.isFinite(s.cases[0].vy));
});
check('walking thrower telegraphs, then attacks at normal cadence',()=>{
  const s=make();s.man.mode='walk';s.man.x=200;s.man.y=300;
  const t=s.target({kind:'thrower',x:400,y:300,cd:0,wind:0});
  s.ai(t,DT);
  assert(t.wind>0&&s.cases.length===0,'walking thrower skipped its windup');
  for(let i=0;i<60&&s.cases.length===0;i++)s.ai(t,DT);
  assert.strictEqual(s.cases.length,1,'walking thrower never fired at nearby walker');
  assert(t.cd>0,'thrower lost its normal cooldown');
});
check('walking thrower cannot snipe across the map',()=>{
  const s=make();s.man.mode='walk';s.man.x=70;s.man.y=300;
  const t=s.target({kind:'thrower',x:400,y:300,cd:0,wind:0});
  s.ai(t,DT);
  assert(t.wind===0&&s.cases.length===0,'out-of-range walking thrower attacked');
});
check('walking thrower cannot shoot across distant floors',()=>{
  const s=make();s.man.mode='walk';s.man.x=200;s.man.y=100;
  const t=s.target({kind:'thrower',x:400,y:300,cd:0,wind:0});
  s.ai(t,DT);assert.strictEqual(t.wind,0);
});
check('fight-back-off disables walking throwers',()=>{
  const s=make();s.man.mode='walk';s.man.x=200;s.man.y=300;
  const t=s.target({kind:'thrower',x:400,y:300,cd:0,wind:0});
  s.fight(false);s.ai(t,DT);assert(t.wind===0&&s.cases.length===0);
});
// The console MxCase pool is exactly four slots. Browser must cap the
// corresponding dynamic array without skipping the original windup/cooldown.
check('four active briefcases block extra browser projectiles',()=>{
  const s=make();s.man.mode='aim';s.man.x=200;s.man.y=300;
  const t=s.target({kind:'thrower',x:400,y:300,wind:DT/2,cd:0});
  for(let i=0;i<4;i++)s.cases.push({x:i,y:40,vx:i+1,vy:-i,l:4});
  const old=JSON.stringify(s.cases);
  s.ai(t,DT);
  assert.strictEqual(s.cases.length,4,'browser exceeded native four-case budget');
  assert.strictEqual(JSON.stringify(s.cases),old,'full pool destroyed active cases');
  assert(t.swing>0&&t.cd>0,'failed spawn must still complete windup and cooldown');
  assert.strictEqual(s.events.filter(e=>e[0]==='sound'&&e[1]==='whoosh').length,0,
    'a suppressed throw must not play an imaginary projectile sound');
});
check('a newly available case slot can be reused',()=>{
  const s=make();s.man.mode='aim';s.man.x=200;s.man.y=300;
  const t=s.target({kind:'thrower',x:400,y:300,wind:DT/2,cd:0});
  for(let i=0;i<3;i++)s.cases.push({x:i,y:i,vx:1,vy:1,l:4});
  const old=JSON.stringify(s.cases);
  s.ai(t,DT);
  assert.strictEqual(s.cases.length,4,'a vacant slot should admit one real throw');
  assert.strictEqual(JSON.stringify(s.cases.slice(0,3)),old,'existing cases were overwritten');
  assert(s.cases[3].vx<0&&Number.isFinite(s.cases[3].vy),
    'the admitted throw needs the original solved trajectory');
  assert.strictEqual(s.events.filter(e=>e[0]==='sound'&&e[1]==='whoosh').length,1,
    'a real throw must retain its sound cue');
});
check('boss retains two-windup cadence under a saturated case pool',()=>{
  const s=make();s.man.mode='aim';s.man.x=200;s.man.y=300;
  const t=s.target({kind:'boss',x:400,y:300,wind:DT/2,b2:0,cd:0});
  for(let i=0;i<4;i++)s.cases.push({x:i,y:i,vx:1,vy:1,l:4});
  s.ai(t,DT);
  assert.strictEqual(s.cases.length,4,'boss exceeded projectile capacity');
  assert(t.b2===1&&t.wind>0,'boss lost its original second-throw telegraph');
  s.cases.shift();t.wind=DT/2;
  s.ai(t,DT);
  assert.strictEqual(s.cases.length,4,'boss failed to reuse a slot');
  assert(t.b2===0&&t.cd>=3,'boss must end follow-up with ordinary cooldown');
});
check('many throwers cannot accumulate an unlimited projectile storm',()=>{
  const s=make();s.man.mode='aim';s.man.x=200;s.man.y=300;
  for(let i=0;i<24;i++){
    const t=s.target({kind:'thrower',x:400+(i%3)*10,y:300,
      wind:DT/2,cd:0});
    s.ai(t,DT);
  }
  assert.strictEqual(s.cases.length,4,'24 throwers overflowed the handheld pool');
  assert.strictEqual(s.events.filter(e=>e[0]==='sound'&&e[1]==='whoosh').length,4,
    'only actual admitted projectiles should play their launch sound');
});

console.log('WEB_COMBAT_RECOVERY_'+(failed?'FAIL':'PASS')+' cases='+checks+' failures='+failed);
if(failed)process.exitCode=1;
