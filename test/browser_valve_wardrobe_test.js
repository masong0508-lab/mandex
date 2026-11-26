'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function get(a,b){const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,a);return html.slice(i,j)}
const src=get('let valveUsedThisLevel=', 'function load(l,custom)')+get('function wardrobeBandOffset(', 'function drawWardrobeFolds(');
const n=new Function('assert',`let THONG=false;`+src+`
let checks=0;const ok=(v,m)=>{assert(v,m);checks++};
resetLevelWardrobe();ok(!valveUsedThisLevel,'fresh level');
const m={mode:'aim',seated:false,vlv:false,inf:.35,R:14};
ok(!noteValveUse(m)&&!valveUsedThisLevel,'not triggered by selecting a tool');
m.seated=true;ok(noteValveUse(m)&&valveUsedThisLevel,'first valid seating triggers outfit');
ok(!noteValveUse(m),'second use idempotent');ok(m.inf===.35&&m.R===14,'no physical state change');
m.seated=false;
for(const mode of ['aim','fly','walk','stuck','wait','none']){m.mode=mode;ok(valveUsedThisLevel,'persistent across '+mode)}
OUTFIT_RUMPLED=true;
for(let x=-24;x<=24;x+=.25){const y=wardrobeBandOffset(x,24);ok(Number.isFinite(y)&&Math.abs(y)<=.901,'bounded waistband wrinkles');}
ok(Math.abs(wardrobeBandOffset(17,24))>.1,'visible cloth variant');THONG=true;ok(wardrobeBandOffset(17,24)===0,'special outfit unchanged');THONG=false;
resetLevelWardrobe();ok(!valveUsedThisLevel&&!OUTFIT_RUMPLED,'retry resets outfit');ok(wardrobeBandOffset(17,24)===0,'neutral art restored');
m.mode='aim';m.vlv=true;ok(noteValveUse(m),'drag route uses same state');
return checks;`)(assert);
assert(html.includes('resetLevelWardrobe();CAM.reset=true;'),'reset is wired into real load');
assert.strictEqual(html.split('noteValveUse(man);SFX.valveCrack();').length-1,2,'both input routes wired');
assert(html.includes('finally{OUTFIT_RUMPLED=previous}'),'render scope restored');
assert(html.includes('drawValveHousing(VX,VY,open&&m.mode==='+"'aim'"+'&&(m.seated||m.vlv))'),'covered equipment drawn');
console.log('VALVE_WARDROBE_PASS checks='+n+' persistence=1 reset=1 tap_drag=2 garment_scope=1');
