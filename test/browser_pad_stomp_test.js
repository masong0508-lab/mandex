/* Actual browser walking solver: stomp-pad launches preserve the impact,
 * held stomp cannot cancel a fresh ascent, and ordinary controls agree with
 * the pre-fix solver. Effects are recorded; production collision/hit code runs. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const sharedStart=html.indexOf('function wBlocked('),stepStart=html.indexOf('function walkStep(');
const stepEnd=html.indexOf('/* Match the console sprite turnaround:',stepStart);
assert(sharedStart>=0&&stepStart>sharedStart&&stepEnd>stepStart,'production walking functions exist');
const shared=html.slice(sharedStart,stepStart),production=html.slice(stepStart,stepEnd);
const reference=String.raw`function walkStep(dt){const m=man;
  m.wt-=dt;if(m.wt<=0){popText(m.x,m.y-40,'TIME!','#ff6b6b',26);walkQuit();return}
  if(wBlocked(m.x,m.y)){let f=false;for(let u=2;u<=80&&!f;u+=2)for(const sg of[0,-1,1])if(!wBlocked(m.x+sg*u,m.y-u)){m.x+=sg*u;m.y-=u;f=true;break}}   // rubble fell on him: pop him out
  const L=KEYS.KeyA||KEYS.ArrowLeft||KEYS.TL,R=KEYS.KeyD||KEYS.ArrowRight||KEYS.TR,UP=KEYS.KeyW||KEYS.ArrowUp||KEYS.Space||KEYS.TJ,DN=KEYS.KeyS||KEYS.ArrowDown||KEYS.TS,PU=KEYS.KeyX||KEYS.KeyZ||KEYS.KeyJ||KEYS.KeyK||KEYS.Enter||KEYS.TP;
  const ax=(R?1:0)-(L?1:0);
  // Countersteering retraces the current rear turn instead of snapping to
  // the other side. Gameplay/punch facing still responds on this tick.
  if(ax&&ax!==m.face){m.turn=m.turn>0?12-m.turn:12;m.face=ax}
  if(m.turn>0)m.turn--;
  m.vx+=(ax*135-m.vx)*Math.min(1,dt*(m.gr?14:5));
  if(m.pCd>0)m.pCd-=dt;if(m.pT>0)m.pT-=dt;if(PU&&m.pCd<=0)walkPunch();
  if(UP&&m.gr&&!m.stomp){m.vy=-345;m.gr=false;SFX.pop();dust(m.x,m.y+WHH,4,8)}
  if(DN&&!m.gr&&!m.stomp){m.stomp=true;m.vy=Math.max(m.vy,720);SFX.whoosh()}
  const dx=m.vx*dt,nx=Math.max(1,Math.ceil(Math.abs(dx)/2));
  for(let i=0;i<nx;i++){const sx=dx/nx;
    if(!wBlocked(m.x+sx,m.y))m.x+=sx;
    else{let ok=false;if(!m.gr&&m.vy>-380)for(let u=1;u<=10;u++)if(!wBlocked(m.x+sx,m.y-u)){m.x+=sx;m.y-=u;if(m.vy>-120)m.vy=-120;ok=true;break}   // airborne next to a ledge: hop up onto it
      if(!ok&&m.gr)for(let u=1;u<=9;u++)if(!wBlocked(m.x+sx,m.y-u)){m.x+=sx;m.y-=u;ok=true;break}   // a stair is one cell (8px): he steps up it
      if(!ok){m.vx=0;break}}}
  m.x=clamp(m.x,WHW+2,WW-WHW-2);
  m.vy+=G*.9*dt;const dy=m.vy*dt,ny=Math.max(1,Math.ceil(Math.abs(dy)/2));
  for(let i=0;i<ny;i++){const sy=dy/ny;
    if(!wBlocked(m.x,m.y+sy))m.y+=sy;
    else{if(sy>0){if(m.stomp){walkSlam();if(!wBlocked(m.x,m.y+sy)){m.y+=sy;m.vy*=.8;continue}}
        if(m.vy>380){dust(m.x,m.y+WHH,6,10);SFX.thud&&SFX.thud(.5)}m.vy=0}else{m.vy=0;if(Math.abs(dy)>2)SFX.thud&&SFX.thud(.3)}
      break}}
  m.gr=wBlocked(m.x,m.y+1.5);if(m.gr)m.stomp=false;
  if(m.padCd>0)m.padCd-=dt;
  if(m.gr&&!(m.padCd>0)){const by=Math.floor((m.y+WHH+2)/V);for(const o of[-4,0,4]){const bx=Math.floor((m.x+o)/V);if(inb(bx,by)&&grid[idx(bx,by)]===RUBBER){m.vy=m.stomp?-540:-470;m.gr=false;m.stomp=false;m.padCd=.3;SFX.boing();popText(m.x,m.y-34,'BOING!','#ff7eb0',26);ring(m.x,m.y+WHH,40,'#ff9ec8');dust(m.x,m.y+WHH,5,8);break}}}
  if(m.gr&&Math.abs(m.vx)>60&&((time*9)|0)%2===0&&rnd()<.25)dust(m.x,m.y+WHH,1,6);
  for(const t of targets)if(t.alive&&Math.hypot(t.x-m.x,t.y-m.y)<WHH+t.r-4)killTarget(t)}   // bump into a landlord and he is done for`;

function make(source,initial={}){
  let seed=0x614ec137;
  const e={WHW:7,WHH:19,WALK_T:45,V:8,RC:4,COLS:120,ROWS:60,WW:960,G:900,
    RUBBER:7,STEEL:6,WOOD:1,KEYS:{},chunks:[],targets:[],cases:[],MAT:[],time:0,shake:0,
    grid:new Uint8Array(120*60),hp:new Float32Array(120*60),events:[],
    man:{mode:'walk',walked:true,face:1,x:164,y:320-19-.24,vy:64,vx:0,
      gr:false,wt:45,pT:0,pCd:0,padCd:0,turn:0,stomp:false,R:14,...initial.man}};
  e.clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
  e.idx=(x,y)=>y*e.COLS+x;
  e.inb=(x,y)=>x>=0&&y>=0&&x<e.COLS&&y<e.ROWS;
  e.solidAt=(x,y)=>y>=480||(e.inb(Math.floor(x/8),Math.floor(y/8))&&
    !!e.grid[e.idx(Math.floor(x/8),Math.floor(y/8))]);
  e.rnd=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296};
  const record=name=>(...args)=>e.events.push([name,...args]);
  for(const name of ['goo','ring','dust','popText','chip','spark','wobKick','wake','killTarget'])
    e[name]=record(name);
  e.SFX={};
  for(const name of ['splat','pop','crunch','whoosh','boom','thud','boing'])
    e.SFX[name]=record(name);
  e.damageCell=(x,y,damage)=>{
    if(!e.inb(x,y))return 0;
    const at=e.idx(x,y);
    if(!e.grid[at])return 0;
    const dealt=Math.min(e.hp[at],damage);
    e.hp[at]-=dealt;
    if(e.hp[at]<=0)e.grid[at]=0;
    e.events.push(['damageCell',x,y,dealt]);
    return dealt;
  };
  for(let x=17;x<=24;x++){e.grid[e.idx(x,40)]=initial.material||e.RUBBER;e.hp[e.idx(x,40)]=150}
  Object.assign(e.KEYS,initial.keys);
  const step=new Function('env','with(env){'+shared+source+'\nreturn walkStep;}')(e);
  return {e,tick(keys){
    if(keys!==undefined){for(const key of Object.keys(e.KEYS))delete e.KEYS[key];Object.assign(e.KEYS,keys)}
    const before=e.events.length;e.time+=1/60;step(1/60);
    return {man:{...e.man},events:e.events.slice(before)};
  },snapshot(){const {ledgeGrace,jumpHold,gait,landFx,landPower,...man}=e.man;return {man,grid:Array.from(e.grid),hp:Array.from(e.hp),
    events:e.events.slice(),shake:e.shake,time:e.time}}};
}
const count=(row,event)=>row.events.filter(e=>e[0]===event).length;
let checks=0,baselineFailures=0;
function verify(name,setup,check,mustFailBaseline=false){
  try{check(make(production,setup))}catch(error){error.message=name+': '+error.message;throw error}
  checks++;
  let failed=false;
  try{check(make(reference,setup))}catch(error){if(!mustFailBaseline)throw error;failed=true}
  if(mustFailBaseline){assert(failed,name+' must reproduce the original defect');baselineFailures++}
}
verify('ordinary pad landing',{},h=>{
  const r=h.tick();
  assert.strictEqual(count(r,'boing'),1);
  assert.strictEqual(r.man.vy,-470,'ordinary bounce strength remains unchanged');
  assert(!r.man.gr&&!r.man.stomp);
  assert.strictEqual(r.man.padCd,.3);
});
for(const pressedNow of [false,true]){
  verify(pressedNow?'same-tick stomp landing':'existing stomp landing',
    {man:{stomp:!pressedNow,vy:pressedNow?64:720},keys:{KeyS:true}},h=>{
      const r=h.tick();
      assert.strictEqual(count(r,'boing'),1,'one pad launch per impact');
      assert.strictEqual(r.events.filter(e=>e[0]==='popText'&&e[3]==='BELLY SLAM!').length,1);
      assert.strictEqual(r.man.vy,-540,'a real stomp impact earns the stronger bounce');
      assert(!r.man.gr&&!r.man.stomp,'launch completes the impact pose');
      assert.strictEqual(r.man.padCd,.3);
      for(let x=17;x<=24;x++){
        assert.strictEqual(h.e.grid[h.e.idx(x,40)],h.e.RUBBER);
        assert.strictEqual(h.e.hp[h.e.idx(x,40)],150,'rubber pads withstand the stomp');
      }
    },true);
}
verify('held stomp preserves fresh ascent',{keys:{KeyS:true}},h=>{
  h.tick();
  const launchY=h.e.man.y;
  for(let i=0;i<8;i++){
    const r=h.tick();
    assert(r.man.vy<0&&!r.man.stomp,'held input cancelled the pad launch');
    assert(r.man.y<launchY,'the bounce must actually carry the player upward');
    assert.strictEqual(count(r,'boing'),0,'no repeated launch in midair');
    assert.strictEqual(r.events.filter(e=>e[0]==='popText'&&e[3]==='BELLY SLAM!').length,0);
  }
},true);
verify('final protected cooldown tick',
  {man:{y:160,vy:-320,padCd:.005},keys:{KeyS:true}},h=>{
    const r=h.tick();
    assert(r.man.vy<0&&!r.man.stomp&&r.man.padCd<=0);
  },true);
verify('stomp rearms after cooldown',
  {man:{y:160,vy:-320,padCd:.005},keys:{KeyS:true}},h=>{
    h.tick();const r=h.tick();
    assert(r.man.stomp&&r.man.vy>=720);
  });
for(const vy of [0,80]){
  verify(vy===0?'ceiling-stopped ascent rearms':'falling during cooldown rearms',
    {man:{y:160,vy,padCd:.2},keys:{KeyS:true}},h=>{
      const r=h.tick();assert(r.man.stomp&&r.man.vy>=720);
    });
}
verify('ordinary airborne stomp',
  {man:{y:160,vy:-320,padCd:0},keys:{KeyS:true}},h=>{
    const r=h.tick();assert(r.man.stomp&&r.man.vy>=720);
  });
verify('cooldown prevents a repeated landing launch',
  {man:{stomp:true,vy:720,padCd:.2}},h=>{
    const r=h.tick();
    assert.strictEqual(count(r,'boing'),0);
    assert(r.man.gr&&!r.man.stomp&&r.man.vy===0);
  });
verify('ordinary hard-floor slam',
  {material:6,man:{stomp:true,vy:720}},h=>{
    const r=h.tick();
    assert.strictEqual(count(r,'boing'),0);
    assert(r.man.gr&&!r.man.stomp&&r.man.vy===0);
    assert(h.e.hp[h.e.idx(20,40)]<150,'ordinary slam still damages a non-pad floor');
  });

// Full state/effect parity for ordinary walking, punching and pad launches
// without jump input. Short hops and coyote timing intentionally differ and
// are checked against their own behavioral references in the jump tests.
let seed=0x31d46bf8;
const rnd=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296};
let ordinaryScenes=0;
for(let scene=0;scene<96;scene++){
  const setup={material:[7,6,1][scene%3],man:{x:140+rnd()*45,y:270+rnd()*30,
    vx:(rnd()-.5)*90,vy:rnd()*200,padCd:scene%4===0?.2:0}};
  const current=make(production,setup),old=make(reference,setup);
  for(let frame=0;frame<80;frame++){
    const keys={KeyA:frame%70>=35,KeyD:frame%70<35,
      KeyX:frame%23<3};
    current.tick(keys);old.tick(keys);
  }
  assert.deepStrictEqual(current.snapshot(),old.snapshot(),'ordinary controls changed in scene '+scene);
  ordinaryScenes++;
}
console.log('WEB_PAD_STOMP_PASS checks='+checks+' original_failures='+baselineFailures+
  ' ordinary_scenes='+ordinaryScenes+' ticks_per_scene=80 held_ascent=1 rearm=1');
