/* Run the real browser shelter-survivor code against a synthetic room.
 * No DOM, browser renderer or mocked survivor algorithm needed. */
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync(require('path').join(__dirname,'../web/index.html'),'utf8');
function chunk(a,b){
  const start=html.indexOf(a),end=html.indexOf(b,start);
  assert(start>=0&&end>start,'missing production function '+a);
  return html.slice(start,end);
}
const program=`
const V=8,COLS=60,ROWS=40,WW=COLS*V,GY=ROWS*V;
const grid=new Uint8Array(COLS*ROWS);
const idx=(x,y)=>y*COLS+x,inb=(x,y)=>x>=0&&y>=0&&x<COLS&&y<ROWS;
const interiors=[{x:20,y:15,w:10,h:8}],paperFrameOrig=[36];
const texts=[],rings=[];
let chanceRoll=0;
const rnd=()=>chanceRoll;
const popText=(x,y,msg)=>texts.push(msg),ring=()=>rings.push(1);
const spark=()=>{},SFX={thud(){}};
`+chunk('function roomFrameSupport(r){','function paperRoomUpdate(){')+
chunk('function solidAt(px,py){','function killTarget(t,hard')+
chunk('const COLLAPSE_POCKETS=','function updTargets(dt){')+`
const t={x:24.5*V,y:19*V,r:7,alive:true,hp:3,hpMax:3,armor:1,inv:0,stun:0,vy:17,roomId:0,collapseRolled:0};
const room=interiors[0];
for(let x=room.x;x<room.x+room.w;x++){grid[idx(x,room.y-1)]=1;grid[idx(x,room.y+room.h)]=1}
for(let y=room.y;y<room.y+room.h;y++){grid[idx(room.x-1,y)]=1;grid[idx(room.x+room.w,y)]=1}
if(roomFrameSupport(room)!==36)throw Error('full room fixture');
if(collapseSurvive(t)||t.collapseRolled)throw Error('intact room escaped');
grid.fill(0);
if(!collapseSurvive(t)||t.collapseRolled!==2||t.hp!==2||!t.inv||!t.stun||t.vy!==0||t.armor)throw Error('lucky 3-HP escape');
if(!texts.includes('SOMEHOW ALIVE!'))throw Error('no visible escape feedback');
if(collapseSurvive(t))throw Error('second free escape');
const t2={...t,hp:1,collapseRolled:0,armor:0,roomId:0};
chanceRoll=.999999;
if(collapseSurvive(t2)||t2.collapseRolled!==1)throw Error('failed roll did not consume chance');
chanceRoll=0;
if(collapseSurvive(t2))throw Error('unlucky escape rerolled');
const t3={...t,hp:1,collapseRolled:0,roomId:-1};
if(collapseSurvive(t3))throw Error('outdoors escaped a collapsing room');
const t4={...t,hp:1,collapseRolled:0,roomId:0};
for(let y=17;y<=21;y++)for(let x=21;x<=27;x++)grid[idx(x,y)]=1;
if(collapseSurvive(t4))throw Error('solid entombment gave free escape');
'web collapse survivor: intact frame, HP injury, survivor announcement, trapped pocket, once-only odds PASSED';
`;
const result=vm.runInNewContext(program,{});
assert(result.includes('PASSED'));
console.log(result);
