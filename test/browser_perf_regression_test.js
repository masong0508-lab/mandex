'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function section(a,b){const x=html.indexOf(a),y=html.indexOf(b,x+a.length);assert(x>=0&&y>x,a);return html.slice(x,y)}
const actual=section('const CHUNK_HASH=new Map()', 'function cleanChunks(dt){');
const reference=actual.replace('if(!b||b.maxId<=ch.id)continue;','if(!b)continue;').replace(
  'if(!ch.rest)for(let j=Math.max(0,gy-1);j<=gy+1&&!a.dead;j++){\n        const ground=j>=ROWS,lo=ground?gx-1:Math.max(0,gx-1),hi=ground?gx+1:Math.min(COLS-1,gx+1),row=j*COLS;\n        for(let i=lo;i<=hi&&!a.dead;i++){\n        if(!ground&&!grid[row+i])continue;',
  'for(let j=gy-1;j<=(ch.rest?gy-2:gy+1)&&!a.dead;j++)for(let i=gx-1;i<=gx+1&&!a.dead;i++){\n        if(!(j>=ROWS||(inb(i,j)&&grid[idx(i,j)])))continue;'
).replace('resolveStatic(ch,a,nx,ny,Math.min(pen,4),i,j)}}','resolveStatic(ch,a,nx,ny,Math.min(pen,4),i,j)}');
assert(actual!==reference&&reference.includes('if(!b)continue;')&&reference.includes('inb(i,j)&&grid[idx(i,j)]'));
const fixture=`
const V=8,RC=4,G=900,ROWS=60,SAND=8,RUBBER=7,TNT=5;
let COLS=120,chunks=[],grid,contacts=[],rng=1;
const MAT=Array.from({length:9},()=>({brit:1}));
const idx=(x,y)=>y*COLS+x,inb=(x,y)=>x>=0&&y>=0&&x<COLS&&y<ROWS,clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
const rnd=()=>((rng=Math.imul(rng,1664525)+1013904223>>>0)/4294967296);
const SFX={thud(){}},wake=c=>{c.rest=false},movedTntBoom=a=>{a.dead=true};
function resolveStatic(c,a,nx,ny,pen,i,j){
 contacts.push([c.id,a.lx,a.ly,i,j,nx,ny,pen]);c.cx+=nx*pen;c.cy+=ny*pen;c.cn++;
 const vn=c.vx*nx+c.vy*ny;if(vn<0){c.vx-=vn*nx;c.vy-=vn*ny}
 if(inb(i,j)&&rnd()<.025)grid[idx(i,j)]=0;
}
function scenario(seed){
 rng=seed;COLS=seed%2?120:360;grid=new Uint8Array(COLS*ROWS);contacts=[];chunks=[];
 for(let k=0;k<600;k++){let x=Math.floor(rnd()*COLS),y=35+Math.floor(rnd()*25);grid[idx(x,y)]=1}
 for(let k=0;k<4;k++){
 const x=k===0?-4:k===1?COLS*V-5:200+(k-2)*12,y=seed%3===0?ROWS*V-4:seed%3===1?-3:330;
 const c={id:k+1,x,y,vx:(rnd()-.5)*600,vy:(rnd()-.5)*800,w:(rnd()-.5)*5,ang:rnd()*.8,rest:seed%5===0&&k===1,m:30,I:800,age:0,cs:1,sn:0};
 c.cells=[];for(let i=0;i<25;i++){let lx=(i%5-2)*8,ly=(Math.floor(i/5)-2)*8;c.cells.push({m:i%9===0?TNT:1,h:30,lx,ly,wx:x+lx,wy:y+ly,ch:c,dead:i===7})}chunks.push(c);
 }
 for(let i=0;i<8;i++)stepChunks(1/360);
 return JSON.stringify({contacts,grid:Array.from(grid),chunks:chunks.map(c=>[c.x,c.y,c.vx,c.vy,c.w,c.cn,c.rest,c.cells.map(a=>[a.h,a.dead,a.wx,a.wy])])});
}
`;
const run=s=>new Function(fixture+s+'return scenario')();
const a=run(actual),b=run(reference);
for(let i=1;i<=80;i++)assert.strictEqual(a(i),b(i),'collision parity scene '+i);
const fx=section('function P(o){','function sweat(')+section('function particlePaintRadius(', 'function drawParts(');
new Function('assert','let parts=[];const fxLimit=()=>3;'+fx+`
P({k:0});P({k:1});P({k:6});P({k:5,s:120});assert.strictEqual(parts.length,3);assert(parts.some(p=>p.k===5));P({k:5,s:30});assert.strictEqual(parts.filter(p=>p.k===5).length,2);
assert(particlePaintRadius({k:5,s:120})>=120);assert(particlePaintRadius({k:2,vx:1000,vy:0})>=35);
`)(assert);
const cache=section('function rubblePaintKey(', 'function draw(){');
new Function('assert',`
const V=8,perf={cache:1},MAT=[{}, {hp:100}],rubbleCache=new WeakMap();
let builds=0,draws=0;
const ctx={drawImage(){draws++}},mkCanvas=(w,h)=>{builds++;return {getContext(){return {drawImage(){}}}}},SPR=[[],Array.from({length:4},()=>[{}, {}, {}])];
`+cache+`
const ch={cells:Array.from({length:25},(_,i)=>({m:1,h:100,lx:(i%5)*8,ly:Math.floor(i/5)*8}))};
assert(drawRubbleCached(ch));assert(drawRubbleCached(ch));assert.strictEqual(builds,1);ch.cells[0].h=90;drawRubbleCached(ch);assert.strictEqual(builds,1);ch.cells[0].h=40;drawRubbleCached(ch);assert.strictEqual(builds,2);ch.cells[1].dead=true;drawRubbleCached(ch);assert.strictEqual(builds,3);ch.cells[2].lx+=.5;drawRubbleCached(ch);assert.strictEqual(builds,4);ch.cells=ch.cells.filter(x=>!x.dead);drawRubbleCached(ch);assert.strictEqual(builds,5);drawRubbleCached(ch);assert.strictEqual(builds,5);
`)(assert);
new Function(html.slice(html.indexOf('<script>')+8,html.indexOf('</script>',html.indexOf('<script>'))));
console.log('PERF_REGRESSION_PASS physics_scenes=80 substeps=640 cache_invalidation=1 priority_fx=1 syntax=1');
