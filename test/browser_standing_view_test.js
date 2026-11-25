/* Execute the actual standing-tile renderer with a synthetic crowded map.
 * Every tile that could reach the screen under shake must still be painted. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const i=html.indexOf('const voxelPad='),j=html.indexOf('/* At high zoom, most collapsed rubble',i);
assert(i>=0&&j>i,'missing tile painter');
const draw=new Function('CAM','W','H','COLS','ROWS','V','paperShake','wobOn','WOB_OMAX','camOn',`
  const grid=new Uint8Array(COLS*ROWS).fill(1),hp=new Float32Array(COLS*ROWS).fill(100),brk=new Uint8Array(COLS*ROWS);
  const ox=new Float32Array(COLS*ROWS),oy=new Float32Array(COLS*ROWS),MAT=[null,{hp:100}],GLASS=4;
  const visible=new Set(),idx=(x,y)=>y*COLS+x,hsh=(x,y)=>0,SPR=[null,[[0,0,0]]];
  let calls=0;
  const ctx={drawImage(_,x,y){calls++;visible.add(x+','+y)}};
  for(let i=0;i<grid.length;i++){ox[i]=((i*17)%7)-3;oy[i]=((i*23)%7)-3;}
  `+html.slice(i,j)+`
  let expected=0;
  for(let y=0;y<ROWS;y++)for(let x=0;x<COLS;x++){
    const i=idx(x,y),px=x*V+(wobOn?ox[i]*V:0),py=y*V+(wobOn?oy[i]*V:0);
    // Touching without overlapping is not a visible pixel.
    if(px+V<=CAM.x-paperShake||px>=CAM.x+W/CAM.z+paperShake||py+V<=CAM.y-paperShake||py>=CAM.y+H/CAM.z+paperShake)continue;
    expected++;
    if(!visible.has(px+','+py))throw Error('visible tile missing '+[x,y,px,py]);
  }
  return {calls,expected};
`);
let cases=0,maxSaved=0;
for(const cols of [120,360])for(const x of [0,320,cols*8-270])for(const y of [-40,0,200,400])for(const z of [1,1.8,3])for(const shake of [0,12])for(const wobble of [false,true]){
  const result=draw({x,y,z},270,180,cols,60,8,shake,wobble,3,true);
  const oldCols=Math.max(0,Math.min(cols,Math.ceil((x+270/z)/8)+2)-Math.max(0,Math.floor(x/8)-2));
  maxSaved=Math.max(maxSaved,oldCols*60-result.calls);cases++;
}
assert(maxSaved>0,'no reduced draw work');
console.log('BROWSER_STANDING_VIEW_PASS cases='+cases+' max_draw_calls_saved='+maxSaved);
