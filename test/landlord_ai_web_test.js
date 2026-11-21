/* Exercise production browser landlord AI, especially bat reaction distance.
 * A previous squared-distance optimization left a stale 'tm' reference
 * that JS syntax checks could not catch. */
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync(require('path').join(__dirname,'../web/index.html'),'utf8');
const a=source.indexOf('const WALKS=['),b=source.indexOf('function batBlock(t,m)',a);
assert(a>=0&&b>a,'landlord AI source range unavailable');
const scene=`
let man={mode:'fly',x:0,y:0},targets=[],chunks=[],V=8,G=900;
const SCARES=['EEK!'],solidAt=()=>false;
const lordSay=(t,s)=>{t.lastSay=s};
const fightOn=()=>true;
`+source.slice(a,b)+`
const makeT=()=>({
  x:200,y:160,kind:'bat',alive:true,face:1,stun:0,wind:0,
  swing:0,pan:0,cd:4,blink:1,pan:0,say:0,dodge:0,
  range:40,cover:null,mv:0,t:0,heal:3
});
let t=makeT();man.x=t.x+149;man.y=t.y;
lordAI(t,1/60);
if(!(t.swing>0))throw Error('bat did not react to a flying player at 149px');
t=makeT();man.x=t.x+150;man.y=t.y;
lordAI(t,1/60);
if(t.swing>0)throw Error('bat reacted outside strict 150px range');
t=makeT();man.x=t.x+70;man.y=t.y+130;
lordAI(t,1/60);
if(!(t.swing>0))throw Error('bat range missed diagonal distance');
'web landlord AI: bat proximity, squared distance threshold and no stale variables PASSED';
`;
const result=vm.runInNewContext(scene,{});
assert(result.includes('PASSED'));
console.log(result);
