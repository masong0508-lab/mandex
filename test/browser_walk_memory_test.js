/* Check browser hot-path allocation reductions without changing hitboxes,
 * visible character detail, or touch-button positions. Runs under plain Node.
 * All production functions are extracted from the actual standalone HTML. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function between(first,last){
  const a=html.indexOf(first),b=html.indexOf(last,a+first.length);
  assert(a>=0&&b>a,'browser source missing '+first+' / '+last);
  return html.slice(a,b);
}
/* Independent geometric reference for the new walking collider:
   sample the character's original 3x5 probe points against each actual
   live voxel. A conservative broadphase may ignore impossible far chunks,
   but the chunk centroid is NOT the contact hitbox of a fallen wall. */
function oldCollision(chunks,solidAt,RC,WHW,WHH,x,y){
  const xs=[x-WHW,x,x+WHW],ys=[y-WHH,y-WHH*.5,y,y+WHH*.5,y+WHH-.01];
  for(const qx of xs)for(const qy of ys)if(solidAt(qx,qy))return true;
  for(const ch of chunks){
    const radius=ch.rad+RC;
    if(Math.abs(ch.x-x)>radius+WHW||Math.abs(ch.y-y)>radius+WHH)continue;
    for(const a of ch.cells){
      if(a.dead||Math.abs(a.wx-x)>=WHW+RC||Math.abs(a.wy-y)>=WHH+RC)continue;
      for(const qx of xs)for(const qy of ys)
        if(Math.abs(a.wx-qx)<RC&&Math.abs(a.wy-qy)<RC)return true;
    }
  }
  return false;
}
const collisionCode=between('function wBlocked(', 'function enterWalk(');
assert(!collisionCode.includes('const xs=['),'walk broadphase makes no per-call sample arrays');
assert(!collisionCode.includes('Math.hypot(ch.x-qx,ch.y-qy)<=radius'),
       'walk collider must use actual transformed debris rather than the centroid');
assert(collisionCode.includes('Math.abs(a.wx-x0)')&&collisionCode.includes('Math.abs(a.wx-x2)')&&
       collisionCode.includes('Math.abs(a.wy-y0)')&&collisionCode.includes('Math.abs(a.wy-y4)'),
       'voxel-based walking hitbox retains both axes and extreme sample points');
const makeFast=new Function('chunks','solidAt','RC','WHW','WHH','Math',collisionCode+'\nreturn wBlocked;');
let seed=0xc001d00d;
function rnd(){seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296;}
function rng(lo,hi){return lo+rnd()*(hi-lo);}
let hits=0,misses=0,hypotCalls=0,cases=0;
for(let scene=0;scene<12;scene++){
  const ch=[];
  for(let c=0;c<10;c++){
    const x=rng(-100,1100),y=rng(-80,570),rad=rng(4,120),cells=[];
    for(let k=0;k<75;k++){
      const on=rnd()<.9;
      cells.push({wx:on?x+rng(-rad,rad):rng(-300,1400),
                  wy:on?y+rng(-rad,rad):rng(-300,900),dead:rnd()<.2});
    }
    ch.push({x,y,rad,cells});
  }
  const grid=new Uint8Array(120*60);
  if(scene%3===0)for(let i=0;i<grid.length;i++)grid[i]=rnd()<.04?1:0;
  const solidAt=(x,y)=>y>=480 || (x>=0&&y>=0&&x<960&&y<480&&grid[Math.floor(y/8)*120+Math.floor(x/8)]>0);
  const countedMath=Object.create(Math);
  countedMath.hypot=(a,b)=>{hypotCalls++;return Math.hypot(a,b);};
  const fast=makeFast(ch,solidAt,4,7,19,countedMath);
  for(let i=0;i<340;i++){
    const x=i%3===0?ch[i%10].x+rng(-18,18):rng(-130,1200);
    const y=i%3===0?ch[i%10].y+rng(-25,25):rng(-80,590);
    const want=oldCollision(ch,solidAt,4,7,19,x,y);
    const got=fast(x,y);
    assert.strictEqual(got,want,'walk collision parity scene '+scene+' query '+i+' x='+x+' y='+y);
    (got?hits++:misses++);cases++;
  }
  /* Exact radius edge and corner contacts, including fractional positions. */
  for(const c of ch){for(const dx of [0,-c.rad-4,c.rad+4,-7,7])
    for(const dy of [0,-c.rad-4,c.rad+4,-19,19]){
      const x=c.x+dx,y=c.y+dy;
      assert.strictEqual(fast(x,y),oldCollision(ch,solidAt,4,7,19,x,y),'chunk edge parity');
      cases++;
    }}
}
assert(hits>0 && misses>0,'fixture includes both collisions and empty-space queries');
/* The fixed rear-detail random series must preserve the original pixels,
   but its 26 small arrays now live once for the page lifetime. */
const detail=between('const SPIN_WISP=', 'function drawSpin(');
const points=new Function(detail+'\nreturn SPIN_WISP;')();
let r=7;const rn=()=>(r=(r*1103515245+12345)&0x7fffffff)/0x7fffffff;
const before=[];for(let k=0;k<26;k++){const edge=rn();before.push([2+rn()*22,17+rn()*21+(edge<.35?0:0),(rn()-.5)*2.6,1.2+rn()*1.8])}
assert.deepStrictEqual(points,before,'canonical detailed art positions unchanged');
assert(html.includes('const WISP=SPIN_WISP;'),'renderer reuses the static points');
const buttons=between('const WALK_BUTTONS=', 'function drawWalkHUD(');
const dims={w:960,h:540};
const factory=new Function('dims',`let W=dims.w,H=dims.h,WEB_UI={portrait:false};const isPortrait=()=>WEB_UI.portrait;\n${buttons}\nreturn {get:walkBtns,resize:(w,h)=>{W=w;H=h},portrait:v=>{WEB_UI.portrait=v}};`);
const b=factory(dims),prior=b.get(),identities=prior.slice();
for(const [w,h] of [[960,540],[540,960],[320,568],[1280,720],[960,540]]){
  b.resize(w,h);b.portrait(false);const arr=b.get(),y=h-64,rr=w-12;
  assert.strictEqual(arr,prior,'touch bar array is stable across frames and resizes');
  arr.forEach((v,i)=>assert.strictEqual(v,identities[i],'touch region reused '+i));
  assert.deepStrictEqual(arr.map(x=>[x.x,x.y,x.w,x.h]),[
    [10,h-128,62,54],[82,h-128,62,54],
    [rr-202,y,62,54],[rr-132,y,62,54],[rr-62,y,62,54]
  ],'same responsive walk input hitboxes');
}
b.resize(540,960);b.portrait(true);
assert.strictEqual(b.get(),prior,'portrait and landscape reuse the five hitbox objects');
assert.deepStrictEqual(b.get().map(v=>[v.x,v.y,v.w,v.h]),[
  [8,854,80,80],[96,854,80,80],[274,854,80,80],
  [362,854,80,80],[450,854,80,80]
],'portrait walking buttons are 80px, separated and inside the screen');
const rawScript=between('<script>','</script>');
new Function(rawScript.slice('<script>'.length)); // Syntax of whole bundled browser game.
console.log('ok: '+cases+' walk-collision cases, '+hits+' hits / '+misses+' misses, '+hypotCalls+' narrow-phase distances');
console.log('ok: 26 original character detail points reused and all five responsive touch controls stable');
