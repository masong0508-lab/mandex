/* Mandex browser: real read-only shot UX messages and tool counts. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const web=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const start=web.indexOf('function shotReadiness(m){');
const end=web.indexOf('function drawHUD(){',start);
assert(start>=0&&end>start,'real browser shot readiness helper exists');
const fn=(state)=>new Function('state','SX','SY','V',
  web.slice(start,end)+'\nreturn shotReadiness;')(state,100,100,8);
const aim={mode:'aim',x:100,y:100,stun:0,seated:false,vlv:false};
const read=fn('play');
assert.strictEqual(read(aim),'DRAG BACK TO AIM','idle sling needs a pull');
assert.strictEqual(read({...aim,x:116}),'RELEASE TO LAUNCH',
  'enough pull previews readiness without launching');
assert.strictEqual(read({...aim,x:120,stun:1}),'DAZED - AIM LOCKED',
  'stun blocks aim');
assert.strictEqual(read({...aim,x:120,seated:true}),'AIR FILLING - DRAG OFF',
  'valve seat must not suggest launching');
assert.strictEqual(read({...aim,vlv:true}),'AIR FILLING - DRAG OFF',
  'inflation latch blocks release');
assert.strictEqual(read({...aim,mode:'fly'}),'','no duplicate dive tip');
assert.strictEqual(fn('win')(aim),'','won levels have no ready prompt');
assert(web.includes("const ready=shotReadiness(man);"),'actual HUD renders helper');
assert(web.includes("Math.max(0,pw.left[tid]||0)"),
  'toolbar never shows undefined or negative counts');
assert(web.includes("}[pw.sel];")&&web.includes('if(tip){'),
  'browser still describes selected power effect');
const scriptStart=web.indexOf('<script>');
const scriptEnd=web.indexOf('</script>',scriptStart);
assert(scriptStart>=0&&scriptEnd>scriptStart);
new Function(web.slice(scriptStart+8,scriptEnd));
console.log('WEB_TOOL_FEEDBACK_PASS ready=1 blocked=2 status=per-shot stock=safe');
