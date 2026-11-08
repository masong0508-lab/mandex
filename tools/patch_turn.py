#!/usr/bin/env python3
"""One-off patch applied to web/index.html: the cape-taunt turn becomes a real step-turn (feet that hop round and stay planted, a
2-bone IK for the legs, hips / shoulders / head out of phase, a crouch while a foot is in the air) instead of a rigid revolve.
Usage: python3 tools/patch_turn.py path/to/index.html   (idempotent: refuses to patch twice)"""
import sys, re
p = sys.argv[1]
s = open(p).read()
if "function spinSway" in s: sys.exit("already patched")

def sub(old, new, count=1):
    global s
    assert old in s, "anchor not found: " + old[:70]
    s = s.replace(old, new, count)

# ---- 1. the pose table: 13 frames forward (every 15 deg), 11 on the way back, plus the two butt-wiggle frames
i0 = s.index("const SPIN_POSE={"); i1 = s.index("function drawSpin(th,o){")
had_hand = "TOWEL_HAND" in s[i0:i1]     # the cloth patch keeps its hand table in this region: it has to be written again (now with the sway/crouch in it)
sub(s[i0:i1], '''// every 15 degrees of the turn: SPIN_0..SPIN_12 going round (0 = facing us, 180 = back to us), SPIN_R1..SPIN_R11 coming back.
// ld = how far the head is ahead of the shoulders, psi = how far the hips lag behind them (both flip when he turns back, and die out at the ends).
const spinKey=(th,dir)=>{const m=Math.min(1,th/20,(180-th)/20);return{th,dir,ld:12*dir*m,psi:-8*dir*m}};
const SPIN_POSE={};
for(let k=0;k<=12;k++)SPIN_POSE['SPIN_'+k]=spinKey(k*15,1);
for(let k=1;k<=11;k++)SPIN_POSE['SPIN_R'+k]=spinKey(k*15,-1);
SPIN_POSE.SPIN_BL={th:180,dir:1,hip:-6,psi:-10,cl:-2.5,cr:2.5};SPIN_POSE.SPIN_BR={th:180,dir:1,hip:6,psi:10,cl:2.5,cr:-2.5};
// ---- the footwork.  Each foot sits on the floor at an angle (degrees) round the body centre and only moves in a quick hop:
// right foot 0->90 while the turn is at 15..45 deg and 90->180 at 105..135, left foot 0->90 at 45..75 and 90->180 at 135..165.
const hopP=(th,a,b)=>Math.max(0,Math.min(1,(th-a)/(b-a))),smo=u=>u*u*(3-2*u);
function spinFeet(th){
  const f=(a1,b1,a2,b2)=>{const u1=hopP(th,a1,b1),u2=hopP(th,a2,b2);
    return{ang:90*(smo(u1)+smo(u2)),lift:7*((u1>0&&u1<1?Math.sin(Math.PI*u1):0)+(u2>0&&u2<1?Math.sin(Math.PI*u2):0))}};
  return{R:f(15,45,105,135),L:f(45,75,135,165)}}
// where the body sits over those feet: it leans toward the foot that stays on the floor and crouches (never further than the legs can reach)
function spinSway(thDeg,psiDeg,hipSway){
  const FT=spinFeet(thDeg),hy=(thDeg+psiDeg)*Math.PI/180,Ch=Math.cos(hy),Sh=Math.sin(hy),oLs=(hipSway||0)*.6;
  const P=(s,F)=>{const a=F.ang*Math.PI/180;return[s*14*Math.cos(a),-s*14*Math.sin(a)]};
  const lr=Math.max(FT.R.lift,FT.L.lift)/7;
  const sup=FT.R.lift>FT.L.lift?P(-1,FT.L):FT.L.lift>FT.R.lift?P(1,FT.R):[0,0];
  const shx=Math.max(-4,Math.min(4,.3*sup[0]))*lr;
  let bob=.8+1.4*lr;
  for(const s of[-1,1]){const F=s>0?FT.R:FT.L,p=P(s,F),dx=p[0]-(s*10*Ch+oLs+shx),dz=p[1]+s*10*Sh,h=Math.hypot(dx,dz),v=Math.sqrt(Math.max(0,(.965*31.3)**2-h*h));
    bob=Math.max(bob,(55-F.lift-v)-24)}
  return{bob,shx,F:FT}}
// two-bone leg: hip H and ankle A (3D), the knee bends toward `pole`
function legIK(H,A,pole,l1,l2){
  let d=[A[0]-H[0],A[1]-H[1],A[2]-H[2]],D=Math.hypot(d[0],d[1],d[2]);const mx=l1+l2-.15;let Ac=A;
  if(D>mx){const k=mx/D;d=d.map(v=>v*k);D=mx;Ac=[H[0]+d[0],H[1]+d[1],H[2]+d[2]]}
  const u=d.map(v=>v/D),a=(l1*l1-l2*l2+D*D)/(2*D),h=Math.sqrt(Math.max(0,l1*l1-a*a)),pd=pole[0]*u[0]+pole[1]*u[1]+pole[2]*u[2];
  let q=[pole[0]-pd*u[0],pole[1]-pd*u[1],pole[2]-pd*u[2]];const ql=Math.hypot(q[0],q[1],q[2])||1;q=q.map(v=>v/ql);
  return{K:[H[0]+u[0]*a+q[0]*h,H[1]+u[1]*a+q[1]*h,H[2]+u[2]*a+q[2]*h],A:Ac}}
''' + ('''// the baked hand anchor (design units, no arm wave) of every frame that carries a towel: the consoles pin their cloth to it.
// For the spin it is where drawSpin puts the raised fist: 30 out from the body centre, turned by th, plus the sway (shx) and crouch (bob) of the step-turn.
const TOWEL_HAND=(()=>{const r=v=>Math.round(v*1000)/1000+0,H={};
  for(const n in SPIN_POSE){const q=SPIN_POSE[n],th=q.th*Math.PI/180,oU=-(q.hip||0)*.35,sw=spinSway(q.th,q.psi||0,q.hip||0);H[n]=[r(30*Math.cos(th)+oU+sw.shx),r(-69+sw.bob),r(-30*Math.sin(th))]}
  for(const[n,hip]of[['BACK_C',0],['BACK_L',-6],['BACK_R',6],['FART_A',-1.5],['FART_B',1.5]])H[n]=[r(38-hip*.35),-58,0];
  return H})();
''' if had_hand else ''))

# ---- 2. options: head lead
sub("const hip=o.hip||0,cl=o.cl||0,cr=o.cr||0,psi=(o.psi||0)*Math.PI/180,",
    "const hip=o.hip||0,cl=o.cl||0,cr=o.cr||0,psi=(o.psi||0)*Math.PI/180,ld=(o.ld||0)*Math.PI/180,")
sub("const C=Math.cos(th),S=Math.sin(th),C2=Math.cos(th+psi),S2=Math.sin(th+psi);",
    "const C=Math.cos(th),S=Math.sin(th),C2=Math.cos(th+psi),S2=Math.sin(th+psi),Ch=Math.cos(th+ld),Sh=Math.sin(th+ld);")
sub("const add=(d,f)=>{list.push({d,f,n:seq++})};",
    "const add=(d,f)=>{list.push({d,f,n:seq++})},addLeg=(d,f)=>{list.push({d,f,n:seq++,leg:1})};   // legs are not shifted by the sway/crouch (their hips already are)\n  const SW=spinSway(th*180/Math.PI,(o.psi||0),hip),bob=SW.bob,shx=SW.shx;")
# G3 takes q.head
sub("const G3=(x,y,z,a,b,c,q)=>{q=q||{};const cc=q.low?C2:C,ss=q.low?S2:S,",
    "const G3=(x,y,z,a,b,c,q)=>{q=q||{};const cc=q.head?Ch:q.low?C2:C,ss=q.head?Sh:q.low?S2:S,")

# ---- 3. legs
j0 = s.index("  // ---------------- legs: feet stay planted"); j1 = s.index("  // ---------------- arms: left fist on the hip")
sub(s[j0:j1], '''  // ---------------- legs: each foot is planted where it was put; a 2-bone IK finds the knee from the (swaying, crouching) hip down to it
  for(const s of[-1,1]){
    const F=s>0?SW.F.R:SW.F.L,fa=F.ang*Math.PI/180,Cf=Math.cos(fa),Sf=Math.sin(fa);
    const Hh=[s*10*C2+oL+shx,24+bob,-s*10*S2],Aa=[s*14*Cf,55-F.lift,-s*14*Sf];
    const ik=legIK(Hh,Aa,[S2+s*.25*C2,0,C2-s*.25*S2],15.3,16),K=ik.K,A=ik.A;
    const sx=A[0]+(K[0]-A[0])*.32,sy=A[1]+(K[1]-A[1])*.32,sPx=s*15.5*Cf+3*Sf,sPz=-s*15.5*Sf+3*Cf,sRx=Math.hypot(10.5*Cf,10*Sf),sy0=60-F.lift;
    const shoe={x:s*15.5,y:sy0,z:3,a:10.5,b:4.4,c:10,cc:Cf,ss:Sf,ox:0,oy:0};
    addLeg(-50+.02*(Hh[2]+sPz),()=>{
      limb(Hh[0],Hh[1],K[0],K[1],16,SK,SKH);limb(K[0],K[1],A[0],A[1],10.5,SK,SKH);
      ctx.fillStyle=HAIR;for(let k=0;k<5;k++){const xo=(k%2)*3-3,zo=Math.sqrt(64-xo*xo),fc=(-xo*S2+zo*C2)/8,yy=42+k*2.4,t=(yy-K[1])/((A[1]-K[1])||1);if(fc>.15&&t>0&&t<1)ctx.fillRect(K[0]+(A[0]-K[0])*t+xo*C2+zo*S2-.6,yy,1.2,1.2)}
      limb(sx,sy,A[0],A[1],12,'#fff');
      ctx.strokeStyle='#e33';ctx.lineWidth=1.6;ctx.lineCap='butt';ctx.beginPath();ctx.moveTo(sx-6,sy+1);ctx.lineTo(sx+6,sy+1);ctx.moveTo(sx-6,sy+4);ctx.lineTo(sx+6,sy+4);ctx.stroke();ctx.lineCap='round';
      ell(sPx,sy0,sRx,4.4,'#8a5a2b',1.8);shade(sPx,sy0,sRx,4.4,.16,.18,'rgba(60,30,10,');
      dec([shoe],s*15.5,sy0-2.6,()=>{ctx.strokeStyle='#5a3515';ctx.lineWidth=1.6;ctx.beginPath();ctx.moveTo(-4,0);ctx.lineTo(4,0);ctx.stroke()})})}

''')

# ---- 4. head turns by its own angle (ahead of the shoulders)
sub("const U={ox:oU},A=15,B=14.2,Cc=14.5,hcy=-44.5;", "const U={ox:oU,head:1},A=15,B=14.2,Cc=14.5,hcy=-44.5,C=Ch,S=Sh;                 // the head has its own turn angle (C,S here are the head's)")
sub("E(0,-31.5,0,10,6,9,SKD,0,{ox:oU,sh:.08,hl:0,d:-900});","E(0,-31.5,0,10,6,9,SKD,0,{ox:oU,head:1,sh:.08,hl:0,d:-900});")
sub("const jaw=E(0,-34,2,11.2,8.2,11,SK,1.6,{ox:oU,hl:.15,d:-800});","const jaw=E(0,-34,2,11.2,8.2,11,SK,1.6,{ox:oU,head:1,hl:.15,d:-800});")
sub("const hd=E(0,-44,0,14.3,13.3,13.5,SK,2,{ox:oU,d:0});","const hd=E(0,-44,0,14.3,13.3,13.5,SK,2,{ox:oU,head:1,d:0});")

# ---- 5. draw: everything except the legs sways / crouches
sub("list.sort(cmp);for(const p of list)p.f();\n  ctx.restore()}", "list.sort(cmp);for(const p of list){if(p.leg)p.f();else{ctx.save();ctx.translate(shx,bob);p.f();ctx.restore()}}\n  ctx.restore()}")

# ---- 6. frame metadata + the gag
names = ["SPIN_%d" % k for k in range(13)] + ["SPIN_R%d" % k for k in range(1, 12)] + ["SPIN_BL", "SPIN_BR"]
i0 = s.index("SPIN_0:_F(.36,[0,-44])"); i1 = s.index("BACK_C:_F(.36,null)")
sub(s[i0:i1], ",".join("%s:_F(.36,[0,-44])" % n for n in names) + ",")
g0 = s.index(" {name:'cape_taunt',steps:["); g1 = s.index("]}];", g0) + 2
steps = "['CAPE_A',.35,'grunt'],['SPIN_0',.08,'whoosh']," + ",".join("['SPIN_%d',.05,'']" % k for k in range(1, 13)).replace("['SPIN_1',.05,'']", "['SPIN_1',.05,'']")
steps += ",['SPIN_BL',.15,'taunt'],['SPIN_BR',.15,'boing']" + ",['SPIN_BL',.15,'boing'],['SPIN_BR',.15,'boing']" * 3
steps += ",['SPIN_R11',.05,'whoosh']," + ",".join("['SPIN_R%d',.05,'']" % k for k in range(10, 0, -1)) + ",['SPIN_0',.06,''],['CAPE_A',.3,''],['IDLE_A',.2,'']"
sub(s[g0:g1], " {name:'cape_taunt',steps:[" + steps + "]}")
open(p, "w").write(s)
print("patched", p)
