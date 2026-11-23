/* Real browser rubble solver extraction; never duplicate production logic.
 * Protect collision parity, GC-cache bounds and stale-reference cleanup. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const i=html.indexOf('const CHUNK_HASH=new Map()'),j=html.indexOf('function cleanChunks(dt){',i);
assert(i>=0&&j>i,'production rubble spatial hash missing');
const production=html.slice(i,j);
assert(!production.includes('const map=new Map()'),'per-step Map allocation returned');
const fixture=String.raw`
const V=8,RC=4,G=0,ROWS=60,SAND=8,RUBBER=7,TNT=5;
let COLS=120,chunks=[],grid=new Uint8Array(COLS*ROWS);
const MAT=Array.from({length:9},()=>({brit:1}));
const idx=(x,y)=>y*COLS+x,inb=(x,y)=>x>=0&&y>=0&&x<COLS&&y<ROWS;
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
const impactDamage=()=>{},wobKick=()=>{},lordShock=()=>{},movedTntBoom=()=>{};
const SFX={thud:()=>{}};
let staticContacts=0;
function resolveStatic(){staticContacts++;}
function body(id,points,rest=false){
 const ch={id,x:0,y:0,vx:0,vy:0,w:0,ang:0,cs:1,sn:0,rest,cx:0,cy:0,cn:0,m:10,I:100,age:0,tntImpact:0};
 ch.cells=points.map(([x,y])=>({m:1,dead:false,h:40,lx:x,ly:y,wx:x,wy:y,ch}));
 return ch;
}
function resetBody(ch,x,y){
 ch.x=x;ch.y=y;ch.vx=ch.vy=ch.w=0;ch.ang=0;ch.age=0;ch.cn=0;
 for(const a of ch.cells){a.lx=0;a.ly=0;a.wx=x;a.wy=y}
}
`;
const tests=String.raw`
let count=0;
function check(ok,msg){assert(ok,msg);count++}
const a=body(1,[[0,0]]),b=body(2,[[0,0]]);
function collide(){
 resetBody(a,100,100);resetBody(b,104,100);chunks=[a,b];
 stepChunks(0);return [a.cn,b.cn,a.x,b.x];
}
const first=collide();
check(first[0]>0&&first[1]>0,'adjacent rubble chunks collide');
check(CHUNK_HASH.size===0,'spatial hash cleared after each physics step');
check(CHUNK_BUCKET_POOL.length===2,'initial broadphase creates two buckets');
const cached=CHUNK_BUCKET_POOL.slice();
check(cached.every(b=>b.length===0),'cache retains no cells after collision');
for(let k=0;k<40;k++){
 const got=collide();
 check(JSON.stringify(got)===JSON.stringify(first),'repeated contacts match reference step');
 check(CHUNK_BUCKET_POOL[0]===cached[0]&&CHUNK_BUCKET_POOL[1]===cached[1],
       'same buckets reused on later physics steps');
 check(CHUNK_HASH.size===0&&cached.every(b=>b.length===0),
       'no stale references after repeated broadphase passes');
}
chunks=[a];resetBody(a,100,100);stepChunks(0);
check(CHUNK_HASH.size===0&&CHUNK_BUCKET_POOL.every(b=>b.length===0),
      'removed chunks cannot persist through the collision cache');
function large(count,cols){
 COLS=cols;grid=new Uint8Array(COLS*ROWS);
 const points=[];for(let i=0;i<count;i++)points.push([(i%COLS)*V+4,Math.floor(i/COLS)*V+4]);
 chunks=[body(5,points,true)];stepChunks(0);
}
large(2105,120);
check(CHUNK_BUCKET_POOL.length===CHUNK_BUCKET_CACHE_MAX,
      'cache stays capped under dense rubble');
check(CHUNK_HASH.size===0&&CHUNK_BUCKET_POOL.every(b=>b.length===0),
      'large rubble body does not stay referenced');
const identities=CHUNK_BUCKET_POOL.slice();
large(2304,360);
check(CHUNK_BUCKET_POOL.length===CHUNK_BUCKET_CACHE_MAX&&
      CHUNK_BUCKET_POOL.every((b,i)=>b===identities[i]),
      'wide levels reuse the same bounded storage');
large(400,120);
check(CHUNK_BUCKET_POOL.length===CHUNK_BUCKET_CACHE_MAX&&
      CHUNK_BUCKET_POOL.every(b=>b.length===0),
      'return to normal width preserves empty cache');
check(staticContacts===0,'test scenarios avoid static walls');
return count;
`;
const passed=new Function('assert',fixture+production+tests)(assert);
const a=html.indexOf('<script>'),b=html.indexOf('</script>',a);
assert(a>=0&&b>a,'standalone JS script missing');
new Function(html.slice(a+8,b));
console.log('BROWSER_COLLISION_HASH_PASS assertions='+passed+' widths=120,360,120 maxcache=2048 syntax=1');
