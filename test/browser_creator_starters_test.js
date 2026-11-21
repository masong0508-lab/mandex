/* Browser level creator features: 3 validated starter buildings, reversible
 * world edits on touch/desktop, a bounded history, and correct save identity. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function fragment(a,b){const i=html.indexOf(a),j=html.indexOf(b,i+a.length);
  assert(i>=0&&j>i,'Missing production fragment '+a);return html.slice(i,j)}
const codecs=fragment('function encodeCells(','function encodeSharedLevel(');
const editor=fragment('function eState(){','function eCell(');
const create=new Function(`
 const ROWS=60,EMIN=28;let COLS=120,notice='';
 const toast=(k,title,detail)=>{notice=title};
 const panel=()=>{},txt=()=>{},btn=()=>{};
 const E={g:new Uint8Array(7200),t:[],men:5,mat:1,undo:[],redo:[],
          starters:false,id:3,name:'Old Design',drag:null};
 ${codecs}
 ${editor}
 return {E,eState,eSnap,eUndo,eRedo,eAdjustMen,eStarterPattern,eApplyStarter,validateSharedLevel,
          encodeCells,get notice(){return notice}};
`)();
const old=create.E;
old.g[30*120+40]=2;
old.t=[[60,31]];old.id=3;old.name='Old Design';old.men=4;
const previous=old.g.slice();
const patterns=new Set();
for(let kind=0;kind<3;kind++){
  const a=create.eStarterPattern(kind),b=create.eStarterPattern(kind);
  assert.strictEqual(a.grid.length,120*60,'fixed creator dimensions');
  assert.deepStrictEqual(a.level,b.level,'starter layout must be deterministic');
  assert.strictEqual(a.level.t.length>=2,true,'starter has viable landlords');
  assert.strictEqual(a.level.m,6,'starter includes reasonable shot allowance');
  assert.deepStrictEqual(create.validateSharedLevel(a.level),a.level,
    'starter can be shared and played with existing codec');
  for(let y=0;y<60;y++)for(let x=0;x<28;x++)
    assert.strictEqual(a.grid[y*120+x],0,'sling area remains open');
  patterns.add(JSON.stringify(a.level.c));
}
assert.strictEqual(patterns.size,3,'starter designs are distinct');
create.eApplyStarter(0);
assert.strictEqual(old.id,-1,'starter creates new level rather than overwriting loaded project');
assert.strictEqual(old.name,'THREE-STOREY TOWER','starter name appears in editor');
assert.strictEqual(old.undo.length,1,'starter is one reversible action');
const selected=old.g.slice(),name=old.name;
create.eUndo();
assert.deepStrictEqual(old.g,previous,'undo restores whole previous world');
assert.strictEqual(old.id,3,'undo restores previous custom level save ID');
assert.strictEqual(old.name,'Old Design','undo restores previous name');
assert.strictEqual(old.men,4,'undo restores previous man count');
assert.deepStrictEqual(old.t,[[60,31]],'undo restores landlord positions');
create.eRedo();
assert.deepStrictEqual(old.g,selected,'redo restores new starter world');
assert.strictEqual(old.name,name,'redo restores starter name');
assert.strictEqual(old.id,-1,'redo cannot overwrite prior saved level');
create.eUndo();
create.eSnap();old.g[40*120+40]=3;
assert.strictEqual(old.redo.length,0,'new edit invalidates redo history');
create.eRedo();
assert.strictEqual(old.g[40*120+40],3,'redo cannot replay stale branch');
for(let i=0;i<60;i++){create.eSnap();old.g[45*120+40]=i%8}
assert(old.undo.length<=24,'undo snapshots stay within memory cap');
old.men=5;
assert.strictEqual(create.eAdjustMen(-1),true,'changing character count records an edit');
assert.strictEqual(old.men,4);
create.eUndo();assert.strictEqual(old.men,5,'Undo restores character count');
create.eRedo();assert.strictEqual(old.men,4,'Redo reapplies character count');
old.men=1;let history=old.undo.length;
assert.strictEqual(create.eAdjustMen(-1),false,'minimum is an unchanged edit');
assert.strictEqual(old.undo.length,history,'minimum must not consume an undo state');
old.men=12;history=old.undo.length;
assert.strictEqual(create.eAdjustMen(1),false,'maximum is an unchanged edit');
assert.strictEqual(old.undo.length,history,'maximum must not consume an undo state');
assert(html.includes("['MEN -',()=>eAdjustMen(-1)]") &&
       html.includes("['MEN +',()=>eAdjustMen(1)]") &&
       html.includes("btn(x,y2,26,24,'-',()=>eAdjustMen(-1)") &&
       html.includes("btn(x+98,y2,26,24,'+',()=>eAdjustMen(1)"),
       'desktop and portrait count controls use the reversible helper');
assert.strictEqual(create.eStarterPattern(1).level.n,'BREAKAWAY BRIDGE');
assert.strictEqual(create.eStarterPattern(2).level.n,'FACTORY WORKSHOP');
assert(html.includes("'STARTER BUILDS'")&&html.includes("'STARTERS'")&&
       html.includes("'REDO',eRedo"),'desktop and mobile expose feature buttons');
assert(html.includes("if(E.starters)return;"),'starter overlay blocks accidental edits');
const a=html.indexOf('<script>'),b=html.indexOf('</script>',a);
new Function(html.slice(a+8,b));
console.log('BROWSER_STARTERS_PASS count=3 validated=3 undo_redo=1 saved_id=1 memory_cap=24 mobile=1 men_undo=1 syntax=1');
