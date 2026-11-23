/* Real browser level builder instrumentation, not a duplicate layout.
   Guards the 12 late Wolfman alley scenes and their C-core coordinates. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const start=html.indexOf('// ---------- CHAPTER 3: the Wolfman');
const end=html.indexOf('const chEnd=',start);
assert(start>=0&&end>start,'live chapter-three constructor exists');
const chapter=html.slice(start,end);
const ROWS=60;
const [WOOD,BRICK,STONE,GLASS,STEEL,RUBBER,SAND]=[1,2,3,4,6,7,8];
const LEVELS=Array.from({length:40},()=>({name:'earlier chapter'}));
let houses=[],blocks=[],charges=[],targets=[];
function house(o){houses.push(o)}
function rect(x,y,w,h,m){blocks.push({x,y,w,h,m})}
function tnt(x,y){charges.push([x,y])}
function tgt(x,row,kind,range){targets.push({x,row,kind,range})}
new Function('LEVELS','ROWS','WOOD','BRICK','STONE','GLASS','STEEL',
  'RUBBER','SAND','house','rect','tnt','tgt','targets',chapter)(
    LEVELS,ROWS,WOOD,BRICK,STONE,GLASS,STEEL,RUBBER,SAND,
    house,rect,tnt,tgt,targets);
assert.strictEqual(LEVELS.length,60,'level indexing and saves remain unchanged');
let checked=0;
function has(x,y,w,h,m){
  return blocks.some(b=>b.x===x&&b.y===y&&b.w===w&&b.h===h&&b.m===m);
}
for(let i=0;i<20;i++){
  houses=[];blocks=[];charges=[];targets=[];
  const lev=LEVELS[40+i];
  assert.equal(typeof lev.build,'function','playable web level');
  lev.build();
  const x2=80+(i%2)*4,w2=10+(i%5)*2;
  const gap=108-(x2+w2),bx=x2+w2+Math.floor((gap-4)/2);
  const alley=houses.filter(h=>h.x===bx&&h.w===4);
  assert.strictEqual(alley.length,i>=8?1:0,'second street landmark only on last 12');
  if(i<8){checked++;continue;}
  assert(gap>=6&&bx>=x2+w2+1&&bx+4<=107,'alley fits second gap');
  const typ=i%4,h=alley[0];
  assert(h.floors===1&&h.fh===7,'small one-storey lane prop');
  assert.strictEqual(h.wall,typ===0||typ===2?GLASS:typ===1?RUBBER:STEEL,
                     'matching wall material');
  assert.strictEqual(h.roof,typ===0||typ===3?GLASS:WOOD,
                     'matching roof material');
  assert(charges.some(([x,y])=>x===bx+1&&y===(typ===2?56:59)),
         'actual mobile-TNT weak point');
  if(typ===0)assert(has(bx+1,48,2,2,GLASS),'glass signal at roof');
  if(typ===1){
    assert(has(bx+1,59,2,1,RUBBER),'soft TNT landing mat');
    assert(has(bx+1,49,2,1,RUBBER),'spring gate rubber cap');
  }
  if(typ===2){
    assert(has(bx+1,56,2,3,SAND),'sandbag spill inside the shed');
    assert(has(bx+1,48,2,2,SAND),'upper sand signal');
  }
  if(typ===3)assert(has(bx+1,48,2,2,STEEL),'reinforced signal');
  if(![10,15].includes(i))
    assert(lev.tip.includes('NIGHT ALLEY'),'new challenge has tactical help');
  assert(targets.length<=20&&houses.length<=5,'fixed native capacities honored');
  checked++;
}
console.log('PASS: '+checked+' web Wolfman encounters, 12 new alleys, real materials and stage cues');
