'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
function expansionRegression(html,assert,section){
 const cut=(a,b)=>{const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i,'source section '+a);return html.slice(i,j)};
 if(!section||section==='starters'){
   const create=new Function(`
     const ROWS=60,EMIN=28,COLS=120;const E={g:new Uint8Array(7200),t:[],men:5,undo:[],redo:[],starters:true,id:1,name:'Original'};
     const toast=()=>{},buttons=[],panel=()=>{},txt=()=>{},btn=(x,y,w,h,n,fn)=>buttons.push({x,y,w,h,n,fn});
     let starterPage=0;
     ${cut('function encodeCells(','function encodeSharedLevel(')}
     ${cut('function eState(){','function eCell(')}
     return {E,buttons,patterns:E_STARTERS,pattern:eStarterPattern,apply:eApplyStarter,undo:eUndo,draw:drawStarterMenu,validate:validateSharedLevel};
   `)();
   assert(create.patterns.length===18,'eighteen starter structures');
   const seen=new Set();
   for(let kind=0;kind<create.patterns.length;kind++){
     const a=create.pattern(kind),b=create.pattern(kind);
     assert(JSON.stringify(a.level)===JSON.stringify(b.level),'deterministic starter');
     assert(JSON.stringify(a.level)===JSON.stringify(create.validate(a.level)),'shareable starter');
     seen.add(JSON.stringify(a.level.c));
     if(kind>=3){
       for(const [cx,row] of a.level.t){
         for(let y=row-5;y<row;y++)for(let x=cx-2;x<=cx+2;x++)assert(!a.grid[y*120+x],'new starter landlord clearance');
         assert(row===60||a.grid[row*120+cx]>0,'new starter landlord has floor');
       }
       assert(a.grid.slice(59*120).some(x=>x!==0),'new starter grounded foundation');
       if(kind>=6){
         const lim=[0,6,9,14,2,2,16,8,8],span=new Uint8Array(7200);span.fill(255);
         const q=[];for(let x=0;x<120;x++){const i=59*120+x;if(a.grid[i]){span[i]=0;q.push(i)}}
         for(let k=0;k<q.length;k++){const i=q[k],x=i%120,y=i/120|0;
           for(const [dx,dy] of [[1,0],[-1,0],[0,1],[0,-1]]){
             if(x+dx<0||x+dx>=120||y+dy<0||y+dy>=60)continue;
             const j=i+dx+dy*120,m=a.grid[j],n=dy===-1?0:span[i]+1;
             if(m&&n<span[j]&&n<=lim[m]){span[j]=n;q.push(j)}
           }
         }
         for(let i=0;i<7200;i++)assert(!a.grid[i]||span[i]!==255,'unsupported cell in new starter '+kind+' at '+i);
       }
     }
     create.apply(kind);assert(create.E.id===-1,'new design cannot overwrite old project');create.undo();assert(create.E.id===1,'undo restores project identity');
   }
   assert(seen.size===18,'eighteen distinct structures');
   const all=new Set();
   for(let p=0;p<3;p++){
     create.buttons.length=0;create.draw(0,0,416);
     for(const b of create.buttons){
       assert(b.x>=0&&b.x+b.w<=416&&b.y+b.h<=290,'desktop starter buttons fit');
       if(create.patterns.includes(b.n))all.add(b.n);
     }
     const next=create.buttons.find(b=>b.n==='NEXT >');assert(next,'next starter page exists');next.fn();
   }
   assert(all.size===18,'all 18 designs reachable in desktop picker');
   create.buttons.length=0;create.draw(0,0,516,216);
   for(const b of create.buttons)assert(b.x>=0&&b.x+b.w<=516&&b.y+b.h<=216,'portrait starter picker fits minimum height');
   assert(create.buttons.filter(b=>create.patterns.includes(b.n)).length===6,'portrait shows six choices per page');
 }
 if(!section||section==='particles'){
   const run=new Function(`
     const parts=[],texts=[],draws=[];let time=0;
     const ctx=new Proxy({}, {get(t,k){if(k in t)return t[k];return (...a)=>draws.push([k,...a])},set(t,k,v){t[k]=v;return true}});
     const txt=(...a)=>draws.push(['txt',...a]);
     ${cut('function drawParts(','function drawTexts(){')}
     return {parts,draws,draw:drawParts};
   ` .replace("undefined","undefined"));
   const p=run();
   p.parts.push({k:0,x:50,y:50,s:4,l:.5,l0:1,c:'red'},{k:0,x:10000,y:50,s:4,l:.5,l0:1,c:'red'},{k:2,x:140,y:50,s:3,vx:1800,vy:0,l:.5,l0:1});
   p.draw();const all=p.draws.slice();p.draws.length=0;
   p.draw(0,100,0,100);const trimmed=p.draws.slice();
   assert(all.filter(x=>x[0]==='fillRect').length===2,'unbounded title effects unchanged');
   assert(trimmed.filter(x=>x[0]==='fillRect').length===1,'offscreen particle skipped');
   assert(trimmed.some(x=>x[0]==='lineTo'&&x[1]<100),'long spark crossing edge remains visible');
   assert(p.parts.length===3&&p.parts[1].x===10000,'render culling never changes simulation');
 }
 if(!section||section==='animation'){
   const make=new Function(`
     const V=8,G=800,WHH=19,WHW=7,WW=960,RUBBER=7,KEYS={},targets=[],grid=new Uint8Array(7200);
     const idx=(x,y)=>y*120+x,inb=()=>false,clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
     const wBlocked=(x,y)=>y>250,rnd=()=>1,SFX={pop(){},thud(){},whoosh(){}};
     const dust=()=>{},popText=()=>{},ring=()=>{},walkQuit=()=>{},walkPunch=()=>{},walkSlam=()=>{},killTarget=()=>{};
     let time=0,man={mode:'walk',x:120,y:245,vy:420,vx:0,gr:false,wt:600,face:1,turn:0,gait:0,stomp:false,pCd:0,pT:0,padCd:0,ledgeGrace:0,jumpHold:0};
     ${cut('function walkStep(dt){','/* Match the console sprite turnaround:')}
     ${cut('function walkTurnAngle(','function drawWalkMan(m){')}
     return {man,tick:walkStep,squash:()=>typeof walkLandingSquash==='function'?walkLandingSquash(man):0,pitch:()=>typeof walkAirPitch==='function'?walkAirPitch(man):null};
   `)();
   make.tick(1/60);assert(make.man.gr&&make.man.vy===0,'physical hard landing still resolves');
   const start=make.squash();assert(start>0&&start<=1,'hard landing starts bounded squash');
   for(let i=0;i<12;i++)make.tick(1/60);
   assert(make.squash()===0,'landing recovers to neutral');
   make.man.gr=false;make.man.vy=-1;const before=make.pitch();make.man.vy=1;const after=make.pitch();
   assert(before!==null&&after!==null&&Math.abs(after-before)<.01,'smooth air pitch through apex');
 }

 if(!section||section==='wallpaper'){
   const original=html.includes('function drawWallpaper(');
   assert(original,'camera bounded wallpaper renderer is missing');
   const render=new Function(`
     let COLS=360;const ROWS=60,V=8;let paper=new Uint8Array(COLS*ROWS),grid=new Uint8Array(COLS*ROWS),CEL_ACTIVE=false,paperTorn=false;
     const interiors=Array.from({length:5},(_,i)=>({p:'room'+i})),celRoomColor=p=>'cel-'+p;
     let calls=[],lo=0,hi=COLS,reads=0;const idx=(x,y)=>y*COLS+x;
     const ctx={fillStyle:'',fillRect(x,y,w,h){calls.push([this.fillStyle,x,y,w,h])}};
     ${cut('function drawWallpaper(','function drawRoomDetails(')}
     function reference(xlo,xhi,ylo,yhi){
       for(let y=ylo;y<yhi;y++){let x=0;while(x<COLS){const n=paper[idx(x,y)];if(!n||grid[idx(x,y)]){x++;continue}let x2=x;while(x2<COLS&&paper[idx(x2,y)]===n&&!grid[idx(x2,y)])x2++;
         if(x2<=xlo||x>=xhi){x=x2;continue}
         ctx.fillStyle=CEL_ACTIVE?celRoomColor(interiors[n-1].p):interiors[n-1].p;ctx.fillRect(x*V,y*V,(x2-x)*V,V);
         ctx.fillStyle='rgba(0,0,0,.07)';for(let k=0;k<(x2-x)*V;k+=16)ctx.fillRect(x*V+k,y*V,8,V);
         if(!paper[idx(x,y+1)]||y+1>=ROWS){ctx.fillStyle='rgba(0,0,0,.18)';ctx.fillRect(x*V,(y+1)*V-5,(x2-x)*V,5)}
         if(paperTorn){ctx.fillStyle='rgba(70,47,40,.48)';if(x>0&&!paper[idx(x-1,y)])ctx.fillRect(x*V,y*V,2,V);
           if(x2<COLS&&!paper[idx(x2,y)])ctx.fillRect(x2*V-2,y*V,2,V)}
         x=x2}}
     }
     let seed=781;
     const rnd=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296};
     function fixture(width,pattern){
       COLS=width;const p=new Uint8Array(COLS*ROWS),g=new Uint8Array(COLS*ROWS);
       for(let y=0;y<ROWS;y++)for(let x=0;x<COLS;x++){
         const i=y*COLS+x;p[i]=pattern===0?1:pattern===1?(x<20||x>COLS-20?0:1+(x/31|0)%5):rnd()<.12?0:1+(x/13|0)%5;
         if(pattern===2&&rnd()<.13)g[i]=2;
       }
       // Count property reads through the exact array index path.
       paper=new Proxy(p,{get(a,k){if(typeof k==='string'&&/^\\d+$/.test(k))reads++;return a[k]}});
       grid=new Proxy(g,{get(a,k){if(typeof k==='string'&&/^\\d+$/.test(k))reads++;return a[k]}});
     }
     function run(fn,x0,x1,y0,y1,cel,torn){
       calls=[];reads=0;CEL_ACTIVE=cel;paperTorn=torn;fn(x0,x1,y0,y1);
       // Production cull boundaries include two-cell padding; compare
       // every overlapping original paint rectangle in its original order.
       return {all:calls.length,reads,visible:calls.filter(a=>a[1]+a[3]>x0*V&&a[1]<x1*V)};
     }
     return {fixture,reference:(...a)=>run(reference,...a),optimized:(...a)=>run(drawWallpaper,...a)};
   `)();
   let cases=0,oldCalls=0,newCalls=0,oldReads=0,newReads=0;
   for(const width of [120,360])for(const pattern of [0,1,2]){
     render.fixture(width,pattern);
     for(const [x0,x1] of [[0,width],[1,20],[20,47],[Math.floor(width*.6),Math.floor(width*.6)+24],[width-21,width]])for(const cel of [false,true])for(const torn of [false,true]){
       const a=render.reference(x0,x1,3,58,cel,torn),b=render.optimized(x0,x1,3,58,cel,torn);
       assert(JSON.stringify(a.visible)===JSON.stringify(b.visible),'visible wallpaper paint order changed '+[width,pattern,x0,x1,cel,torn]);
       assert(b.all<=a.all,'extra wallpaper paint calls');
       if(x0!==0||x1!==width){oldCalls+=a.all;newCalls+=b.all;oldReads+=a.reads;newReads+=b.reads;}
       cases++;
     }
   }
   assert(newCalls<oldCalls*.6&&newReads<oldReads*.7,'camera culling must materially reduce paint and grid reads');
   console.log('WALLPAPER_CULL_PASS cases='+cases+' paint='+oldCalls+'->'+newCalls+' reads='+oldReads+'->'+newReads);
 }
 return 'WEB_EXPANSION_PASS '+(section||'starters particles animation');
}
console.log(expansionRegression(fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8'),assert));
