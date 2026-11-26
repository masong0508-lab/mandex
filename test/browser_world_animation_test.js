'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
function worldAnimationRegression(html,assert,section){
 const cut=(a,b)=>{const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,'missing source '+a);return html.slice(i,j)};
 const near=(a,b,msg)=>assert(Math.abs(a-b)<1e-8,msg+': '+a+' vs '+b);
 if(!section||section==='pad'){
  const h=new Function(`
   const V=8,RUBBER=7,COLS=120,ROWS=60,grid=new Uint8Array(7200),pads=[{x:80,y:160,w:32}];
   const idx=(x,y)=>y*COLS+x,inb=(x,y)=>x>=0&&x<COLS&&y>=0&&y<ROWS;
   let time=0,man={mode:'aim'},draws=0,points=[];
   const ctx={save(){},restore(){},beginPath(){},moveTo(x,y){points.push([x,y])},lineTo(x,y){points.push([x,y])},stroke(){draws++}};
   ${cut('function drawPads(){','function drawTubes(){')}
   return {grid,idx,pads,draw:()=>{draws=0;points=[];drawPads();return {draws,points}},time:v=>{time=v}};
  `)();
  assert(h.draw().draws===0,'destroyed rubber pad must not advertise bounce');
  h.grid[h.idx(10,20)]=7;h.grid[h.idx(11,20)]=7;
  const first=h.draw();assert(first.draws===3,'live pad gets three moving chevrons');
  assert(first.points.filter((_,i)=>i%3===1).every(p=>p[0]===88),'remaining rubber determines marker center');
  h.time(.15);const second=h.draw();assert(JSON.stringify(second.points)!==JSON.stringify(first.points),'live chevrons travel upward');
  h.grid[h.idx(11,20)]=0;h.grid[h.idx(13,20)]=7;
  const split=h.draw();
  assert(split.points.filter((_,i)=>i%3===1).every(p=>p[0]===84),'split pad must point to an actual surviving run, not the hole');
  h.grid[h.idx(12,20)]=7;
  const widest=h.draw();
  assert(widest.points.filter((_,i)=>i%3===1).every(p=>p[0]===104),'widest surviving run takes priority');
  assert(widest.points.every(p=>p[0]>=96&&p[0]<=112),'narrowed marker stays within surviving rubber width');
 }
 if(!section||section==='tube'){
  assert(html.includes('function tubePulse('),'tube transfer pulse exists');
  const h=new Function(`
   let time=10,shake=0;const tubes=[{ax:0,ay:0,bx:300,by:250,r:40,ang:0,sp:760,col:'#fff'}];
   const ring=()=>{},popText=()=>{},SFX={whoosh(){},pop(){}};
   ${cut('function flightFaceFor(','function drawPads(){')}
   return {tubes,pulse:tubePulse,tick:m=>tubeStep(m,1/60),clock:v=>{time=v}};
  `)();
  const tb=h.tubes[0];near(h.pulse(tb),0,'no idle transfer glow');
  const m={x:0,y:0,vx:0,vy:0,tubeT:0,tubeCd:0,ang:1,face:1};
  h.tick(m);near(m.x,300,'teleport physics unchanged');assert(h.pulse(tb)>.9,'real tube use starts pulse');
  h.clock(10.2);assert(h.pulse(tb)>0&&h.pulse(tb)<.9,'pulse fades');h.clock(11);near(h.pulse(tb),0,'pulse expires');
  assert(html.includes('tubePulse(tb)'),'production tube effect reads the pulse');
 }
 if(!section||section==='tnt'){
  assert(html.includes('function fuseGlow('),'fuse animation exists');
  const f=new Function(cut('function fuseGlow(','function drawFuseGlow(')+'return fuseGlow;')();
  near(f(0),0,'no fuse glow when inactive');
  assert(f(2)>f(23),'near detonation is visibly brighter');
  assert(f(23)>0&&f(1)<=1,'armed fuse pulse bounded');
  const draw=html.slice(html.indexOf('  /* At high zoom, most collapsed rubble'),html.indexOf('  const lastTargetVisible='));
  assert(draw.includes('if(a.fuse>0&&a.m===TNT)drawFuseGlow(a)'),'visible moving TNT consumes real cell fuse timer');
 }
 if(!section||section==='shock'){
  const h=new Function(`
   const parts=[],texts=[];let time=0,arcs=[],opacity=[];
   const ctx={globalAlpha:1,beginPath(){},arc(x,y,r){arcs.push(r);opacity.push(this.globalAlpha)},stroke(){}};
   ${cut('function drawParts(','function drawTexts(){')}
   return {parts,arcs,opacity,draw:drawParts};
  `)();
  const p={k:5,x:20,y:20,s:80,l:1,l0:1,c:'#fff'};h.parts.push(p);
  h.draw();near(h.arcs.at(-1),0,'shockwave starts at center');
  p.l=.75;h.draw();const r1=h.arcs.at(-1);assert(r1>20&&r1<80,'shockwave rapidly expands at impact');
  p.l=.25;h.draw();const r2=h.arcs.at(-1);assert(r2>r1&&r2-r1<40,'shockwave decelerates as it dissipates');
  assert(h.opacity.at(-1)<.25,'shockwave fades more gently at tail');near(p.l,.25,'draw never changes lifetime');
 }
 return 'WEB_WORLD_ANIMATION_PASS '+(section||'pads tubes tnt shockwaves');
}
console.log(worldAnimationRegression(fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8'),assert));
