/* Actual standalone web builder and source catalog must agree with the
 * independently compiled native map fingerprints. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const catalog=JSON.parse(fs.readFileSync(path.join(__dirname,'../content/demolition_contracts.json'),'utf8'));
function section(a,b){const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,'missing '+a);return html.slice(i,j)}
const core=section('function set(x,y,m)','const LEVELS=[');
const campaign=section('const CH3_END=LEVELS.length;','// ---- pneumatic tubes ----');
const expected=[1978458262,3881162397,2234426049,2234742033,3701005221,2662884443,2340284964,2409039517,3635458059,508532293,489688324,984490101,3067476221,989929033,2740109274,1882119693];
const counts=[7,6,6,7,6,8,7,5,8,6,8,7,8,9,9,11],rooms=[7,5,6,7,4,9,7,5,10,6,8,7,8,9,13,9],cells=[325,412,348,364,594,402,325,332,421,300,531,314,369,582,621,754];
const validate=new Function('assert','catalog','expected','counts','rooms','cells',`
const V=8,COLS=120,ROWS=60,SAND=8,MAT=Array.from({length:9},()=>({hp:100})),rnd=()=>.5,
 idx=(x,y)=>y*COLS+x,inb=(x,y)=>x>=0&&x<COLS&&y>=0&&y<ROWS;
let grid,hp,anch,interiors,targets,tubes,pads;const LEVELS=Array.from({length:60},()=>({}));
`+core+campaign+`
assert(JSON.stringify(CONTRACT_DATA)===JSON.stringify(catalog.levels),'embedded offline content differs from catalog');
assert(CH3_END===60&&LEVELS.length===76,'original campaign IDs unchanged');
const seen=new Set();let total=0;
for(let i=0;i<CONTRACT_DATA.length;i++){
 const d=CONTRACT_DATA[i],l=LEVELS[CH3_END+i];
 grid=new Uint8Array(7200);hp=new Float32Array(7200);anch=new Uint8Array(7200);interiors=[];targets=[];tubes=[];pads=[];
 l.build();let hash=2166136261,n=0;
 for(const m of grid){hash=Math.imul(hash^m,16777619)>>>0;if(m)n++}
 assert(hash===expected[i]&&!seen.has(hash),'map fingerprint mismatch or duplicate layout');seen.add(hash);
 assert(n===cells[i]&&targets.length===counts[i]&&interiors.length===rooms[i],'encounter or room count mismatch');
 assert(targets.length<=20&&interiors.length<=20,'native bounded pools');
 for(const t of targets){
   const x=Math.floor(t.x/V),y=Math.round((t.y+7)/V);
   assert(y>=59||grid[idx(x,y)]!==0,'target has no floor');
   for(let j=y-5;j<y;j++)for(let k=x-2;k<=x+2;k++)assert(inb(k,j)&&!grid[idx(k,j)],'clipped landlord '+l.name);
 }
 assert(l.men>=6&&l.bonusPower===CONTRACT_POWERS[d.power]&&l.tip.length<=50&&l.brief.length>50,'missing useful briefing or bonus');
 assert(l.walk===(i===6||i===11),'walking mission flag');
 total+=n;
}
return total;
`);
const n=validate(assert,catalog,expected,counts,rooms,cells);
assert(html.includes('function drawContractReview(){')&&html.includes('function makeContractPlan(data){'),'preflight briefing and site plan');
assert(html.includes('pw.left[CL.bonusPower]=2'),'real inventory grant');
new Function(html.slice(html.indexOf('<script>')+8,html.indexOf('</script>')));
console.log('BROWSER_CONTRACT_CAMPAIGN_PASS layouts=16 distinct=16 cells='+n+' targets=118 rooms=120');
