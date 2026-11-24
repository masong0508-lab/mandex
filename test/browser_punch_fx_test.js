/* Actual-source browser regression: the punch indicator stays on the
 * live attack rectangle, even when the sprite is still facing backwards. */
'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
function section(a,b){
  const i=html.indexOf(a),j=html.indexOf(b,i+a.length);
  assert(i>=0&&j>i,'missing production source '+a);
  return html.slice(i,j);
}
const punch=section('function walkPunch(){','function walkSlam(');
const painter=section('function walkTurnAngle(','/* Two fingers on the same virtual key');
const harness=`
  let man,time=0,JIG=null,hit=null;
  const V=8,WHH=19,cases=[],SFX={whoosh(){}};
  function walkHit(cx,cy,hw,hh){hit={cx,cy,hw,hh};return 0;}
  let strokes=[],draws=[];
  const ctx={
    globalAlpha:1,depth:0,transformed:false,stack:[],points:[],
    save(){this.stack.push({a:this.globalAlpha,t:this.transformed});this.depth++},
    restore(){const v=this.stack.pop();this.globalAlpha=v.a;this.transformed=v.t;this.depth--},
    beginPath(){this.points=[]},ellipse(){},fill(){},
    translate(){this.transformed=true},scale(){this.transformed=true},rotate(){this.transformed=true},
    moveTo(x,y){this.points.push([x,y])},lineTo(x,y){this.points.push([x,y])},
    stroke(){strokes.push({points:this.points.slice(),depth:this.depth,transformed:this.transformed,alpha:this.globalAlpha})}
  };
  function drawSpin(theta,pose){draws.push({theta,hip:pose.hip})}
`;
const checks=`
  let n=0,oppositeTurns=0;
  function ok(c,msg){assert(c,msg);n++}
  function near(a,b,msg){ok(Math.abs(a-b)<1e-7,msg+' '+a+' vs '+b)}
  for(const face of [-1,1])for(let turn=0;turn<=12;turn++)for(const gr of [false,true]){
    man={mode:'walk',x:320,y:260,face,turn,gr,vx:75,vy:-40,gait:12,pT:0,pCd:0,stomp:false};
    if(turn>6&&walkTurnFace(face,turn)!==face)oppositeTurns++;
    walkPunch();
    near(hit.cx,man.x+face*17,'actual strike centre follows new gameplay face');
    near(hit.cy,man.y-3,'actual strike height');
    for(const [t,stage] of [[.2,3],[.11,2],[.04,1],[.001,1],[0,0],[-.001,0]]){
      man.pT=t;strokes=[];draws=[];
      drawWalkMan(man);
      ok(draws.length===1,'character painted once');
      ok(ctx.depth===0&&!ctx.transformed,'restore render transform');
      near(ctx.globalAlpha,1,'restore effect opacity');
      ok(walkPunchFxStage(man)===stage,'recovery stage');
      ok(strokes.length===(stage?1:0),'show only during live attack');
      if(!stage)continue;
      const s=strokes[0];
      ok(!s.transformed&&s.depth===1,'sweep painted in world space, outside mirrored sprite transform');
      near(s.alpha,stage/3,'effect fades during recovery');
      ok(s.points.length===2*stage+1,'bounded point count');
      for(let i=0;i<s.points.length;i++){
        const [x,y]=s.points[i],k=i-stage;
        near(x,hit.cx+face*hit.hw*(1-k*k/9),'sweep follows the real attack reach');
        near(y,hit.cy+hit.hh*k/3,'sweep follows the real attack height');
        ok(Math.abs(x-hit.cx)<=hit.hw+1e-7&&Math.abs(y-hit.cy)<=hit.hh+1e-7,'sweep within actual strike bounds');
        ok(Math.sign(x-man.x)===face,'effect never follows the old mirrored silhouette');
      }
    }
  }
  ok(oppositeTurns>0,'countersteering fixtures exercise stale visual facing');
  man.mode='fly';man.pT=.2;
  ok(walkPunchFxStage(man)===0,'cannot inherit attack in flight');
  return {checks:n,oppositeTurns};
`;
const result=new Function('assert',harness+punch+painter+checks)(assert);
for(const p of ['ds/source/main.c','3ds/main.c','dreamcast/main.c']){
  const code=fs.readFileSync(path.join(__dirname,'..',p),'utf8');
  assert(code.includes('mx_walk_punch_fx_stage(m)')&&code.includes('mx_walk_punch_fx_point(m,k,&dx,&dy)'),p+' uses common attack-facing geometry');
}
console.log('BROWSER_PUNCH_FX_PASS checks='+result.checks+' opposite_turns='+result.oppositeTurns+' native_wiring=3');
