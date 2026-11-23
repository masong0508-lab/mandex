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
