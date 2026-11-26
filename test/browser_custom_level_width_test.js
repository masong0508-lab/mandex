/* Custom-level geometry regression: the 360-column finale must never
 * make a saved 120x60 editor grid load at the wrong width. */
'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const find=(a,b)=>{const i=html.indexOf(a),j=html.indexOf(b,i+a.length);
  assert(i>=0&&j>i,'missing actual browser source '+a);return html.slice(i,j)};
const code=find('function encodeCells(','function decodeCells(');
const levels=find('function decodeCells(','// Shared-code validation');
const scene=new Function(`
 const ROWS=60;let COLS=120,testing=true,lastWidth=null,selTab=-1,selectCustomItem=null;
 function selectRoute(tab,district,item){selTab=tab;selectCustomItem=item}
 const PD={custom:[]},placed=[],targets=[];
 const set=(x,y,m)=>placed.push([x,y,m]),tgt=(x,y)=>targets.push([x,y]);
 const load=(i,level)=>{lastWidth=level.cols;COLS=level.cols||120;level.build()};
 ${code}
 ${levels}
 return {PD,placed,targets,encodeCells,decodeCells,levelFromData,startCustom,
   setCols(n){COLS=n},get cols(){return COLS},get loadWidth(){return lastWidth},
   get testing(){return testing},get selected(){return selectCustomItem},reset(){placed.length=0;targets.length=0}};
`)();
const cells=new Uint8Array(120*60);
for(let i=0;i<10;i++)cells[30*120+50+i]=i%2?2:1;
for(let i=0;i<8;i++)cells[45*120+82+i]=3;
const L={n:'After the Finale',m:5,c:scene.encodeCells(cells),t:[[74,31],[105,46]]};
scene.PD.custom.push(L);
const expected=[];
for(let i=0;i<cells.length;i++)if(cells[i])expected.push([i%120,(i/120)|0,cells[i]]);
scene.setCols(360);
const decoded=scene.decodeCells(L.c,120);
assert.strictEqual(decoded.length,7200,'custom decoder ignores active finale width');
assert.deepStrictEqual(decoded,cells,'all material cells retain their grid coordinates');
scene.startCustom(0);
assert.strictEqual(scene.loadWidth,120,'custom level declares its own world width');
assert.strictEqual(scene.cols,120,'loading custom level resets world geometry');
assert.deepStrictEqual(scene.placed,expected,'all custom cells placed at correct coordinates');
assert.deepStrictEqual(scene.targets,L.t,'landlord targets retain their coordinates');
assert.strictEqual(scene.testing,false,'custom play exits creator playtest mode');
assert.strictEqual(scene.selected,L,'selection retains the actual saved design');
scene.reset();scene.setCols(120);scene.startCustom(0);
assert.deepStrictEqual(scene.placed,expected,'normal-width custom play remains unchanged');
assert.strictEqual(scene.decodeCells(L.c,120).length,7200);
scene.setCols(360);
assert.strictEqual(scene.decodeCells(L.c).length,21600,
  'unspecified decode width retains legacy generic behavior');
assert(html.includes('E.g=decodeCells(L.c,120)'), 'editor explicitly decodes fixed grid');
const a=html.indexOf('<script>'),b=html.indexOf('</script>',a);
new Function(html.slice(a+8,b));
console.log('CUSTOM_LEVEL_WIDTH_PASS finale_360_to_custom_120=1 tiles=18 targets=2 playtest_reset=1 editor=1 syntax=1');
