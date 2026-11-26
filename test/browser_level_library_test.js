'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const a=html.indexOf('let selectDistrict='),b=html.indexOf('function drawMobileAchievements(){',a);assert(a>0&&b>a);
const setup=`
let W=540,H=960,selTab=-1,hits=[],goes=0,launches=[],edits=[],shared=[],imports=0,deleted=0;
const WEB_UI={page:{select:0}},CH1=20,CH2=40,PD={stars:{},custom:Array.from({length:31},(_,i)=>({n:'Custom '+i}))};
const LEVELS=Array.from({length:60},(_,i)=>({name:'Level '+(i+1)}));
const CONTRACT_FAMILIES=Array.from({length:18},(_,i)=>({n:'District '+i})),CONTRACT_MODES=['Open','Bank','Chain','Guard'].map(n=>({n}));
const CONTRACTS=Array.from({length:72},(_,id)=>({id}));const contractStars=i=>i===0?3:0;
const storyUnlocked=i=>i<3||!!PD.stars[i-1],ch2Unlocked=i=>i===20||!!PD.stars[i-1],ch3Unlocked=i=>!!PD.stars[i-1];
const startStory=i=>launches.push(['story',i]),startContract=i=>launches.push(['contract',i]),startCustom=i=>launches.push(['custom',i]),openEditor=(back,i)=>edits.push(i),exportCode=q=>shared.push(q.n),importCode=()=>imports++,confirmDel=()=>true,pdTouch=()=>{},pdSave=()=>deleted++;
const goMenu=()=>goes++,goSelect=()=>{},SFX={pop(){}};
const ctx={fillRect(){},measureText(s){return {width:s.length*8}}};
const txt=()=>{},btn=(x,y,w,h,label,fn)=>hits.push({x,y,w,h,label,fn});
const mobilePage=(k,n,size)=>Math.max(0,Math.min(Math.ceil(n/size)-1,WEB_UI.page[k]||0));
const mobilePager=(k,n,size,y)=>{if(n>size){btn(12,y,100,46,'prev',()=>{});btn(W-112,y,100,46,'next',()=>{})}};
`;
const test=new Function('assert',setup+html.slice(a,b)+`
let cases=0;const model=()=>selectModel();
assert.strictEqual(model().cards.length,4);model().cards[0].fn();assert.deepStrictEqual(launches.pop(),['story',0]);
PD.stars[0]=3;assert.strictEqual(nextCampaignLevel(),1);
model().cards[1].fn();assert.strictEqual(selTab,-2);assert.strictEqual(model().cards.length,3);
for(const c of model().cards.slice()){c.fn();assert.strictEqual(model().cards.length,20);selectBack();assert.strictEqual(selTab,-2)}
selectBack();assert.strictEqual(selTab,-1);model().cards[2].fn();assert.strictEqual(model().cards.length,18);
for(const [i,c] of model().cards.entries()){WEB_UI.page.select=2;c.fn();assert.strictEqual(model().cards.length,4);for(const [k,q] of model().cards.entries()){q.fn();assert.deepStrictEqual(launches.pop(),['contract',4*i+k]);}selectBack();assert.strictEqual(WEB_UI.page.select,2);}
selectBack();model().cards[3].fn();assert.strictEqual(model().cards.length,31);WEB_UI.page.select=3;model().cards[30].fn();assert.strictEqual(model().cards.length,4);model().cards[0].fn();assert.deepStrictEqual(launches.pop(),['custom',30]);model().cards[1].fn();assert.strictEqual(edits.pop(),30);model().cards[2].fn();assert.strictEqual(shared.pop(),'Custom 30');model().cards[3].fn();assert.strictEqual(PD.custom.length,30);assert.strictEqual(deleted,1);assert.strictEqual(WEB_UI.page.select,3);
model().actions[0].fn();assert.strictEqual(edits.pop(),-1);model().actions[1].fn();assert.strictEqual(imports,1);
for(const [width,height] of [[540,720],[540,960],[540,1240],[960,540]]){
 W=width;H=height;
 for(const [tab,d,item] of [[-1,-1,null],[-2,-1,null],[0,-1,null],[2,-1,null],[3,-1,null],[4,-1,null],[4,17,null],[1,-1,null],[1,-1,PD.custom[5]]]){
 selectRoute(tab,d,item);for(const page of [0,1,999]){
 WEB_UI.page.select=page;hits=[];drawLevelLibrary();
 assert(hits.length>0&&hits.length<=17,'bounded visible controls: '+tab+' '+hits.length);
 for(const h of hits)assert(h.x>=0&&h.y>=0&&h.x+h.w<=W&&h.y+h.h<=H&&h.h>=42,'touch target fits');
 for(let i=0;i<hits.length;i++)for(let j=i+1;j<hits.length;j++){const a=hits[i],b=hits[j];assert(a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y,'buttons must not overlap')}
 cases++;
 }}
}
return cases;`)(assert);
console.log('LEVEL_LIBRARY_PASS layouts='+test+' campaign=60 contracts=72 custom_pagination=1');
