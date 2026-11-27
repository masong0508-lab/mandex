/* Test the production browser support-pass function without duplicating its
 * implementation. Exercise repeated collapses and world width changes. */
'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const begin=html.indexOf('const SUPPORT_QUEUE=[]');
const end=html.indexOf('function recompute(ch){',begin);
assert(begin>=0&&end>begin,'actual support-pass source missing');
const source=html.slice(begin,end);
const harness=String.raw`
const ROWS=60,V=8,SAND=8;
let COLS=120,grid,hb,hp,anch,brk,ox,oy,ovx,ovy;
const MAT=Array.from({length:9},()=>({lim:6}));
const DX=[1,-1,0,0],DY=[0,0,1,-1];
const idx=(x,y)=>y*COLS+x,inb=(x,y)=>x>=0&&y>=0&&x<COLS&&y<ROWS;
let needSupport=false,paperDirty=false;
const detached=[],paperHit=()=>{},wobZero=()=>{},clearLinks=()=>{};
const newChunk=cells=>{detached.push(cells)};
function resize(cols){
  COLS=cols;const n=COLS*ROWS;
  grid=new Uint8Array(n);hb=new Uint8Array(n);hp=new Float32Array(n);
  anch=new Uint8Array(n);brk=new Uint8Array(n);
  ox=new Float32Array(n);oy=new Float32Array(n);
  ovx=new Float32Array(n);ovy=new Float32Array(n);
}
function block(x,y){const k=idx(x,y);grid[k]=1;hp[k]=40}
`;
const checks=String.raw`
let passed=0;function check(ok,msg){assert(ok,msg);passed++}
resize(120);block(0,59);block(55,22);
supportPass();
check(detached.length===1&&detached[0].length===1&&detached[0][0].x===55,
      'first detached island appears correctly');
const firstVis=supportVis,queue=SUPPORT_QUEUE,stack=SUPPORT_STACK;
check(firstVis.length===7200&&stack.length===0,'initial scratch bounds and stack drain');
block(55,22);supportPass();
check(detached.length===2,'visited cells are cleared between repeated collapses');
check(firstVis===supportVis&&queue===SUPPORT_QUEUE&&stack===SUPPORT_STACK,
      'same scratch reused between normal support passes');
for(let i=0;i<120;i++)supportPass();
check(detached.length===2&&supportVis===firstVis,'empty passes do not allocate new bitmap');
resize(360);block(0,59);for(let x=300;x<360;x++)block(x,20);
supportPass();
check(detached.length===3&&detached[2].length===60,
      'wide 360-column island detaches without losing cells');
check(supportVis.length===21600&&supportVis!==firstVis,
      'wide stage allocates exactly one appropriately sized visited bitmap');
const wideVis=supportVis;
block(355,22);supportPass();
check(detached.length===4&&detached[3][0].x===355,
      'new wide-stage collapse is not skipped due to stale visited flags');
check(wideVis===supportVis,'no fresh visited allocation for second wide pass');
resize(120);block(0,59);block(75,23);supportPass();
check(detached.length===5&&detached[4][0].x===75&&supportVis.length===7200,
      'return from 360 columns shrinks scratch and retains detachment correctness');
check(queue===SUPPORT_QUEUE&&stack===SUPPORT_STACK&&stack.length===0,
      'queue and DFS stack identities survive world resizing');
return passed;
`;
const run=new Function('assert',harness+source+checks);
const count=run(assert);
const a=html.indexOf('<script>'),b=html.indexOf('</script>',a);
assert(a>=0&&b>a,'standalone browser script missing');
new Function(html.slice(a+8,b));
console.log('BROWSER_SUPPORT_CACHE_PASS assertions='+count+' widths=120,360,120 syntax=1');

/* Frozen traversal before the linear-offset optimization; compare exact
 * support distances, slab order, material/health, pose and broken links
 * across successive support passes and both supported world widths. */
const referenceSource="const SUPPORT_QUEUE=[],SUPPORT_STACK=[];\nlet supportVis=new Uint8Array(0);\nfunction supportPass(){\n  needSupport=false;hb.fill(255);\n  const q=SUPPORT_QUEUE;q.length=0;\n  for(let x=0;x<COLS;x++){const i=idx(x,ROWS-1);if(grid[i]){hb[i]=0;q.push(i)}}\n  for(let qi=0;qi<q.length;qi++){const i=q[qi],x=i%COLS,y=(i/COLS)|0,h=hb[i];\n    for(let d=0;d<4;d++){const nx=x+DX[d],ny=y+DY[d];if(!inb(nx,ny)||((brk[i]>>d)&1))continue;const j=idx(nx,ny),m=grid[j];if(!m)continue;\n      const nh=DY[d]===-1?0:h+1;if(nh<hb[j]&&nh<=MAT[m].lim+(anch[j]?8:0)){hb[j]=nh;q.push(j)}}}\n  if(supportVis.length!==grid.length)supportVis=new Uint8Array(grid.length);\n  else supportVis.fill(0);\n  const vis=supportVis;\n  for(let i=0;i<grid.length;i++){\n    if(!grid[i]||grid[i]===SAND||hb[i]!==255||vis[i])continue;\n    const comp=[],st=SUPPORT_STACK;let oversized=false;st.length=0;st.push(i);vis[i]=1;\n    while(st.length){const a=st.pop(),x=a%COLS,y=(a/COLS)|0;\n      // Limit one falling slab to 512 voxels, as in the shared C engine.\n      // Keep the remainder in the world until the next support pass.\n      if(comp.length<512)comp.push({x,y,m:grid[a],h:hp[a],px:(x+.5+ox[a])*V,py:(y+.5+oy[a])*V,vx:ovx[a]*V,vy:ovy[a]*V});\n      else oversized=true;\n      for(let d=0;d<4;d++){const nx=x+DX[d],ny=y+DY[d];if(!inb(nx,ny))continue;const j=idx(nx,ny);if(grid[j]&&grid[j]!==SAND&&hb[j]===255&&!vis[j]&&!((brk[a]>>d)&1)){vis[j]=1;st.push(j)}}}\n    for(const c of comp){const k=idx(c.x,c.y);grid[k]=0;hp[k]=0;anch[k]=0;paperHit(c.x,c.y);wobZero(k);clearLinks(k)}\n    paperDirty=true;newChunk(comp);\n    if(oversized)needSupport=true; // detach remaining unsupported cells next pass\n  }\n}\n";
const parityHarness=String.raw`
const ROWS=60,V=8,SAND=8,DX=[1,-1,0,0],DY=[0,0,1,-1];
let COLS,grid,hb,hp,anch,brk,ox,oy,ovx,ovy,needSupport,paperDirty,detached,seed;
const MAT=Array.from({length:9},(_,i)=>({lim:i+2}));
const idx=(x,y)=>y*COLS+x,inb=(x,y)=>x>=0&&x<COLS&&y>=0&&y<ROWS;
const paperHit=()=>{},wobZero=i=>{ox[i]=oy[i]=ovx[i]=ovy[i]=0};
function clearLinks(i){
  brk[i]=0;const x=i%COLS,y=(i/COLS)|0;
  for(let d=0;d<4;d++){
    const X=x+DX[d],Y=y+DY[d];
    if(inb(X,Y))brk[idx(X,Y)]&=~(1<<(d^1));
  }
}
const newChunk=c=>detached.push(c);
function rnd(){seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296}
function run(cols,density,initial,fracture){
  COLS=cols;seed=initial;let n=cols*ROWS;
  grid=new Uint8Array(n);hb=new Uint8Array(n);hp=new Float32Array(n);
  anch=new Uint8Array(n);brk=new Uint8Array(n);ox=new Float32Array(n);oy=new Float32Array(n);
  ovx=new Float32Array(n);ovy=new Float32Array(n);
  for(let i=0;i<n;i++){
    if(rnd()<density){
      grid[i]=1+(rnd()*8|0);hp[i]=rnd()*100;anch[i]=rnd()<.2;
      ox[i]=(rnd()-.5)*.2;oy[i]=(rnd()-.5)*.2;
    }
    if(rnd()<fracture)brk[i]=rnd()*16|0;
  }
  detached=[];needSupport=true;paperDirty=false;const history=[];
  for(let passes=0;passes<6&&needSupport;passes++){
    supportPass();
    history.push([Array.from(grid),Array.from(hb),Array.from(brk),
      detached.map(c=>c.map(a=>[a.x,a.y,a.m,a.h,a.px,a.py,a.vx,a.vy])),needSupport,paperDirty]);
  }
  return JSON.stringify(history);
}`;
const original=new Function(parityHarness+referenceSource+'\nreturn run;')();
const optimized=new Function(parityHarness+source+'\nreturn run;')();
let fixtures=0;
for(const cols of [120,360,120])for(const density of [.015,.15,.45,.85,1])for(const fracture of [0,.08,.6]){
  for(const seed of [7,211]){
    assert.strictEqual(optimized(cols,density,seed,fracture),original(cols,density,seed,fracture),
      'support traversal parity '+[cols,density,seed,fracture]);fixtures++;
  }
}
console.log('BROWSER_SUPPORT_TRAVERSAL_PASS fixtures='+fixtures+' repeated_passes=6 exact_state=1');
