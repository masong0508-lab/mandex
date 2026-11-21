/* Production browser editor regression: large-brush target removal without
 * per-tile arrays, cancellation-safe rectangle history, and on-demand checks. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function fragment(a,b){
  const i=html.indexOf(a),j=html.indexOf(b,i+a.length);
  assert(i>=0&&j>i,'Missing production fragment '+a);
  return html.slice(i,j);
}
const codec=fragment('function encodeCells(','function encodeSharedLevel(');
const history=fragment('function eState(){','// Deterministic starter structures:');
const paint=fragment('function eCell(','function eBuild(){');
const preflight=fragment('function eCheckedLevel(action){','function eTest(){');
const create=new Function(`
 const ROWS=60,COLS=120,EMIN=28,V=8,GY=480;
 const idx=(x,y)=>y*COLS+x;
 const messages=[],toast=(kind,title,detail)=>messages.push([kind,title,detail]);
 const E={g:new Uint8Array(7200),t:[],id:-1,name:'Creator test',men:5,
   tool:'mat',mat:2,shape:'rect',bk:0,bs:0,bt:1,undo:[],redo:[],
   drag:null,pointer:null};
 const PD={custom:[]},testing=false,set=()=>{},tgt=()=>{},load=()=>{};
 ${codec}
 ${history}
 ${paint}
 ${preflight}
 return {E,messages,eDown,eMove,eUp,eBeginStroke,eEndStroke,
   eUndo,eRedo,eClearLord,eCheckBuild};
`)();
const E=create.E;
function pointerRect(id,x0,y0,x1,y1,cancel=false){
  create.eDown(x0*8,y0*8);
  assert(create.eBeginStroke(id),'rectangle must capture its owning finger');
  create.eMove(x1*8,y1*8);
  assert(create.eEndStroke(id,cancel),'owning finger must end the stroke');
}
pointerRect(1,50,30,60,35,true);
assert.strictEqual(E.undo.length,0,'cancelled rectangle must not create undo');
assert.strictEqual(E.g[31*120+52],0,'cancelled rectangle must not paint');
pointerRect(2,50,30,60,35);
assert.strictEqual(E.undo.length,1,'completed rectangle is a single edit');
assert.strictEqual(E.g[31*120+52],2,'completed rectangle paints');
create.eUndo();
assert.strictEqual(E.g[31*120+52],0,'undo reverts committed rectangle');
assert.strictEqual(E.redo.length,1,'redo preserves the previous state');
pointerRect(3,52,30,54,32,true);
assert.strictEqual(E.redo.length,1,'cancel keeps redo branch');
assert.strictEqual(E.undo.length,0,'cancel does not consume history');
pointerRect(4,50,30,60,35);
assert.strictEqual(E.redo.length,0,'commit invalidates stale redo');
const originalTargets=[[55,31],[59,31],[75,31]];
E.t=originalTargets;
pointerRect(5,54,30,61,31);
assert.strictEqual(E.t,originalTargets,'painting never reallocates target array');
assert.deepStrictEqual(E.t,[[75,31]],'paint removes only overlapping targets');
let length=E.undo.length;
create.eDown(-10,20);
create.eDown(50000,20);
create.eDown(50*8,-5);
create.eDown(50*8,800);
assert.strictEqual(E.undo.length,length,'invalid touches never write history');
assert.strictEqual(E.drag,null,'invalid taps cannot capture a stroke');
E.tool='lord';
E.g[45*120+80]=1;
create.eDown(80*8,45*8);
assert.strictEqual(E.undo.length,length,'blocked landlord placement has no history');
E.t=Array.from({length:12},(_,i)=>[40+i,21]);
create.eDown(80*8,22*8);
assert.strictEqual(E.undo.length,length,'full target inventory has no history');
E.t=[];
create.eDown(80*8,22*8);
assert.strictEqual(E.undo.length,length+1,'valid target placement is undoable');
assert.deepStrictEqual(E.t,[[80,23]]);
E.t=[];E.g=new Uint8Array(7200);
assert.strictEqual(create.eCheckBuild(),false,'empty level should not pass preflight');
assert(create.messages.some(x=>x[1]==='Fix level before checking'),
  'invalid check displays existing validator error');
for(let i=0;i<8;i++)E.g[35*120+50+i]=1;
E.t=[[60,36]];E.men=4;
assert.strictEqual(create.eCheckBuild(),true,'valid level passes preflight');
assert(create.messages.some(x=>x[1]==='BUILD READY'&&x[2].includes('1 landlords')&&x[2].includes('4 shots')),
  'successful check provides real objective and shot counts');
assert(html.includes("['CHECK BUILD',eCheckBuild]"),'portrait actions show check');
assert(html.includes("'CHECK BUILD',eCheckBuild,{size:11}"),'desktop actions show check');
assert(!html.includes('E.t=E.t.filter('),'per-tile target array allocation removed');
const a=html.indexOf('<script>'),b=html.indexOf('</script>',a);
new Function(html.slice(a+8,b));
console.log('CREATOR_PAINT_PASS cancel=1 redo_preserve=1 commit=1 target_alloc=0 invalid_taps=1 preflight=1 portrait=1 landscape=1 syntax=1');
