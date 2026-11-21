/* Actual browser creator codec: bounded imports, valid legacy MX1,
 * Unicode names, full-sized RLE and safe user level data. */
'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const start=html.indexOf('function encodeCells(');
const end=html.indexOf('// ---------- the editor ----------',start);
assert(start>=0&&end>start,'real creator codec is available');
const create=new Function(`
  const V=8,ROWS=60;let COLS=120;const EMIN=28;
  let promptText='',lastToast='',saved=0,dirty=0,exported=0;
  const PD={custom:[]};const ST=()=>({set exported(x){exported=x},get exported(){return exported}});
  const pdTouch=()=>dirty++,pdSave=()=>saved++,achCheck=()=>{};
  const toast=(kind,message)=>{lastToast=message};
  const navigator={clipboard:{writeText:()=>Promise.resolve()}};
  const prompt=(label,value)=>{if(label.startsWith('Paste'))return promptText;return value};
  ${html.slice(start,end)}
  return {encodeCells,decodeSharedLevel,encodeSharedLevel,validateSharedLevel,
          importCode,exportCode,PD,get lastToast(){return lastToast},
          get saved(){return saved},setPrompt(x){promptText=x},setCols(x){COLS=x}};
`)();
const grid=new Uint8Array(120*60);
for(let i=0;i<8;i++)grid[30*120+30+i]=1;
const canonical={n:'Café 世界 😀',m:5,c:create.encodeCells(grid),t:[[56,31],[80,25]]};
const code=create.encodeSharedLevel(canonical);
assert(code.startsWith('MX1.'),'MX1 prefix retained');
assert.deepStrictEqual(create.decodeSharedLevel(code),canonical,'unicode round trip');
create.setCols(360);
assert.deepStrictEqual(create.decodeSharedLevel(code),canonical,
  'share-code geometry stays 120x60 after 360-column finale');
create.setCols(120);
const old='MX1.'+btoa(JSON.stringify({...canonical,n:'Legacy ASCII'}));
assert.strictEqual(create.decodeSharedLevel(old).n,'Legacy ASCII','existing ASCII codes read');
const latin='MX1.'+btoa(JSON.stringify({...canonical,n:'Café'}));
assert.strictEqual(create.decodeSharedLevel(latin).n,'Café','legacy Latin-1 names read');
function rejects(o,message){
  assert.throws(()=>create.validateSharedLevel(o),Error,message);
}
rejects({...canonical,c:[0,7201]},'oversized run');
rejects({...canonical,c:[0,7200,1,1]},'overflow run');
rejects({...canonical,c:[0,7199]},'short run');
rejects({...canonical,c:[1,7200]},'blocked sling zone');
rejects({...canonical,c:[0,7200.5]},'fractional run');
rejects({...canonical,c:[9,7200]},'invalid material');
rejects({...canonical,c:[0,-1]},'negative run');
rejects({...canonical,t:[]},'empty landlord set');
rejects({...canonical,t:[[120,30]]},'landlord beyond world');
rejects({...canonical,t:[[28,0]]},'landlord below world');
rejects({...canonical,t:[[30,31]]},'landlord inside wall');
rejects({...canonical,t:[[56,31],[56,31]]},'duplicate landlords');
rejects({...canonical,t:[[56.5,31]]},'non-integer coordinates');
rejects({...canonical,m:99},'unreasonable player count');
assert.throws(()=>create.decodeSharedLevel('MX1.'+'A'.repeat(100001)),Error,'oversized code rejected before decode');
const before=create.PD.custom.length,priorSave=create.saved;
create.setPrompt('MX1.'+btoa(JSON.stringify({...canonical,t:[[-100,31]]})));
create.importCode();
assert.strictEqual(create.PD.custom.length,before,'invalid import does not mutate save');
assert.strictEqual(create.saved,priorSave,'invalid import does not write save');
create.setPrompt(code);create.importCode();
assert.strictEqual(create.PD.custom.length,before+1,'valid import adds exactly one');
assert.strictEqual(create.saved,priorSave+1,'valid import persists');
assert.strictEqual(create.PD.custom[before].n,'Café 世界 😀','Unicode saved correctly');
assert.strictEqual(create.exportCode(canonical),code,'export matches pure encoder');
new Function(html.slice(html.indexOf('<script>')+8,html.indexOf('</script>'))); // whole shipped game parses
console.log('WEB_CREATOR_CODEC_PASS legacy=2 unicode=1 fixed_grid=1 rejects=14 atomic_import=1 whole_script=1');
