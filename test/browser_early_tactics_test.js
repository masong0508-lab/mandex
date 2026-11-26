/* Execute the real browser early-stage tactics and compare them with
   the shared console's ROM table, not just two handwritten fixtures. */
'use strict';
const assert=require('assert');
const fs=require('fs'),path=require('path');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const core=fs.readFileSync(path.join(__dirname,'../core/mx.c'),'utf8');
const start=html.indexOf('const EARLY_TACTICS=[');
const end=html.indexOf('// BEGIN MANDEX TRIAL DATA',start);
assert(start>=0&&end>start,'live JS early-level hint data found');
const levels=Array.from({length:60},()=>({}));
const longWalk='Special controls: walking, jumping and punching';levels[22].tip=longWalk;
const browser=new Function('LEVELS',html.slice(start,end)+'\nreturn EARLY_TACTICS;')(levels);
assert.equal(browser.length,40,'all original + chapter-two levels covered');
const a=core.indexOf('static const char *const early[MX_CH3_START]={');
const b=core.indexOf('static const char *const annex[4]',a);
assert(a>=0&&b>a,'production C stage hints found');
const c=[...core.slice(a,b).matchAll(/"([^"]+)"/g)].map(x=>x[1]);
assert.equal(c.length,40,'shared native core has the same 40 stage hints');
for(let i=0;i<40;i++){
  assert.equal(browser[i],c[i],'browser and C tactics agree on level '+(i+1));
  assert(browser[i].length<=50,'DS-readable tip '+(i+1));
  if(i!==22)assert.equal(levels[i].tip,browser[i],'new tip fills an empty stage '+(i+1));
}
assert.equal(levels[22].tip,longWalk,'author-created walk tutorial remains intact');
console.log('PASS: 40 browser stage hints, exact C parity and preservation of authored tutorials');
