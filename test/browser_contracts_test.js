/* Contract generation and exact browser blast optimization. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function fragment(a,b){const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,a);return html.slice(i,j)}
const world=fragment('function set(x,y,m)','// landlord kinds');
const contracts=fragment('const CONTRACT_FAMILIES=','function drawContractSelect(');
const build=new Function(`
 const ROWS=60,COLS=120,V=8,STONE=3,GLASS=4,TNT=5,STEEL=6,RUBBER=7,SAND=8;
 let grid=new Uint8Array(7200),hp=new Float32Array(7200),anch=new Uint8Array(7200),interiors=[],targets=[],PD={contracts:{}};
 const MAT=Array.from({length:9},()=>({hp:1}));
 const idx=(x,y)=>y*120+x,inb=(x,y)=>x>=0&&x<120&&y>=0&&y<60;
 function tgt(x,y,kind){targets.push({x,y,kind})}
 ${world}
 ${contracts}
 ${fragment('function bodyClear(','function unclipTargets(')}
 return {CONTRACTS,contractStars,PD,run(id){grid.fill(0);targets=[];interiors=[];let q=buildContract(id);q.build();
 return {q,grid:grid.slice(),targets,interiors,clear:targets.every(t=>bodyClear(t.x,t.y))}}}
`)();
assert.strictEqual(build.CONTRACTS.length,72);
const signatures=new Set();
for(let id=0;id<72;id++){
 const a=build.run(id),b=build.run(id);
 assert.deepStrictEqual(a.grid,b.grid,'deterministic contract '+id);
 assert.deepStrictEqual(a.targets,b.targets);
 assert(a.clear,'clear and supported landlords '+id);
 assert(a.targets.length>=2&&a.targets.length<=12,'target bounds '+id);
 assert(a.interiors.length>=1,'destructible room backdrops '+id);
 assert(a.q.custom&&a.q.contract===id&&a.q.cols===120);
 assert(a.q.men>=5&&a.q.men<=9);
 for(let y=0;y<60;y++)for(let x=0;x<28;x++)assert.strictEqual(a.grid[y*120+x],0,'sling area');
 signatures.add(Array.from(a.grid).join(''));
 if(id%4===1)assert(a.grid.includes(7),'bank material');
 if(id%4===2)assert(a.grid.includes(5),'TNT supports');
 if(id%4===3)assert(a.targets.some(t=>t.kind==='shield'),'guard');
}
assert.strictEqual(signatures.size,72,'all contracts have distinct grids');
assert.strictEqual(build.contractStars(0),0);
build.PD.contracts[0]=2;assert.strictEqual(build.contractStars(0),2);
build.PD.contracts[0]=99;assert.strictEqual(build.contractStars(0),0);
assert(html.includes("if(selTab===4)return CONTRACT_ITEMS"));
assert(html.includes("if(selTab===4){drawContractSelect();return}"));
assert(html.includes("PD.contracts[CL.contract]=Math.max(contractStars(CL.contract),st)"));
assert(html.includes("PD.contracts={}"));
const blastSrc=fragment('function blast(cx,cy,r,power)','function applyImpulse(');
const compare=new Function(`
 const COLS=120,ROWS=60,idx=(x,y)=>y*COLS+x;
 let grid,paper,hits;
 function damageCell(x,y,d){
   if(x<0||y<0||x>=COLS||y>=ROWS)return 0;
   const k=idx(x,y);
   if(!grid[k]){if(paper[k]&&d>=4){hits.push([x,y,d,'paper']);paper[k]=0}return 0}
   hits.push([x,y,d,'block']);grid[k]=0;return d;
 }
 ${blastSrc}
 function old(cx,cy,r,power){let spent=0;const R=Math.ceil(r);
 for(let j=-R;j<=R;j++)for(let i=-R;i<=R;i++){const d=Math.hypot(i,j);if(d>r)continue;spent+=damageCell(cx+i,cy+j,power*(1-d/r)+(d<1?power*.25:0))}return spent}
 function run(fn,x,y,r,seed){
   grid=new Uint8Array(7200);paper=new Uint8Array(7200);hits=[];
   for(let i=0;i<grid.length;i++){let k=(Math.imul(i+seed,1103515245)>>>0)%43;
     if(k<3)grid[i]=1;else if(k===7)paper[i]=1}
   const spent=fn(x,y,r,40);return {spent,hits,grid,paper}
 }
 return (x,y,r,seed)=>[run(old,x,y,r,seed),run(blast,x,y,r,seed)];
`)();
for(const [x,y] of [[0,0],[119,59],[-2,30],[121,40],[60,25],[10,59]])
 for(const r of [1,3,7.5,15,24])for(let seed=0;seed<3;seed++){
   const [a,b]=compare(x,y,r,seed);assert.deepStrictEqual(a,b,'blast parity');
 }
const i=html.indexOf('<script>'),j=html.indexOf('</script>',i);
new Function(html.slice(i+8,j));
console.log('CONTRACTS_PASS maps=72 distinct=72 blast_cases=90 syntax=1');
