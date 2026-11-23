/* Real chunk solver: dead cells must stop causing contacts immediately.
 * The frozen pre-fix solver is an independent oracle for live-cell scenes. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const start=html.indexOf('function resolveStatic('),end=html.indexOf('function cleanChunks(',start);
assert(start>=0&&end>start,'production chunk contact solver exists');
const production=html.slice(start,end);
const material=html.slice(html.indexOf('const MAT=[null,'),html.indexOf('// ---------- state ----------'));
const reference=String.raw`function resolveStatic(ch,a,nx,ny,pen,ci,cj){
  const rx=a.wx-ch.x,ry=a.wy-ch.y,vpx=ch.vx-ch.w*ry,vpy=ch.vy+ch.w*rx,vn=vpx*nx+vpy*ny;
  if(vn<0){const MP=MAT[a.m],rn=rx*ny-ry*nx,den=1/ch.m+rn*rn/ch.I,e=-vn>(a.m===RUBBER?24:130)?MP.rest:0,j=-(1+e)*vn/den;
    ch.vx+=j*nx/ch.m;ch.vy+=j*ny/ch.m;ch.w+=rn*j/ch.I;
    const tx=-ny,ty=nx,vt=(ch.vx-ch.w*ry)*tx+(ch.vy+ch.w*rx)*ty,rt=rx*ty-ry*tx,jt=clamp(-vt/(1/ch.m+rt*rt/ch.I),-MP.fric*j,MP.fric*j);
    ch.vx+=jt*tx/ch.m;ch.vy+=jt*ty/ch.m;ch.w+=rt*jt/ch.I;
    if(-vn>(a.m===GLASS?72:170))impactDamage(ch,a,ci,cj,-vn);
    if(-vn>56){const kf=clamp(ch.m/200,.03,.4);wobKick(ci,cj,2,nx*vn/V*kf,ny*vn/V*kf)}}   // the thing that got hit wobbles
  ch.cx+=nx*pen;ch.cy+=ny*pen;ch.cn++}
function impactDamage(ch,a,ci,cj,vn){
  const dmg=(vn-(a.m===GLASS?57:160))*.07;if(inb(ci,cj)&&grid[idx(ci,cj)])damageCell(ci,cj,dmg*.9);a.h-=dmg*.6*MAT[a.m].brit;
  if(a.h<=0){a.dead=true;const c=MAT[a.m].c;for(let k=0;k<3;k++)chip(a.wx,a.wy,c,(rnd()-.5)*200,-rnd()*150);dust(a.wx,a.wy,1,7)}
  if(vn>250)lordShock(a.wx,a.wy,8*V,.45);SFX.thud(vn*ch.m/3000);shake=Math.max(shake,Math.min(6,vn*ch.m/5000));if(rnd()<.3)dust(a.wx,a.wy+4,1,8)}
function stepChunks(dt){
  if(!chunks.length)return;
  const map=new Map();
  for(const ch of chunks){if(!ch.rest){ch.vy+=G*dt;ch.vx*=.9995;ch.w*=.998;ch.x+=ch.vx*dt;ch.y+=ch.vy*dt;ch.ang+=ch.w*dt;ch.cs=Math.cos(ch.ang);ch.sn=Math.sin(ch.ang)}ch.cx=ch.cy=0;ch.cn=0;
    for(const a of ch.cells){if(!ch.rest){a.wx=ch.x+ch.cs*a.lx-ch.sn*a.ly;a.wy=ch.y+ch.sn*a.lx+ch.cs*a.ly}const k=(Math.floor(a.wx/V)+4)*1024+(Math.floor(a.wy/V)+4);let b=map.get(k);if(!b){b=[];map.set(k,b)}b.push(a)}}
  for(const ch of chunks){
    for(const a of ch.cells){
      const gx=Math.floor(a.wx/V),gy=Math.floor(a.wy/V);
      for(let j=gy-1;j<=(ch.rest?gy-2:gy+1);j++)for(let i=gx-1;i<=gx+1;i++){
        if(!(j>=ROWS||(inb(i,j)&&grid[idx(i,j)])))continue;
        const bx=i*V,by=j*V,qx=clamp(a.wx,bx,bx+V),qy=clamp(a.wy,by,by+V),dx=a.wx-qx,dy=a.wy-qy,d2=dx*dx+dy*dy;
        if(d2>=RC*RC)continue;let nx,ny,pen;
        if(d2>1e-6){const d=Math.sqrt(d2);nx=dx/d;ny=dy/d;pen=RC-d}
        else{const l=a.wx-bx,r=bx+V-a.wx,t=a.wy-by,b=by+V-a.wy,mn=Math.min(l,r,t,b);
          if(mn===t){nx=0;ny=-1;pen=t+RC}else if(mn===l){nx=-1;ny=0;pen=l+RC}else if(mn===r){nx=1;ny=0;pen=r+RC}else{nx=0;ny=1;pen=b+RC}}
        resolveStatic(ch,a,nx,ny,Math.min(pen,4),i,j)}
      // chunk vs chunk (circle-circle)
      for(let j=gy-1;j<=gy+1;j++)for(let i=gx-1;i<=gx+1;i++){const b=map.get((i+4)*1024+(j+4));if(!b)continue;
        for(const o of b){const oc=o.ch;if(oc.id<=ch.id)continue;const dx=a.wx-o.wx,dy=a.wy-o.wy,d2=dx*dx+dy*dy;if(d2>=V*V||d2<1e-6)continue;
          const d=Math.sqrt(d2),nx=dx/d,ny=dy/d,pen=V-d,rax=a.wx-ch.x,ray=a.wy-ch.y,rbx=o.wx-oc.x,rby=o.wy-oc.y;
          const vax=ch.vx-ch.w*ray,vay=ch.vy+ch.w*rax,vbx=oc.vx-oc.w*rby,vby=oc.vy+oc.w*rbx,vn=(vax-vbx)*nx+(vay-vby)*ny;
          if(vn<0){const rna=rax*ny-ray*nx,rnb=rbx*ny-rby*nx,den=1/ch.m+1/oc.m+rna*rna/ch.I+rnb*rnb/oc.I,jj=-(1.1)*vn/den;
            ch.vx+=jj*nx/ch.m;ch.vy+=jj*ny/ch.m;ch.w+=rna*jj/ch.I;oc.vx-=jj*nx/oc.m;oc.vy-=jj*ny/oc.m;oc.w-=rnb*jj/oc.I;
            if(-vn>200){a.h-=(-vn-180)*.04;o.h-=(-vn-180)*.04;if(a.h<=0)a.dead=true;if(o.h<=0)o.dead=true;SFX.thud(.2)}}
          const tm=ch.m+oc.m;ch.cx+=nx*pen*.5*oc.m/tm;ch.cy+=ny*pen*.5*oc.m/tm;ch.cn++;oc.cx-=nx*pen*.5*ch.m/tm;oc.cy-=ny*pen*.5*ch.m/tm;oc.cn++}}
    }}
  for(const ch of chunks){if(ch.cn>0&&!ch.rest){ch.x+=ch.cx/ch.cn*.9;ch.y+=ch.cy/ch.cn*.9}ch.age+=dt}
}
`;
const harness=String.raw`
const V=8,RC=4,ROWS=60,COLS=120,G=900;
const grid=new Uint8Array(COLS*ROWS);
const idx=(x,y)=>y*COLS+x,inb=(x,y)=>x>=0&&x<COLS&&y>=0&&y<ROWS;
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
const events=[],chunks=[];
let shake=0,seed=0x31415926;
const rnd=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296};
const chip=(...a)=>events.push(['chip',...a]),dust=(...a)=>events.push(['dust',...a]);
const damageCell=(...a)=>events.push(['damage',...a]);
const wobKick=(...a)=>events.push(['wobble',...a]);
const lordShock=(...a)=>events.push(['shock',...a]);
// The actual browser physics now wakes a sleeping piece after a hard hit.
const wake=ch=>{ch.rest=false;ch.sleep=0;ch.restT=0;ch.age=Math.max(ch.age,.2)};
const SFX={thud(...a){events.push(['thud',...a])}};
for(const [x,y,m]of fixture.walls||[])grid[idx(x,y)]=m;
for(const d of fixture.chunks){
 const c=Object.assign({vx:0,vy:0,w:0,ang:0,m:1,I:32,age:0,rest:false},d);
 c.cs=Math.cos(c.ang);c.sn=Math.sin(c.ang);
 c.cells=d.cells.map(a=>Object.assign({m:1,h:1e9,lx:0,ly:0,dead:false},a));
 for(const a of c.cells){a.ch=c;a.wx=c.x+c.cs*a.lx-c.sn*a.ly;a.wy=c.y+c.sn*a.lx+c.cs*a.ly}
 chunks.push(c);
}
`;
const finish=String.raw`
for(let i=0;i<steps;i++)stepChunks(dt);
// The old solver predates TNT fuse/impact metadata. Compare physics state,
 // collision contacts, HP, effects and body transforms; the separate
 // browser_mobile_tnt_test.js checks those new per-charge fields directly.
 return {chunks:chunks.map(({tntHit,tntImpact,...c})=>({...c,
   cells:c.cells.map(({ch,...a})=>a)})),events,shake};
`;
const make=source=>new Function('fixture','dt','steps',material+harness+source+finish);
const run=make(production),old=make(reference);
const body=(id,x,y,extra={},cell={})=>({id,x,y,...extra,cells:[cell]});
let cases=0,baselineFailures=0;
function check(name,fixture,verify){
 const current=run(fixture,0,1);
 try{verify(current)}catch(error){error.message=name+': '+error.message;throw error}cases++;
 try{verify(old(fixture,0,1))}catch(_){baselineFailures++}
}
check('already-dead ground contact',{chunks:[body(1,204,478,{vy:100},{dead:true,h:0})]},r=>{
 assert.strictEqual(r.chunks[0].cn,0,'dead source collided with the ground');
 assert.strictEqual(r.chunks[0].vy,100,'dead source changed body velocity');
 assert.strictEqual(r.events.length,0,'dead source emitted impact feedback');
});
check('already-dead pair target',{chunks:[
 body(1,200,200,{vx:120}),body(2,206,200,{}, {dead:true,h:0})
]},r=>{
 assert.strictEqual(r.chunks[0].cn,0,'live body collided with an invisible target');
 assert.strictEqual(r.chunks[1].vx,0,'dead target received a ghost impulse');
});
check('resting dead target',{chunks:[
 body(1,200,200,{vx:120}),body(2,206,200,{rest:true},{dead:true,h:0})
]},r=>assert.strictEqual(r.chunks[0].cn,0,'resting shattered debris still acts as support'));
check('source dies on first of two floor contacts',{chunks:[
 body(1,200,478,{vy:300},{m:4,h:.01})
]},r=>{
 assert(r.chunks[0].cells[0].dead,'fixture must fracture on its real first impact');
 assert.strictEqual(r.chunks[0].cn,1,'fatal static contact must finish, then stop');
 assert.strictEqual(r.events.filter(e=>e[0]==='thud').length,1,'shattered glass repeated impact sound');
});
check('static fracture before pair scan',{chunks:[
 body(1,204,478,{vx:200,vy:300},{m:4,h:.01}),body(2,210,478)
]},r=>{
 assert(r.chunks[0].cells[0].dead,'source must break against actual ground');
 assert.strictEqual(r.chunks[1].vx,0,'source collided with next body after its ground fracture');
});
check('pair fracture before third body',{chunks:[
 body(1,200,200,{vx:500,vy:500},{m:4,h:.01}),body(2,207,200),body(3,200,207)
]},r=>{
 assert(r.chunks[0].cells[0].dead,'first body must fracture in actual pair impulse');
 assert(r.chunks[1].vx>0,'fatal contact must retain its valid impulse');
 assert.strictEqual(r.chunks[0].cn,1,'source kept generating pair contacts after fracture');
 assert.strictEqual(r.chunks[2].cn,0,'third body contacted an already shattered source');
 assert.strictEqual(r.chunks[2].vy,0,'third body received a ghost impulse');
});
check('target dies after hash insertion',{chunks:[
 body(1,200,200,{vx:500}),body(2,207,207,{vy:-100}),body(3,207,200,{}, {m:4,h:.01})
]},r=>{
 assert(r.chunks[2].cells[0].dead,'first body must destroy the later hash target');
 assert.strictEqual(r.chunks[1].cn,0,'stale hash entry collided after becoming dead');
 assert.strictEqual(r.chunks[1].vy,-100,'stale hash entry deflected another live body');
});
assert.strictEqual(baselineFailures,cases,'every regression must demonstrate an original failure');

// Cleanup intentionally remains outside stepChunks, just as in the six
// production substeps. Dead-only bodies must stay noncolliding throughout.
const dead={chunks:[body(1,200,478,{vy:300},{dead:true,h:0})]};
const six=run(dead,1/360,6);
assert.strictEqual(six.chunks[0].cn,0);
assert.strictEqual(six.events.length,0);
assert.strictEqual(six.chunks[0].cells.length,1,'solver must leave cleanup timing intact');

// Seeded all-moving live-cell worlds compare complete bodies, HP, transforms,
// impulses, position correction and RNG-driven dust against the old code.
// Intentional changes to resting-body response have their own regression in
// browser_collapse_response_test.js.
let seed=0x78ca32ee;
const rnd=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296};
let liveCases=0;
for(let i=0;i<600;i++){
 const chunks=[];
 for(let j=0;j<4;j++){
  const cells=Array.from({length:3},(_,k)=>({m:1+(i+j+k)%8,h:1e9,lx:(k-1)*8,ly:(k%2)*8}));
  chunks.push({id:j+1,x:160+rnd()*48,y:450+rnd()*30,vx:(rnd()-.5)*180,vy:(rnd()-.5)*180,
   w:(rnd()-.5)*3,ang:(rnd()-.5)*3,m:2+rnd()*5,I:200+rnd()*300,
   rest:false,cells});
 }
 const fixture={chunks,walls:[[22,58,3],[24,57,2],[25,59,1]]};
 assert.deepStrictEqual(run(fixture,1/360,6),old(fixture,1/360,6),'live contact behavior changed in case '+i);
 liveCases++;
}
console.log('WEB_DEAD_RUBBLE_PASS original_failures='+baselineFailures+' regressions='+cases+
 ' live_worlds='+liveCases+' substeps=6');
