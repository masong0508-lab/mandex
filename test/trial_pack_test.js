'use strict';
/* Validate all authored trials and both generated runtime representations. */
const assert=require('assert'),fs=require('fs'),path=require('path');
const root=path.join(__dirname,'..');
const pack=require('../tools/trial_pack.js');
const levels=pack.generateTrials();
const out=pack.emitTrials(levels);
assert.strictEqual(levels.length,1050);
assert.strictEqual(new Set(levels.map(l=>l.rows.join(''))).size,1050);
assert.strictEqual(levels.reduce((n,l)=>n+l.rows.join('').length,0),7560000);
assert(out.commandBytes<1050*7200/5,'compact command payload must fit handheld budget');
for(let d=0;d<53;d++){
 const source=fs.readFileSync(path.join(root,'content/trials/district-'+(d+1)+'.json'),'utf8');
 assert.strictEqual(source,out.districts[d],'uncompressed editable source matches generator');
}
assert.strictEqual(fs.readFileSync(path.join(root,'core/trial_levels.h'),'utf8'),out.header,'native data matches source');
const html=fs.readFileSync(path.join(root,'web/index.html'),'utf8');
assert(html.includes(out.web),'embedded web data matches source');
const a=html.indexOf('// BEGIN MANDEX TRIAL DATA'),b=html.indexOf('// END MANDEX TRIAL DATA',a);
assert(a>=0&&b>a);
const build=new Function('LEVELS','rect','tgt','interiors','pads','tube','V',html.slice(a,b));
for(let i=0;i<levels.length;i++){
 const l=levels[i],grid=new Uint8Array(7200),targets=[],rooms=[],p=[],pipes=[],list=Array(60).fill(null);
 const rect=(x,y,w,h,m)=>{for(let j=y;j<y+h;j++)grid.fill(m,j*120+x,j*120+x+w)};
 build(list,rect,(x,y,kind,range)=>targets.push([x,y,kind,range]),rooms,p,(c,x,y,bx,by,a)=>pipes.push([x/8,y/8,bx/8,by/8]),8);
 const q=list[60+i];assert(q&&q.name===l.name&&q.men===l.men);
 q.build();
 assert.strictEqual(Array.from(grid).join(''),l.rows.join(''),'browser grid '+i);
 assert.strictEqual(targets.length,l.targets.length,'browser targets '+i);
 assert.strictEqual(rooms.length,l.rooms.length,'browser rooms '+i);
 assert.strictEqual(p.length,l.pads.length,'browser pads '+i);
 assert.strictEqual(pipes.length,l.tubes.length,'browser tubes '+i);
}
const city=levels.slice(-11);
assert.strictEqual(city[0].id,1099);
assert.strictEqual(city[10].id,1109);
assert(city.every(l=>l.walk&&l.tubes.length>=1&&l.rooms.length>=9));
assert(city.reduce((n,l)=>n+l.targets.length,0)>=90);
assert.deepStrictEqual(city[10].tubes[0],[29,56,105,6]);
console.log('TRIAL_PACK_PASS levels='+levels.length+' raw_cells=7560000 commands='+out.commandBytes);
