'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
function checkLandscape(html){
 const a=html.indexOf('function drawSelect(){'),b=html.indexOf('function confirmDel(',a);
 const pa=html.indexOf('const mobilePage='),pb=html.indexOf('function layoutTick()',pa);
 const p=html.indexOf('function mobilePager('),q=html.indexOf('function mobileTab(',p);
 assert(a>=0&&b>a&&pa>=0&&pb>pa&&p>=0&&q>p);
 const env={W:960,H:540,ctx:{measureText:s=>({width:String(s).length*9}),fillRect(){}},txt(){},selTab:1,trialDistrict:0,trialDirectoryPage:0,TRIAL_START:60,CH1:20,CH2:40,
  LEVELS:Array.from({length:1110},(_,i)=>({name:'Stage '+i})),TRIAL_DISTRICTS:Array.from({length:53},(_,i)=>'District '+i),
  PD:{stars:{},custom:Array.from({length:25},(_,i)=>({n:'Custom '+i}))},WEB_UI:{page:{select:0}},
  actions:[],tiles:[],buttons:[],goMenu(){},startStory(i){env.actions.push(['story',i])},
  startCustom(i){env.actions.push(['play',i])},openEditor(back,i){env.actions.push(['edit',i])},
  importCode(){env.actions.push(['import'])},exportCode(l){env.actions.push(['share',l.n])},
  confirmDel(){return true},pdTouch(){},pdSave(){},ch3Unlocked:()=>true,ch2Unlocked:()=>true,storyUnlocked:()=>true,totalStars:()=>0,storyDone:()=>0};
 function bounds(x,y,w,h){assert([x,y,w,h].every(Number.isFinite),'all tile and button hitboxes must have finite coordinates');assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=env.W&&y+h<=env.H,'control within landscape canvas')}
 env.btn=(x,y,w,h,label,fn)=>{bounds(x,y,w,h);env.buttons.push({label,fn,x,y,w,h})};
 env.tile=(x,y,w,h,label,name,stars,locked,fn)=>{bounds(x,y,w,h);assert(typeof name==='string');assert(typeof fn==='function');env.tiles.push({name,fn,x,y,w,h})};
 const helpers=html.slice(html.indexOf('function drawDistrictDirectory('),html.indexOf('function goMenu()'));
 const draw=new Function('env','with(env){'+helpers+html.slice(pa,pb)+html.slice(p,q)+html.slice(a,b)+';return drawSelect;}')(env);
 const paint=()=>{env.tiles.length=env.buttons.length=0;draw()};
 const click=label=>{const b=env.buttons.find(b=>b.label===label);assert(b,'missing button '+label);b.fn()};
 paint();assert.strictEqual(env.tiles.length,12);click('NEXT >');paint();assert.strictEqual(env.tiles[0].name,'Custom 12');env.tiles[0].fn();assert.deepStrictEqual(env.actions.pop(),['play',12]);
 click('EDIT');assert.deepStrictEqual(env.actions.pop(),['edit',12]);click('SHARE');assert.deepStrictEqual(env.actions.pop(),['share','Custom 12']);
 click('NEXT >');paint();assert.strictEqual(env.tiles.length,1);click('DEL');paint();assert.strictEqual(env.WEB_UI.page.select,1);assert.strictEqual(env.tiles.length,12);
 click('NEW LEVEL');assert.deepStrictEqual(env.actions.pop(),['edit',-1]);click('IMPORT LEVEL');assert.deepStrictEqual(env.actions.pop(),['import']);
 click('TRIALS');paint();assert.strictEqual(env.tiles.length,20);env.tiles[0].fn();assert.deepStrictEqual(env.actions.pop(),['story',60]);
 assert(!env.buttons.some(b=>b.label==='CHAPTER 4'),'secret tab hidden');
 click('< DISTRICT');paint();assert.strictEqual(env.tiles.length,10);env.tiles.at(-1).fn();assert.deepStrictEqual(env.actions.pop(),['story',1009]);
 env.WEB_UI.districtPicker=true;env.trialDirectoryPage=0;paint();assert.strictEqual(env.tiles.length,0);assert.strictEqual(env.buttons.filter(b=>b.label==='').length,20);click('PAGE >');paint();assert.strictEqual(env.buttons.filter(b=>b.label==='').length,20);click('PAGE >');paint();assert.strictEqual(env.buttons.filter(b=>b.label==='').length,8);click('< PAGE');paint();click('< PAGE');paint();env.buttons.filter(b=>b.label==='')[18].fn();paint();env.tiles[0].fn();assert.deepStrictEqual(env.actions.pop(),['story',420]);
 env.PD.stars[1009]=1;env.trialDistrict=0;paint();click('< DISTRICT');paint();assert.strictEqual(env.tiles.length,1);env.tiles[0].fn();assert.deepStrictEqual(env.actions.pop(),['story',1098]);assert(!env.buttons.some(b=>b.label==='CHAPTER 4'));
 env.PD.stars[1098]=1;paint();click('CHAPTER 4');paint();assert.strictEqual(env.tiles.length,20);env.tiles[0].fn();assert.deepStrictEqual(env.actions.pop(),['story',1010]);
 click('< DISTRICT');paint();env.tiles.at(-1).fn();assert.deepStrictEqual(env.actions.pop(),['story',1109]);click('TRIALS');paint();assert.strictEqual(env.tiles.length,20);
 for(let i=60;i<1110;i++)env.PD.stars[i]=3;
 env.PD.stars[65]=0;env.PD.stars[641]=0;env.trialDistrict=18;paint();click('PLAY NEXT UNCLEARED DISTRICT');assert.deepStrictEqual(env.actions.pop(),['story',641]);
 env.PD.stars[641]=3;env.trialDistrict=47;paint();click('PLAY NEXT UNCLEARED DISTRICT');assert.deepStrictEqual(env.actions.pop(),['story',65]);
 env.PD.stars[65]=3;paint();assert(env.buttons.some(b=>b.label==='ALL TRIALS CLEARED'));env.buttons.find(b=>b.label==='ALL TRIALS CLEARED').fn();assert(env.actions.length===0);
 click('CHAPTER 4');paint();env.PD.stars[1109]=0;paint();click('PLAY NEXT UNCLEARED DISTRICT');assert.deepStrictEqual(env.actions.pop(),['story',1109]);
 env.PD.stars[1109]=3;paint();assert(env.buttons.some(b=>b.label==='ALL TRIALS CLEARED'));
 return 'WEB_LANDSCAPE_SELECT_PASS all_custom_pages=1 actions=1 delete_clamp=1 trial_wrap=1';
}
if(require.main===module)console.log(checkLandscape(fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8')));
module.exports=checkLandscape;
