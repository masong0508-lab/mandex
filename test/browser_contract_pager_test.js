'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function sec(a,b){const i=html.indexOf(a),j=html.indexOf(b,i+a.length);assert(i>=0&&j>i);return html.slice(i,j)}
const run=new Function('assert',`
let W=960,H=540,selTab=4,contractReview=-1,CH3_END=60,LEVELS=Array.from({length:84},(_,i)=>({name:'Contract '+i,bonusPower:'breaker'})),WEB_UI={page:{}},PD={stars:{},unlockAll:false},tiles=[],buttons=[];
const ctx={fillRect(){}},txt=()=>{},goMenu=()=>{},openContract=()=>{},btn=(x,y,w,h,label,fn)=>buttons.push({x,y,w,h,label,fn}),tile=(x,y,w,h,label,name,stars,locked,fn)=>tiles.push({x,y,w,h,label,locked,fn});
`+sec('const mobilePage=','function layoutTick()')+sec('const contractUnlocked=','function storyUnlocked(')+sec('function mobilePager(','function mobileTab(')+sec('function drawSelect(){','function confirmDel(')+`
function draw(){tiles=[];buttons=[];drawSelect();for(const t of tiles.concat(buttons))assert(t.x>=0&&t.y>=0&&t.x+t.w<=W&&t.y+t.h<=H,'offscreen hit target');for(const t of tiles)assert(t.y+t.h<=H-62,'level overlaps pagination')}
draw();assert(tiles.length===16&&tiles[0].label===1&&tiles[15].label===16,'original contract page');buttons.find(b=>b.label==='NEXT >').fn();draw();assert(tiles.length===8&&tiles[0].label===17&&tiles[7].label===24,'overtime page');assert(!tiles[0].locked&&tiles[1].locked,'new entry point without unlock cheat');PD.stars[76]=1;draw();assert(!tiles[1].locked,'overtime progression');buttons.find(b=>b.label==='< PREV').fn();draw();assert(tiles[0].label===1,'return to first page');assert(!contractUnlocked(59)&&!contractUnlocked(84),'invalid IDs not offered');return 24;
`);
console.log('BROWSER_CONTRACT_PAGER_PASS '+run(assert)+' accessible tiles');
