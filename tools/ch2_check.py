#!/usr/bin/env python3
"""
Headless checks for the chapter 2 levels of web/index.html (needs Playwright; uses the real update() loop with a fixed RNG seed).

  python3 tools/ch2_check.py [--html web/index.html] [--chrome /usr/bin/google-chrome]

  1. every chapter 2 level loads, stands untouched for 10 s (nothing lost, no chunks) and raise no page errors
  2. LV21 Special Delivery: sweeps every pull; no shot that skips the tubes may kill anybody, each of the 3 tubes must be reachable and
     kill its room of 3, and a 3-shot sequence must win with 3 men left
  3. LV22 Hourglass Heights: popping the plug must flood the steel bulb and kill its 3 landlords within 8 s, and some real pull must pop it
  4. LV24 Over the Top / LV25 Needle Threader (pure slingshot): sweeps every pull; most pulls must kill nobody (the steel stops flat shots), a real
     lob must clear the wall, a precise one-shot solution wins, and a 3-shot route wins
  5. the level select: chapter 2 tab draws, its first level is open, and chapter 2 stars stay out of the story counters
Exit code 1 on any failure.
"""
import argparse, os, sys
from playwright.sync_api import sync_playwright

HERE = os.path.dirname(os.path.abspath(__file__))
BOOT = "()=>{window.requestAnimationFrame=()=>0; let s=12345; Math.random=()=>{s=(s*1664525+1013904223)>>>0;return s/4294967296}}"
STAND = """(L)=>{load(L);const c0=grid.reduce((n,v)=>n+(v?1:0),0);for(let i=0;i<600;i++)update(1/60);draw();
  return {name:CL.name,same:c0===grid.reduce((n,v)=>n+(v?1:0),0),chunks:chunks.length,alive:aliveTargets(),n:targets.length}}"""
SWEEP = """([L,step])=>{const out=[];
 for(let px=-92;px<=-8;px+=step)for(let py=-92;py<=92;py+=step){if(Math.hypot(px,py)>92)continue;
  load(L);for(let i=0;i<20;i++)update(1/60);man.x=SX+px;man.y=SY+py;launch();
  let used='',after=0;
  for(let f=0;f<900;f++){update(1/60);
    if(man.tubeCd>.3){const k=tubes.findIndex(t=>Math.hypot(man.x-t.bx,man.y-t.by)<30);if(k>=0&&used[used.length-1]!==String(k))used+=k}
    if(man.mode==='stuck'||man.mode==='done'||man.mode==='wait')after++;if(after>40)break}
  out.push([px,py,used,aliveTargets(),targets.length])}
 return out}"""
PLAY = """(shots)=>{load(20);for(let i=0;i<20;i++)update(1/60);
 for(const [px,py] of shots){let g=0;while(man.mode!=='aim'&&g++<1200)update(1/60);
  man.x=SX+px;man.y=SY+py;launch();let f=0;while((man.mode==='fly'||man.mode==='stuck')&&f++<1500)update(1/60)}
 for(let i=0;i<400;i++)update(1/60);return {state,men,alive:aliveTargets()}}"""
FLOOD = """()=>{load(21);for(let i=0;i<30;i++)update(1/60);const n0=aliveTargets();
 // the plug is the wooden block in the neck: pop it with a keg
 explQ.push([79,35]);for(let i=0;i<480;i++)update(1/60);
 const bulb=targets.filter(t=>t.x>64*8&&t.x<96*8).length;
 return {n0,alive:aliveTargets(),bulbLeft:targets.filter(t=>t.alive&&t.x>64*8&&t.x<96*8).length,bulb}}"""
POP = """([step])=>{let hits=0,best=[];
 for(let px=-92;px<=-8;px+=step)for(let py=-92;py<=92;py+=step){if(Math.hypot(px,py)>92)continue;
  load(21);for(let i=0;i<20;i++)update(1/60);man.x=SX+px;man.y=SY+py;launch();
  let after=0;for(let f=0;f<900;f++){update(1/60);if(man.mode==='stuck'||man.mode==='done'||man.mode==='wait')after++;if(after>40)break}
  const a1=aliveTargets();for(let f=0;f<480;f++)update(1/60);
  if(aliveTargets()<a1){hits++;if(best.length<3)best.push([px,py,a1,aliveTargets()])}}
 return {hits,best}}"""
KILLS = """([L,step])=>{const out=[];
 for(let px=-92;px<=-8;px+=step)for(let py=-92;py<=92;py+=step){if(Math.hypot(px,py)>92)continue;
  load(L);for(let i=0;i<20;i++)update(1/60);man.x=SX+px;man.y=SY+py;launch();
  let after=0;for(let f=0;f<900;f++){update(1/60);if(man.mode==='stuck'||man.mode==='done'||man.mode==='wait')after++;if(after>40)break}
  for(let f=0;f<300;f++)update(1/60);out.push([px,py,targets.length-aliveTargets()])}
 return out}"""
PLAY2 = """([L,shots])=>{load(L);for(let i=0;i<20;i++)update(1/60);
 for(const [px,py] of shots){let g=0;while(man.mode!=='aim'&&g++<1500&&state==='play')update(1/60);if(state!=='play')break;
  man.x=SX+px;man.y=SY+py;launch();let f=0;while((man.mode==='fly'||man.mode==='stuck')&&f++<1500)update(1/60)}
 for(let i=0;i<400;i++)update(1/60);return {state,men,alive:aliveTargets()}}"""
SELECT = """()=>{PD.unlockAll=false;PD.stars={};selTab=2;state='select';draw();const a=[ch2Unlocked(20),ch2Unlocked(21)];
 PD.stars[20]=3;PD.stars[21]=3;const b=[totalStars(),storyDone(),threeStarCount()];PD.stars={};return {a,b}}"""

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--html', default=os.path.join(HERE, '..', 'web', 'index.html'))
    ap.add_argument('--chrome', default=None)
    a = ap.parse_args(); fails = []
    def ok(c, msg):
        print(('ok   ' if c else 'FAIL ') + msg)
        if not c: fails.append(msg)
    with sync_playwright() as p:
        kw = {'executable_path': a.chrome} if a.chrome else {}
        b = p.chromium.launch(args=['--no-sandbox'], **kw)
        pg = b.new_page(viewport={'width': 960, 'height': 540}); errs = []
        pg.on('pageerror', lambda e: errs.append(str(e)))
        pg.goto('file://' + os.path.abspath(a.html)); pg.wait_for_timeout(300); pg.evaluate(BOOT)
        n, ch1 = pg.evaluate("()=>[LEVELS.length,CH1]"); ok(n == 27 and ch1 == 20, f'{n} levels, chapter 1 = {ch1}')
        for L in (20, 21, 23, 24, 25, 26):
            r = pg.evaluate(STAND, L); ok(r['same'] and r['chunks'] == 0 and r['alive'] == r['n'], f"LV{L+1} {r['name']} stands for 10 s ({r['n']} landlords)")
        r = pg.evaluate(SWEEP, [20, 4]); skip = [x for x in r if x[2] == '' and x[3] < x[4]]
        ok(not skip, f"LV21: {sum(1 for x in r if x[2]=='')} shots that miss every tube kill nobody" + (f' (but {len(skip)} did)' if skip else ''))
        for k in range(3):
            v = [x for x in r if x[2] == str(k)]; ok(bool(v) and all(x[3] <= x[4]-3 for x in v), f'LV21: tube {k+1} reachable by {len(v)} pulls, each kills a room of 3')
        r = pg.evaluate(PLAY, [[-84, 28], [-64, 60], [-36, 80]]); ok(r['state'] == 'win' and r['men'] == 3, f'LV21: three tube shots win with {r["men"]} men left')
        r = pg.evaluate(FLOOD); ok(r['bulbLeft'] == 0, f"LV22: popping the plug buries the bulb landlords ({r['n0']} -> {r['alive']} alive)")
        r = pg.evaluate(POP, [4]); ok(r['hits'] > 0, f"LV22: {r['hits']} real pulls pop the plug (alive drops while the sand runs)")
        r = pg.evaluate(KILLS, [23, 4]); n = len(r)
        ok(0 < sum(1 for x in r if x[2] >= 3) <= n * .05 and sum(1 for x in r if x[2] == 0) > n * .9, f'LV24: only a narrow lob clears the wall ({sum(1 for x in r if x[2]>=3)} of {n} pulls kill 3+, {sum(1 for x in r if x[2]==0)} kill nobody)')
        ok(all(x[1] > 0 for x in r if x[2] > 0), 'LV24: nothing dies to a pull that is level or aimed down')
        r = pg.evaluate(PLAY2, [23, [[-44, 80]]]); ok(r['state'] == 'win' and r['men'] == 5, f'LV24: the one precise lob wins ({r})')
        r = pg.evaluate(PLAY2, [23, [[-52, 68], [-36, 72], [-36, 72]]]); ok(r['state'] == 'win' and r['men'] >= 3, f'LV24: tower shot + two hut lobs wins ({r})')
        r = pg.evaluate(KILLS, [24, 4]); n = len(r)
        ok(sum(1 for x in r if x[2] >= 1) >= 20 and sum(1 for x in r if x[2] == 0) > n * .85, f'LV25: the slots are threadable but the steel stops the rest ({sum(1 for x in r if x[2]>=1)} of {n} pulls kill, {sum(1 for x in r if x[2]==0)} kill nobody)')
        r = pg.evaluate(PLAY2, [24, [[-76, 44], [-80, 44]]]); ok(r['state'] == 'win' and r['men'] >= 3, f'LV25: two slot shots win ({r})')
        # ---- landlords fight back ----
        r = pg.evaluate("""()=>{load(25);const xs=targets.map(t=>t.x);for(let i=0;i<60*12;i++)update(1/60);
          const ok=targets.every((t,i)=>t.alive&&Math.abs(t.x-t.hx)<=t.range+9&&solidAt(t.x,t.y+t.r+1));
          const moved=targets.filter((t,i)=>Math.abs(t.x-xs[i])>6).length;
          OPT().fight=false;load(25);const x1=targets.map(t=>t.x);for(let i=0;i<60*6;i++)update(1/60);const still=targets.every((t,i)=>t.x===x1[i]);OPT().fight=true;
          return{ok,moved,still,n:targets.length}}""")
        ok(r['ok'] and r['moved'] >= 4 and r['still'], f'LV26: walkers pace their own deck and stay on it, and stand still with fight-back off ({r})')
        r = pg.evaluate("""()=>{load(25);const t=targets[2];man={mode:'fly',x:t.x+56,y:t.y-4,vx:100,vy:0,R:14,t:0,a:1,hitCd:0,gooT:0,stT:0,swT:0,pop:1,G:gagNew(2.5,4,3),jg:jigNew(),inf:0,pwr:null,bel:0};enterWalk();
          let minwt=99,swung=0;for(let i=0;i<60*3;i++){update(1/60);minwt=Math.min(minwt,man.wt);if(targets.some(q=>q.swing>0))swung=1}
          const w0=man.wt;return{swung,wt:man.wt,mode:man.mode,alive:aliveTargets()}}""")
        ok(r['swung'] == 1 and r['wt'] < 45 - 3 - 2, f'LV26: a walker closes in and clubs a man on foot (time lost beyond the clock: {r})')
        r = pg.evaluate("""()=>{load(26);let thrown=0,dazed=0,s0=0;for(let i=0;i<60*14;i++){const n=cases.length;update(1/60);if(cases.length>n)thrown++;if(man.stun>0)dazed=1}
          OPT().fight=false;load(26);let t2=0;for(let i=0;i<60*14;i++){const n=cases.length;update(1/60);if(cases.length>n)t2++}OPT().fight=true;
          return{thrown,dazed,off:t2}}""")
        ok(r['thrown'] >= 2 and r['dazed'] == 1 and r['off'] == 0, f'LV27: throwers lob briefcases at the sling and daze the man; none with fight-back off ({r})')
        r = pg.evaluate("""()=>{let bonk=0,best=null;for(let px=-60;px<=-8;px+=4)for(let py=-30;py<=60;py+=6){if(Math.hypot(px,py)>58)continue;load(26);for(let i=0;i<20;i++)update(1/60);man.x=SX+px;man.y=SY+py;launch();let f=0,st=0;
            for(let f=0;f<400;f++){update(1/60);if(targets[0].stun>0||targets[1].stun>0)st=1;if(man.mode!=='fly')break}
            if(st){bonk++;if(!best)best=[px,py,targets[0].alive&&targets[1].alive]}}
          return{bonk,best}}""")
        ok(r['bonk'] >= 3 and r['best'] and r['best'][2], f'LV27: a weak shot at a bat guy is swatted away and he survives ({r})')
        r = pg.evaluate("""()=>{let wins=[];for(const [px,py] of [[-76,44],[-72,48],[-72,44],[-68,48],[-76,48],[-72,52]]){load(26);for(let i=0;i<20;i++)update(1/60);man.x=SX+px;man.y=SY+py;launch();for(let i=0;i<900;i++)update(1/60);if(state==='win')wins.push([px,py])}return wins}""")
        ok(len(r) >= 1, f'LV27: a hard, steep pull ploughs through the bats and clears the fort in one shot ({r})')
        r = pg.evaluate(SELECT); ok(r['a'] == [True, False] and r['b'] == [0, 0, 0], f'select: chapter 2 opens at 21, its stars stay out of the story counters {r}')
        ok(not errs, 'no page errors' + (': ' + errs[0] if errs else '')); b.close()
    sys.exit(1 if fails else 0)
main()
