'use strict';
/* Validate all authored trials and both generated runtime representations. */
const assert=require('assert'),fs=require('fs'),path=require('path');
const root=path.join(__dirname,'..');
const pack=require('../tools/trial_pack.js');
const levels=pack.generateTrials();
const out=pack.emitTrials(levels);
assert.strictEqual(levels.length,1057);
assert.strictEqual(new Set(levels.map(l=>l.rows.join(''))).size,1057);
assert.strictEqual(levels.reduce((n,l)=>n+l.rows.join('').length,0),7610400);
assert(out.commandBytes<1057*7200/5,'compact command payload must fit handheld budget');
for(let d=0;d<53;d++){
 const source=fs.readFileSync(path.join(root,'content/trials/district-'+(d+1)+'.json'),'utf8');
 assert.strictEqual(source,out.districts[d],'uncompressed editable source matches generator');
}
assert.strictEqual(fs.readFileSync(path.join(root,'core/trial_levels.h'),'utf8'),out.header,'native data matches source');
const html=fs.readFileSync(path.join(root,'web/index.html'),'utf8');
assert(html.includes(out.web),'embedded web data matches source');
const a=html.indexOf('// BEGIN MANDEX TRIAL DATA'),b=html.indexOf('// END MANDEX TRIAL DATA',a);
assert(a>=0&&b>a);
assert.strictEqual(fs.readFileSync(path.join(root,'content/trials/metro-finale.json'),'utf8'),out.city);
const build=new Function('LEVELS','rect','tgt','interiors','pads','tube','V',html.slice(a,b)+'\nreturn trialLayout;');
for(let i=0;i<levels.length;i++){
 const l=levels[i],grid=new Uint8Array(7200),targets=[],rooms=[],p=[],pipes=[],list=Array(60).fill(null);
 const rect=(x,y,w,h,m)=>{for(let j=y;j<y+h;j++)grid.fill(m,j*120+x,j*120+x+w)};
 const layout=build(list,rect,(x,y,kind,range)=>targets.push([x,y,kind,range]),rooms,p,(c,x,y,bx,by,a)=>pipes.push([x/8,y/8,bx/8,by/8]),8);
 assert.strictEqual(list.length,1110,'private sectors never enter level selector');
 const q=i<1050?list[60+i]:layout(i);assert(q&&q.name===l.name&&q.men===l.men);
 q.build();
 assert.strictEqual(Array.from(grid).join(''),l.rows.join(''),'browser grid '+i);
 assert.strictEqual(targets.length,l.targets.length,'browser targets '+i);
 assert.strictEqual(rooms.length,l.rooms.length,'browser rooms '+i);
 assert.strictEqual(p.length,l.pads.length,'browser pads '+i);
 assert.strictEqual(pipes.length,l.tubes.length,'browser tubes '+i);
}
const city=levels.slice(1039,1050);
assert.strictEqual(city[0].id,1099);
assert.strictEqual(city[10].id,1109);
assert(city.every(l=>l.walk&&l.tubes.length>=1&&l.rooms.length>=9));
assert(city.reduce((n,l)=>n+l.targets.length,0)>=90);
assert.deepStrictEqual(city[10].tubes[0],[29,56,105,6]);
const metro=levels.slice(1050);
assert.strictEqual(metro.length,7);
assert(metro.every(l=>l.internal&&l.walk&&l.entries.length===3&&l.towerFeet.every(ft=>ft>=500&&ft<=2000)));
assert.strictEqual(metro[6].towerFeet[2],2000);
for(const l of levels.slice(540)){assert(l.entries.length>=2,l.name);for(const [x,y,w,h] of l.entries){assert(h>=5&&w>=2);for(let yy=y;yy<y+h;yy++)for(let xx=x;xx<x+w;xx++)assert.strictEqual(l.rows[yy][xx],'0',l.name+' blocked doorway');}}
console.log('TRIAL_PACK_PASS levels='+levels.length+' raw_cells=7610400 commands='+out.commandBytes);

const walkingSource=html.slice(html.indexOf('function wBlocked('),html.indexOf('function enterWalk()'));
let entrances=0,bridges=0,transit=0;
for(const l of levels.slice(540)){
 const grid=Uint8Array.from(l.rows.join(''),Number),env={WHW:7,WHH:19,V:8,COLS:120,ROWS:60,WW:960,RC:4,chunks:[],tubes:[],time:0,shake:0,clamp:(v,lo,hi)=>Math.max(lo,Math.min(hi,v)),ring(){},popText(){},SFX:{whoosh(){}},solidAt(x,y){const gx=Math.floor(x/8),gy=Math.floor(y/8);return y>=480||(gx>=0&&gx<120&&gy>=0&&gy<60&&grid[gy*120+gx]!==0)}};
 const clear=new Function('e','with(e){'+walkingSource+String.fromCharCode(10)+'return walkClear;}')(env);
 for(const [x,y,w,h] of l.entries||[]){for(let px=x*8-4;px<=(x+w)*8+4;px+=2)assert(clear(px,(y+h)*8-19.25),'blocked door '+l.id);entrances++}
 for(const [a,y,b] of l.bridges||[]){for(let px=a*8;px<=(b+2)*8;px+=2)assert(clear(px,y*8-19.25),'blocked bridge '+l.id);bridges++}
 if(l.id>=1099)for(const [sx,sy,ex,ey] of l.tubes){assert(clear(sx*8,sy*8)&&[0,1,2,3,4].some(k=>clear(ex*8,(ey-k)*8)),'blocked transit '+l.id);transit++}
}
assert.strictEqual(entrances,1176);assert.strictEqual(bridges,14);assert.strictEqual(transit,43);
const helper=html.slice(html.indexOf('function drawDistrictDirectory('),html.indexOf('function goMenu()'));
const nav=new Function('LEVELS','TRIAL_START','TRIAL_DISTRICTS','PD','chEnd',helper+String.fromCharCode(10)+'return {chapter4Open,trialVisible,trialGroupCount,trialGroupFirst,trialDistrictSize,trialAfterWin};')(Array(1110),60,Array(53),{stars:{}},i=>60+20*(Math.floor((i-60)/20)+1));
const stars={};assert(!nav.chapter4Open(stars));for(let i=1010;i<1110;i++)assert(!nav.trialVisible(i,stars));assert.strictEqual(nav.trialGroupCount(false,stars),48);assert.strictEqual(nav.trialDistrictSize(47,false,stars),10);
stars[1009]=1;assert(nav.trialVisible(1098,stars));assert(!nav.trialVisible(1010,stars));assert.strictEqual(nav.trialAfterWin(1009,stars),1098);assert.strictEqual(nav.trialGroupCount(false,stars),49);assert.strictEqual(nav.trialDistrictSize(48,false,stars),1);
stars[1098]=1;assert(nav.chapter4Open(stars));assert.strictEqual(nav.trialGroupCount(true,stars),5);assert.strictEqual(nav.trialAfterWin(1098,stars),1010);for(let i=1010;i<1110;i++)assert(nav.trialVisible(i,stars));for(let i=1010;i<1098;i++)stars[i]=1;assert.strictEqual(nav.trialAfterWin(1098,stars),1099);
console.log('SECRET_CITY_PASS entrances='+entrances+' bridges='+bridges+' finale_tubes='+transit+' hidden_levels=100 qualifier=1099');
