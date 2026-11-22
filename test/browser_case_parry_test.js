/* Execute the shipped browser case/punch/ground-slam code. Parrying must
   use the existing attack hitbox, with one bounded reward per actual case. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function section(a,b){
  const i=html.indexOf(a),j=html.indexOf(b,i+a.length);
  assert(i>=0&&j>i,'Production source section '+a);
  return html.slice(i,j);
}
const hit=section('function walkHit(', 'function walkStep(');
const stepCases=section('function updCases(', '/* Only a collapsing room');
function make(){
  const e={
    V:8,RC:4,WHH:19,RUBBER:7,COLS:120,ROWS:60,WW:960,G:900,GY:480,
    score:0,shake:0,grid:new Uint8Array(7200),hp:new Float32Array(7200),
    MAT:[],chunks:[],cases:[],targets:[],events:[],hurtCount:0,
    man:{mode:'walk',x:320,y:300,face:1,vx:0,vy:0,pT:0,pCd:0,stomp:false,R:14}};
  e.idx=(x,y)=>y*120+x;
  e.inb=(x,y)=>x>=0&&x<120&&y>=0&&y<60;
  e.damageCell=()=>0;e.wobKick=()=>{};e.wake=()=>{};e.killTarget=()=>{};
  for(const type of ['spark','ring','dust','popText','P']){
    e[type]=(...args)=>e.events.push([type,...args]);
  }
  e.rnd=()=>.5;
  e.solidAt=()=>false;
  e.hurtMan=()=>{e.hurtCount++;return true;};
  e.SFX={};
  for(const type of ['crunch','whoosh','thud','boom'])
    e.SFX[type]=(...args)=>e.events.push(['sfx',type,...args]);
  const funcs=new Function('e','with(e){'+hit+'\n'+stepCases+
    '\nreturn {walkHit,walkPunch,walkSlam,updCases};}')(e);
  return {e,...funcs,caseAt(x,y){e.cases.push({x,y,vx:-60,vy:0,vr:9,rot:0,l:4})}};
}
const count=(e,name)=>e.events.filter(x=>x[0]===name).length;
const popups=(e)=>e.events.filter(x=>x[0]==='popText').map(x=>x[3]);
let checks=0;
const check=(ok,msg)=>{assert(ok,msg);checks++};
const first=make();
first.caseAt(337,297);
first.walkPunch();
check(first.e.cases.length===0,'front-facing punch must remove the incoming case');
check(first.e.score===150,'one briefcase parry earns 150 points exactly once');
check(popups(first.e).includes('PARRY! +150'),'parry must produce readable impact feedback');
check(popups(first.e).includes('POW!'),'punch impact feedback still uses the existing hit effect');
first.updCases(1/60);
check(first.e.hurtCount===0,'removed case must not hit the player in the same step');

for(const [label,x,y] of [['behind',296,297],['edge',337+19,297],['too high',337,333]]){
  const a=make();a.caseAt(x,y);a.walkPunch();
  check(a.e.cases.length===1&&a.e.score===0&&
    !popups(a.e).includes('PARRY! +150'),'miss must not parry '+label);
}
const two=make();
two.caseAt(337,297);two.caseAt(340,298);two.walkPunch();
check(two.e.cases.length===0&&two.e.score===300,
  'punch must parry both incoming cases without double-awarding one');
two.walkPunch();
check(two.e.score===300&&popups(two.e).filter(x=>x==='PARRY! +150').length===2,
  'inactive cases must not be repeat-scored by another punch');

const slam=make();slam.caseAt(320,320);
slam.walkSlam();
check(slam.e.cases.length===0&&slam.e.score===150,
  'belly slam must parry the case exactly once across two hitboxes');
check(popups(slam.e).filter(x=>x==='PARRY! +150').length===1,
  'dual-hit slam may not duplicate parry feedback');

const empty=make();empty.walkPunch();
check(empty.e.score===0&&count(empty.e,'spark')===0,
  'ordinary empty punch must not invent a parry');

const unguarded=make();
unguarded.caseAt(320,300);
unguarded.updCases(1/60);
check(unguarded.e.hurtCount===1&&unguarded.e.score===0&&unguarded.e.cases.length===0,
  'briefcase must still strike an undefended player');

const parseStart=html.indexOf('<script>'),parseEnd=html.indexOf('</script>',parseStart);
new Function(html.slice(parseStart+8,parseEnd));
console.log('BROWSER_CASE_PARRY_PASS checks='+checks+
  ' front=1 miss=1 multi=1 slam=1 score_once=1 vulnerable=1 syntax=1');
