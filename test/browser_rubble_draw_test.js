/* Execute the exact in-game rubble drawing loop with a headless Canvas
 * transform spy. No browser/DOM dependencies and no simulated game physics.
 * Assert every potentially visible sprite stays in the same paint order. */
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const src=fs.readFileSync(require('path').join(__dirname,'../web/index.html'),'utf8');
const a=src.indexOf('  /* At high zoom, most collapsed rubble');
// Stop before the target draw setup, not just before its actor loop.
// The final-landlord locator now precomputes its objective state between
// rubble and actor painting; this isolated rubble test has no HUD globals.
const marker=src.indexOf('  const lastTargetVisible=',a);
const fallback=src.indexOf('  for(const t of targets)if(t.alive)',a);
const b=marker>=0?marker:fallback;
assert(a>=0&&b>a,'missing production optimized rubble renderer');
const live=src.slice(a,b);
const program=`
const V=8;
let W=960,H=640,CAM={x:285,y:95,z:1.8},camOn=true,shake=0,shakeOption=1;
let shakeX=0,shakeY=0;
const shakeScale=()=>shakeOption;const drawRubbleCached=()=>false;const rubbleRenderPose=ch=>ch;
const MAT=Array.from({length:9},(_,i)=>({hp:80}));
const SPR=Array.from({length:9},(_,i)=>Array.from({length:4},(_,v)=>
  Array.from({length:3},(_,d)=>({id:i+'/'+v+'/'+d}))));
let visible=[],count=0,stack=[],pose={x:0,y:0,angle:0};
const ctx={
  reset(){visible=[];count=0;stack=[];pose={x:shakeX,y:shakeY,angle:0}},
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
let chunks=[];
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
    cells.push({m:1+(i%3),h:[16,40,80][i%3],lx,ly,wx:cx,wy:cy,dead:i%19===0});
  }
  chunks.push({x,y,rad:radius+V,ang:angle,cells});
}
const scattered=chunks.slice();
// Tiny rotating chunks just beyond every viewport edge, including the
// corners. Large shake proves the culling margin covers the displacement
// itself instead of only surviving because of the existing voxel padding.
function edgeFixtures(){
  chunks=scattered.slice();
  const x0=CAM.x,x1=CAM.x+W/CAM.z,y0=CAM.y,y1=CAM.y+H/CAM.z;
  for(const d of [-72,-38,-25,-18,-8,-.5,.5,8,18,25,38,72]){
    for(const [x,y] of [[x0+d,(y0+y1)/2],[x1+d,(y0+y1)/2],
                       [(x0+x1)/2,y0+d],[(x0+x1)/2,y1+d],
                       [x0+d,y0+d],[x1+d,y1+d]]){
      for(const ang of [0,Math.PI/4,Math.PI/2,Math.PI*.83])
        chunks.push({x,y,rad:V,ang,cells:[{m:1,h:80,lx:0,ly:0,wx:x,wy:y}]});
    }
  }
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
shake=16;shakeX=7.99;shakeY=-7.99;const shaken=check('screen shake');
if(shaken.drawn>shaken.base*.65)
  throw Error('screen shake must retain useful offscreen rubble culling');
let cases=0;
for(const [width,height] of [[960,540],[540,960],[540,1240]])
for(const [x,y,z] of [[285,95,1.8],[0,-120,1],[1800,65,.55]]){
  W=width;H=height;CAM={x,y,z};edgeFixtures();
  for(const amplitude of [0,1e-12,4,16,128])for(const option of [0,.5,1]){
    shake=amplitude;shakeOption=option;
    for(const dx of [-.5,0,.499999])for(const dy of [-.5,0,.499999]){
      shakeX=dx*shake*shakeOption;shakeY=dy*shake*shakeOption;
      check('viewport '+width+'x'+height+' camera '+x+','+y+','+z+
            ' shake '+amplitude+' option '+option+' direction '+dx+','+dy);
      cases++;
    }
  }
}
shake=16;camOn=false;const full=check('unzoomed play');
if(full.base!==full.drawn)
  throw Error('full-world rendering must remain untouched');
'web rubble camera culling: '+zoom.base+' reference calls -> '+zoom.drawn+
  ' with zoom / '+shaken.drawn+' during shake; '+cases+' edge/layout/shake cases PASSED';
`;
const result=vm.runInNewContext(program,{});
assert(result.includes('PASSED'),result);
console.log(result);
