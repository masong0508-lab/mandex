/* Execute the exact in-game rubble drawing loop with a headless Canvas
 * transform spy. No browser/DOM dependencies and no simulated game physics.
 * Assert every potentially visible sprite stays in the same paint order. */
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const src=fs.readFileSync(require('path').join(__dirname,'../web/index.html'),'utf8');
const a=src.indexOf('  /* At high zoom, most collapsed rubble');
const b=src.indexOf('  for(const t of targets)if(t.alive)',a);
assert(a>=0&&b>a,'missing production optimized rubble renderer');
const live=src.slice(a,b);
const program=`
const W=960,H=640,V=8;
let CAM={x:285,y:95,z:1.8},camOn=true,shake=0;
const MAT=Array.from({length:9},(_,i)=>({hp:80}));
const SPR=Array.from({length:9},(_,i)=>Array.from({length:4},(_,v)=>
  Array.from({length:3},(_,d)=>({id:i+'/'+v+'/'+d}))));
let visible=[],count=0,stack=[],pose={x:0,y:0,angle:0};
const ctx={
  reset(){visible=[];count=0;stack=[];pose={x:0,y:0,angle:0}},
  save(){stack.push({...pose})},
  restore(){pose=stack.pop()},
  translate(x,y){pose.x+=x;pose.y+=y},
  rotate(a){pose.angle+=a},
  drawImage(img,x,y){
    count++;
    const dx=x+V/2,dy=y+V/2,c=Math.cos(pose.angle),s=Math.sin(pose.angle);
    const wx=pose.x+dx*c-dy*s,wy=pose.y+dx*s+dy*c;
    /* A rotated 8px sprite cannot extend beyond 6px of its centre. */
    const x0=CAM.x,x1=CAM.x+W/CAM.z,y0=CAM.y,y1=CAM.y+H/CAM.z;
    if(!camOn||wx+6>=x0&&wx-6<=x1&&wy+6>=y0&&wy-6<=y1)
      visible.push(img.id+':'+wx.toFixed(2)+':'+wy.toFixed(2));
  }
};
let seed=0x53171abcd;
const rnd=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296};
const chunks=[];
for(let k=0;k<64;k++){
  const x=k<7?310+rnd()*390:rnd()*2600-700;
  const y=k<7?140+rnd()*230:rnd()*1400-400;
  const angle=(rnd()-.5)*6.28;
  const cells=[];
  let radius=0;
  for(let i=0;i<25;i++){
    const lx=(rnd()-.5)*130,ly=(rnd()-.5)*110;
    radius=Math.max(radius,Math.hypot(lx,ly));
    const cx=x+lx*Math.cos(angle)-ly*Math.sin(angle);
    const cy=y+lx*Math.sin(angle)+ly*Math.cos(angle);
    cells.push({m:1+(i%3),h:80,lx,ly,wx:cx,wy:cy,dead:i%19===0});
  }
  chunks.push({x,y,rad:radius+V,ang:angle,cells});
}
function reference(){
  for(const ch of chunks){
    ctx.save();ctx.translate(ch.x,ch.y);ctx.rotate(ch.ang);
    for(const a of ch.cells){
      if(a.dead)continue;
      const r=a.h/MAT[a.m].hp;
      ctx.drawImage(SPR[a.m][(a.lx*7+a.ly*3|0)&3][r>.66?0:r>.33?1:2],-V/2+a.lx,-V/2+a.ly);
    }
    ctx.restore();
  }
}
function optimized(){
`+live+`
}
function check(label){
  ctx.reset();reference();
  const expected=visible.slice(),base=count;
  ctx.reset();optimized();
  if(JSON.stringify(visible)!==JSON.stringify(expected))
    throw Error(label+': lost/reordered a fragment visible on the canvas');
  if(count>base)throw Error(label+': optimized path added draw calls');
  return {base,drawn:count};
}
const zoom=check('zoom');
if(zoom.base<1200||zoom.drawn>zoom.base*0.65)
  throw Error('zoom failed to skip enough offscreen rubble to matter');
shake=4;const shaken=check('screen shake');
if(shaken.base!==shaken.drawn)
  throw Error('screen shake must bypass rubble culling');
shake=0;camOn=false;const full=check('unzoomed play');
if(full.base!==full.drawn)
  throw Error('full-world rendering must remain untouched');
'web rubble camera culling: '+zoom.base+' reference calls -> '+zoom.drawn+' with zoom; edge visibility and shake parity PASSED';
`;
const result=vm.runInNewContext(program,{});
assert(result.includes('PASSED'),result);
console.log(result);
