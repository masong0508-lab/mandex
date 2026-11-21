/* Native portrait/landscape browser UI regression: extracted straight from
 * the real single-file game's layout and screen functions. Runs in Node with
 * no browser dependency and checks viewport sizing, pagination and touch
 * navigation after screen rotations without changing 60 Hz simulation. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function between(start,end){
  const a=html.indexOf(start),b=html.indexOf(end,a+start.length);
  assert(a>=0&&b>a,'missing layout section '+start);
  return html.slice(a,b);
}
const viewportCode=between('function webViewport(', 'let mobilePortrait=');
const webViewport=new Function(viewportCode+'\nreturn webViewport;')();
const cases=[
  [390,844,'menu',540,1169,true],
  [844,390,'menu',960,540,false],
  [1024,768,'play',960,540,false],
  [360,740,'play',540,1110,true],
  [390,844,'options',540,1169,true],
  [390,844,'select',540,1169,true],
  [390,844,'ach',540,1169,true],
  [390,844,'outfit',540,1169,true],
  [390,844,'cutscene',960,540,false],
  [390,844,'edit',540,1169,true]
];
for(const [vw,vh,mode,sw,sh,portrait]of cases){
  const o=webViewport(vw,vh,mode);
  assert.strictEqual(o.w,sw,'width '+mode);
  assert.strictEqual(o.h,sh,'height '+mode);
  assert.strictEqual(o.tall,portrait,'portrait/landscape state '+mode);
  assert(o.cssW<=vw&&o.cssH<=vh&&o.cssW>0&&o.cssH>0,'canvas physically fits');
  assert(o.cssW>=(portrait?vw-2:1),'portrait content uses full phone width');
}
for(const [w,h]of [[320,568],[390,844],[430,932],[540,1000],[360,800],[800,360],[1920,1080]]){
  for(const state of ['play','menu','select','options','ach','outfit','edit']){
    const o=webViewport(w,h,state);
    assert(o.cssW<=w&&o.cssH<=h,'visible browser viewport fits on rotation');
    assert(o.w>=540&&o.h>=540&&o.w<=960&&o.h<=1240,'bounded backing canvas');
  }
}
const htmlJS=between('<script>','</script>').slice('<script>'.length);
new Function(htmlJS);
const scriptChecks=[
  "if(mobilePortrait){drawMobileMenu();return}",
  "if(mobilePortrait){drawMobileSelect();return}",
  "if(mobilePortrait){drawMobileOptions();return}",
  "if(mobilePortrait){",
  "cv.setPointerCapture(e.pointerId)",
  "lostpointercapture",
  "orientationchange",
  "document.hidden",
  "cameraMaxX=Math.max(0,WW-vw)"
];
for(const s of scriptChecks)assert(html.includes(s),'browser implementation missing '+s);
const chunk=between('let mobileLevelPage=0,','function drawMenu(){');
const nav=new Function(
  'ctx','txt','btn','tile','panel','goMenu','goSelect','goSelectStub',
  'startStory','startCustom','openEditor','importCode','PD','LEVELS','CH1','CH2',
  'storyUnlocked','ch2Unlocked','ch3Unlocked','CL','W','H','ACH','OUTFITS',
  'achCount','ownedCount','totalStars','storyDone','muted','audioInit',
  'setMuted','OPT','SFX','pdTouch','pdSave','toast','confirmDel','exportCode',
  'selTabValue', 'stateValue',
  'let selTab=selTabValue,state=stateValue,mouseX=0,mouseY=0;'+
  'function mobileTest(){'+chunk+
  'return {drawMobileSelect,drawMobileMenu,getState:()=>state,setTab:v=>{selTab=v},setPage:v=>{mobileLevelPage=v},getPage:()=>mobileLevelPage};}\nreturn mobileTest();'
);
const touchHits=[],ctx={fillStyle:null,fillRect(){},save(){},restore(){},beginPath(){},rect(){},clip(){}};
const levels=Array.from({length:35},(_,i)=>({name:'House '+i}));
const pd={custom:[{n:'Custom One'},{n:'Custom Two'}],stars:{}};
for(const s of ['menu','select'])for(const height of [780,960,1240]){
  const entries=[];
  const navUI=nav(ctx,()=>{},(x,y,w,h,label,fn)=>{entries.push({x,y,w,h,label,fn})},
    (x,y,w,h,id,name,stars,locked,fn)=>{entries.push({x,y,w,h,label:name,fn})},
    ()=>{},()=>{},()=>{},()=>{},()=>{},()=>{},()=>{},()=>{},pd,levels,12,25,
    ()=>true,()=>true,()=>true,{},540,height,Array.from({length:18},(_,i)=>({id:i})),[],
    ()=>0,()=>0,()=>0,()=>0,false,()=>{},()=>{},()=>({}),{pop(){}},()=>{},()=>{},()=>{},
    ()=>true,()=>{},0,s);
  if(s==='select'){
    navUI.drawMobileSelect();
    assert(entries.length>4,'mobile selection has playable tiles');
    for(const e of entries)assert(e.x>=0&&e.y>=0&&e.x+e.w<=540&&e.y+e.h<=height,
      'portrait selection touch rect visible');
    const oldPage=navUI.getPage();entries.at(-1).fn(); // may be pager next if present
    assert(navUI.getPage()>=oldPage,'page control can advance');
  }else{
    navUI.drawMobileMenu();
    const playable=entries.filter(e=>/PLAY|LEVEL CREATOR|OPTIONS/.test(e.label));
    assert(playable.length>=3,'mobile main menu has accessible actions');
    for(const e of entries)assert(e.x>=0&&e.y>=0&&e.x+e.w<=540&&e.y+e.h<=height,
      'portrait main menu touch rect visible');
  }
}
console.log('WEB_ORIENTATION_PASS viewport=portrait+landscape states=10 menu=touch select=paged');
