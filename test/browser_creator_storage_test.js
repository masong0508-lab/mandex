/* Regression for browser creator storage quota and private-window failures.
 * Exercises the real production codec, persistence routine and editor save. */
'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const fragment=(start,end)=>{let a=html.indexOf(start),b=html.indexOf(end,a+start.length);
  assert(a>=0&&b>a,'missing production fragment '+start);return html.slice(a,b)};
const persistence=fragment('let pdDirty=false;','pdLoad();');
const codec=fragment('function encodeCells(','// ---------- the editor ----------');
const preflight=fragment('function eCheckedLevel(','function eTest(){');
const eSaveSource=fragment('function eSave(){','const MATCOL=');
const create=new Function(`
 const ROWS=60,EMIN=28;let COLS=120;
 const SAVE_KEY='mandex-test';let stored='',throwWrite=false,saveCount=0;
 const PD={custom:[],st:{created:0,exported:0}};
 const ST=()=>PD.st;
 const localStorage={setItem(key,text){if(throwWrite)throw Error('quota');
   stored=text;saveCount++}};
 const toast=(kind,title,detail)=>{messages.push([kind,title,detail])};
 const messages=[],prompts=[];
 const prompt=(label,value)=>{prompts.push([label,value]);return value};
 const achCheck=()=>{};
 ${persistence}
 ${codec}
 const E={g:new Uint8Array(7200),t:[[55,31]],id:-1,name:'Saved Name',men:5};
 ${preflight}
 ${eSaveSource}
 return {PD,E,eSave,pdSave,pdTouch,importCode,decodeSharedLevel,encodeSharedLevel,
  get stored(){return stored},get saveCount(){return saveCount},
  get dirty(){return pdDirty},messages,prompts,setFailure(v){throwWrite=v}};
`)();
for(let i=0;i<8;i++)create.E.g[30*120+30+i]=1;
create.setFailure(true);
assert.strictEqual(create.pdSave(),false,'quota failure is visible');
assert.strictEqual(create.dirty,true,'failed write stays dirty for retry');
create.eSave();
assert.strictEqual(create.PD.custom.length,0,'failed new save rolls back');
assert.strictEqual(create.E.id,-1,'failed new save leaves editor unsaved');
assert.strictEqual(create.PD.st.created,0,'failed new save preserves achievement counter');
assert(create.prompts.some(p=>p[0].startsWith('Storage unavailable')&&p[1].startsWith('MX1.')),
 'new design offers copyable recovery share code');
assert(create.messages.some(p=>p[1]==='Level not saved'),'player sees save failure');
create.setFailure(false);
create.eSave();
assert.strictEqual(create.PD.custom.length,1,'valid retry succeeds');
assert.strictEqual(create.PD.st.created,1,'successful retry increments once');
assert.strictEqual(create.dirty,false,'successful save clears dirty flag');
const successful=create.PD.custom[0],stored=create.stored;
create.E.g[30*120+30]=2;create.setFailure(true);
create.eSave();
assert.strictEqual(create.PD.custom.length,1,'failed edit leaves one saved design');
assert.strictEqual(create.PD.custom[0],successful,'failed edit rolls back original object');
assert.strictEqual(create.stored,stored,'failed update cannot overwrite last good save');
assert.strictEqual(create.PD.st.created,1,'failed edit does not create an extra level');
const copyCode=create.encodeSharedLevel(successful);
create.prompts.length=0;
create.setFailure(false);
create.E.id=-1;create.E.name='A second level';
create.eSave();
assert.strictEqual(create.PD.custom.length,2,'next valid save still works');
create.setFailure(true);
const oldLength=create.PD.custom.length,oldStored=create.stored;
const realPrompt=copyCode;
assert.strictEqual(create.decodeSharedLevel(realPrompt).n,successful.n,
  'recovery share code remains independently importable');
// Inject a valid import via source-equivalent prompt mock's global override.
const quotaImport=new Function('PD','pdTouch','pdSave','decodeSharedLevel','toast','prompt',`
  ${html.slice(html.indexOf('function importCode(){'),html.indexOf('// ---------- the editor ----------',html.indexOf('function importCode(){')))}
  return importCode;
`);
const importLevel=quotaImport(create.PD,()=>{},()=>false,create.decodeSharedLevel,
  (k,m)=>create.messages.push([k,m]),()=>realPrompt);
importLevel();
assert.strictEqual(create.PD.custom.length,oldLength,'failed import restores custom list');
assert.strictEqual(create.stored,oldStored,'failed import cannot overwrite storage');
assert(create.messages.some(p=>p[1]==='Level not imported'),'failed import warns player');
new Function(html.slice(html.indexOf('<script>')+8,html.indexOf('</script>')));
console.log('CREATOR_STORAGE_RECOVERY_PASS quota=1 new_save_rollback=1 edit_rollback=1 backup=1 import_rollback=1 retry=1');
