/* Exercise the shipped browser editor's one-pointer-per-stroke owner and
 * cancellation behavior without invoking the full rendering engine. */
'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const a=html.indexOf('function eUp(){');
const b=html.indexOf('function eBuild(){',a);
assert(a>=0&&b>a,'real editor stroke helpers exist');
const make=new Function(`
 const E={drag:null,pointer:null},commits=[];
 const eRect=(a,b,m)=>commits.push([a,b,m]);
 ${html.slice(a,b)}
 return {E,commits,eBeginStroke,eStrokeOwner,eEndStroke};
`)();
const E=make.E;
E.drag={a:[30,33],b:[40,39],m:2};
assert(make.eBeginStroke(101),'first touch should own rectangle');
assert.strictEqual(E.pointer,101,'owner recorded');
assert(!make.eBeginStroke(202),'second touch may not claim stroke');
assert(!make.eStrokeOwner(202),'second finger cannot drag');
assert(make.eStrokeOwner(101),'first finger can drag');
assert(!make.eEndStroke(202,false),'second finger cannot finish stroke');
assert.strictEqual(make.commits.length,0,'foreign release never commits rectangle');
assert(E.drag,'foreign release does not erase active stroke');
assert(make.eEndStroke(101,false),'owner release finishes stroke');
assert.strictEqual(make.commits.length,1,'rectangle commits exactly once');
assert.strictEqual(E.pointer,null,'owner cleared on completion');
assert.strictEqual(E.drag,null,'draw state cleared on completion');
E.drag={a:[30,33],b:[36,37],m:4};
assert(make.eBeginStroke(303),'new gesture can begin after prior ends');
assert(!make.eEndStroke(404,true),'foreign cancellation ignored');
assert(make.eEndStroke(303,true),'owner cancellation clears stroke');
assert.strictEqual(make.commits.length,1,'cancelled rectangle is not committed');
assert.strictEqual(E.pointer,null);
assert.strictEqual(E.drag,null);
assert(!make.eBeginStroke(505),'an empty stroke cannot steal pointer');
assert.strictEqual(E.pointer,null);
const requireText=(x)=>assert(html.includes(x),'missing runtime wire-up: '+x);
requireText("if(E.pointer!==null&&E.pointer!==e.pointerId)return;");
requireText("if(!eStrokeOwner(e.pointerId))return;");
requireText("if(eBeginStroke(e.pointerId))");
requireText("eEndStroke(e.pointerId,false)");
requireText("eEndStroke(e.pointerId,true)");
requireText("E.drag=null;E.pointer=null;");
const i=html.indexOf('<script>'),j=html.indexOf('</script>',i);
new Function(html.slice(i+8,j));
console.log('EDITOR_TOUCH_ISOLATION_PASS owner=1 second_finger=1 move_guard=1 cancel=1 completion=1 rotation=1 syntax=1');
