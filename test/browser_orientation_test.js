'use strict';
/* Real-source regression for responsive Mandex canvas and menu geometry.
 * No Playwright, DOM library or Internet access required. */
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function extract(start,end){
  const a=html.indexOf(start),b=html.indexOf(end,a+start.length);
  assert(a>=0&&b>a,'missing source anchor: '+start+' / '+end);
  return html.slice(a,b);
}
const script=extract('<script>','</script>').slice(8);
new Function(script); /* Fail immediately on duplicate declarations or syntax. */
assert(html.includes('viewport-fit=cover'),'respect phone safe areas');
assert(html.includes('MOBILE.includes(t.kind)')&&!html.includes('WEB_UI.includes(t.kind)'),
  'landlord AI mobile unit roster must never be replaced by responsive UI state');
assert(html.includes('height:100dvh'),'use dynamically resized mobile viewport');
const cv={width:960,height:540,style:{},getBoundingClientRect(){
  return {left:10,top:20,width:parseFloat(this.style.width)||960,
          height:parseFloat(this.style.height)||540};
}};
const window={innerWidth:390,innerHeight:844,visualViewport:{width:390,height:844}};
const document={body:{getBoundingClientRect:()=>({
  width:window.visualViewport.width,height:window.visualViewport.height
})}};
const layout=extract('const CAM=', 'function camTick(');
const h=new Function('cv','window','document',`
  let W=960,H=540,COLS=120,state='menu',dragging=true,sliderDrag=true,
      E={drag:{pen:true}},KEYS={TL:true},TID={7:'TL'},hits=[1],_vig=null,
      CL={cols:120};
  function setWorld(cols){COLS=cols}
  ${layout}
  return {
    tick:layoutTick,setState:v=>{state=v},
    info:()=>({W,H,portrait:isPortrait(),camera:CAM.on,
      gesture:WEB_UI.gesture,dragging,sliderDrag,activeTouches:Object.keys(TID).length,
      bufferW:cv.width,bufferH:cv.height})
  }
`)(cv,window,document);
h.tick();let v=h.info();
assert(v.portrait&&v.W===540&&v.H>=1080,'portrait menus use tall virtual layout');
assert(!v.camera,'menu does not accidentally activate gameplay camera');
assert(!v.dragging&&!v.sliderDrag&&v.activeTouches===0,'rotating cancels stuck input');
assert(parseFloat(cv.style.width)<=390 && parseFloat(cv.style.height)<=844,'canvas fits portrait viewport');
h.setState('edit');h.tick();assert(h.info().portrait,'editor stays portrait');
h.setState('play');h.tick();assert(h.info().camera,'portrait gameplay camera activates');
/* A soft keyboard changes the visual viewport, not the physical device
   orientation: it must shrink CSS size without triggering landscape menus. */
window.visualViewport.height=340;h.tick();v=h.info();
assert(v.portrait&&v.W===540,'soft keyboard never forces a portrait layout to landscape');
window.visualViewport.height=844;
window.innerWidth=844;window.innerHeight=390;
window.visualViewport.width=844;window.visualViewport.height=390;h.tick();v=h.info();
assert(!v.portrait&&v.W===960&&v.H===540,'landscape uses original virtual resolution');
assert(!v.camera,'normal landscape returns to unzoomed world camera');
assert(parseFloat(cv.style.width)<=844 && parseFloat(cv.style.height)<=390,'landscape fits viewport');
window.innerWidth=360;window.innerHeight=640;
window.visualViewport.width=360;window.visualViewport.height=640;h.setState('menu');h.tick();
assert(h.info().portrait,'resize back to portrait without restarting game');
const pointerCode=extract('function pt(e){','function retry(');
const pointer=new Function('cv','W','H',pointerCode+'return pt;')(cv,540,960);
cv.getBoundingClientRect=()=>({left:20,top:30,width:270,height:480});
const p=pointer({clientX:155,clientY:270});
assert.deepStrictEqual(p,[270,480],'client pointer scaled exactly to virtual canvas');
const editorCode=extract('function mobileEditorSurface(){','function drawMobileEditor(){');
for(const height of [720,960,1200]){
  const ui={editorX:0,editorScale:1,editorHeight:480};
  const funcs=new Function('WEB_UI','W','H','GY','WW','clamp',editorCode+
    'return {surface:mobileEditorSurface,point:mobileEditorToWorld};')(
      ui,540,height,480,960,(x,a,b)=>Math.max(a,Math.min(b,x)));
  const surf=funcs.surface();assert(surf>=300&&surf<=480,'editor fits all screens');
  ui.editorX=999;funcs.surface();
  const maxPan=Math.max(0,960-540/ui.editorScale);
  assert(Math.abs(ui.editorX-maxPan)<1e-6,'pan never exposes beyond world edge');
  const xy=funcs.point(0,surf/2);
  assert(Math.abs(xy[0]-ui.editorX)<1e-6 && Math.abs(xy[1]-240)<1e-6,
    'portrait editor touch maps to correct underlying world cell');
}
const mobile=extract('/* Portrait pages are deliberately','function drawMenu(){');
const records=[];
const ctx=new Proxy({measureText:s=>({width:String(s).length*10})},{
  get(target,key){return key in target?target[key]:()=>{}},
  set(target,key,value){target[key]=value;return true}
});
const env={
  W:540,H:960,ctx,WEB_UI:{portrait:true,page:{select:0,ach:0,outfit:0,options:0,edit:0},editorX:0},
  state:'menu',time:1,frameDt:.016,muted:false,webEcoFX:false,frameMeterVisible:false,armReset:0,armProg:0,isPortrait:()=>env.W===540,
  hits:records,ACH:Array.from({length:17},(_,i)=>({id:'a'+i,n:'Achievement '+i,d:'Earn '+i})),
  OUTFITS:Array.from({length:16},(_,i)=>({id:'look'+i,n:'Outfit '+i})),
  outfitCur:{n:'Classic Red'},PD:{stars:{},ach:{},custom:Array.from({length:6},(_,i)=>({n:'User level '+i,id:i})),own:{}},
  LEVELS:Array.from({length:660},(_,i)=>({name:'District Building '+i})),
  CH1:20,CH2:40,TRIAL_START:60,TRIAL_DISTRICTS:Array.from({length:30},(_,i)=>'District '+i),trialDistrict:0,selTab:0,TRIAL_META:Array.from({length:600},()=>['Trial','TIP',0,0,0,0,0,4,0,0,0,0,6,0]),
  GUIDES:['OFF','SHORT','FULL'],SHAKES:[['OFF',0],['LOW',.5],['FULL',1]],
  OPT:()=>({}),optVol:()=>.8,MAT:[null,...Array.from({length:8},(_,i)=>({n:'Material '+i}))],
  btn:(x,y,w,h,label,fn,o)=>records.push({x,y,w,h,label,fn}),
  panel:()=>{},txt:()=>{},drawSlamScene:()=>{},drawManFrame:()=>{},
  goMenu:()=>{},goSelect:()=>{},startStory:()=>{},startCustom:()=>{},
  openEditor:()=>{},importCode:()=>{},exportCode:()=>{},confirmDel:()=>true,
  pdSave:()=>{},pdTouch:()=>{},equip:()=>{},setMuted:()=>{},audioInit:()=>{},
  toolName:()=> 'NONE',toolCycle:()=>{},ownedCount:()=>4,achCount:()=>2,totalStars:()=>3,
  mobilePage:(kind,n,size)=>Math.max(0,Math.min(Math.max(0,Math.ceil(n/size)-1),env.WEB_UI.page[kind]||0)),
  storyUnlocked:()=>true,ch2Unlocked:()=>true,ch3Unlocked:()=>true,
  SFX:{pop:()=>{},ding:()=>{}},performance:{now:()=>0}
};
const trialHelpers=extract('function drawDistrictDirectory(', 'function goMenu()');
const perfHelpers=extract('const WEB_PROFILES=', 'function chip(');
const methods=new Function('env','with(env){'+perfHelpers+trialHelpers+mobile+
  'return {menu:drawMobileMenu,select:drawMobileSelect,ach:drawMobileAchievements,'+
  'outfit:drawMobileOutfits,options:drawMobileOptions}};')(env);
let total=0;
function inspect(kind){
  records.length=0;env.state=kind;methods[kind]();
  assert(records.length>=(kind==='ach'||kind==='outfit'?1:4),kind+' should provide touch controls');
  for(const r of records){
    assert(r.w>=40&&r.h>=35,kind+' touch button too small: '+r.label);
    assert(r.x>=0&&r.y>=0&&r.x+r.w<=env.W+1&&r.y+r.h<=env.H+1,
      kind+' touch button outside portrait viewport: '+r.label);
  }
  total+=records.length;
}
for(const height of [760,960,1170]){
  env.H=height;env.W=540;
  inspect('menu');
  for(const tab of [0,1,2,3,4]){env.selTab=tab;env.WEB_UI.page.select=0;inspect('select')}
  env.selTab=4;env.WEB_UI.districtPicker=true;inspect('select');env.WEB_UI.districtPicker=false;
  inspect('ach');inspect('outfit');
  for(const page of [0,1,2,3]){env.WEB_UI.page.options=page;inspect('options')}
}
assert(html.includes('WEB_UI.gesture={id:e.pointerId,x,y,screen:state}'),
 'portrait menu activation must defer to pointerup so swipes do not fire buttons');
assert(html.includes('mobileEditorToWorld(x,y)') &&
       html.includes('mobileEditorToWorld(mx,my)') &&
       html.includes('my<WEB_UI.editorHeight'),
 'portrait paint input is transformed and never writes while dragging across toolbar');
console.log('WEB_ORIENTATION_PASS responsive_canvas=1 portrait_editor=1 ratio_pointer=1 touch_controls='+total);
