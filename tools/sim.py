#!/usr/bin/env python3
"""
Headless flight/landing sim for web/index.html (needs Playwright).  Fires the man at a level with a fixed RNG seed, steps the real
update()/draw() loop, and either writes a contact sheet around him (default) or checks the body physics (--check).

  python3 tools/sim.py  [--level 1] [--pull -60 20] [--after 14] [--dt 60] [--no-fx] [--out sim.png] [--html web/index.html]
  python3 tools/sim.py  --check            # sweep levels x pulls x frame rates; exit 1 on any NaN / stretched bone / limb inside a wall

--pull X Y   where he is dragged to, relative to the sling (SX,SY) before launch.  --dt N steps the sim at N updates per second.
--no-fx      empties particles/score text every frame so the contact sheet shows just the body.
"""
import argparse, base64, io, os, sys
from playwright.sync_api import sync_playwright
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_HTML = os.path.join(HERE, '..', 'web', 'index.html')

BOOT = "()=>{window.requestAnimationFrame=()=>0; let s=12345; Math.random=()=>{s=(s*1664525+1013904223)>>>0;return s/4294967296}}"
STEP = """([dt,nofx])=>{update(dt);if(nofx){parts.length=0;texts.length=0}draw();
  const g=man.jg,r=man.rg;
  return {mode:man.mode,x:man.x,y:man.y,j:g?[g.bx,g.by,g.rx,g.ry]:null,
          sk:g&&g.sk?g.sk.x.concat(g.sk.y):null,
          rg:r?r.P.map(c=>c.map(p=>[p.x,p.y])):null,rk:r?r.k:null,
          sc:(()=>{try{const X=ragXf(man);return X?(X.sx+X.sy)/2:null}catch(e){return null}})(),
          wall:r?r.P.map(c=>[solidAt(c[1].x,c[1].y),solidAt(c[2].x,c[2].y)]):null}}"""

def open_page(pw, html, level, px, py):
    b = pw.chromium.launch(); pg = b.new_page(viewport={'width': 960, 'height': 540})
    errs = []
    pg.on('pageerror', lambda e: errs.append(str(e)))
    pg.on('console', lambda m: errs.append(m.text) if m.type == 'error' else None)
    pg.goto('file://' + os.path.abspath(html)); pg.wait_for_timeout(200)
    pg.evaluate(BOOT); pg.wait_for_timeout(80)
    pg.evaluate(f"()=>{{load({level});for(let i=0;i<30;i++)update(1/60);man.x=SX+({px});man.y=SY+({py});launch()}}")
    return b, pg, errs

# rest bone lengths of the stuck pose (RAG_ST in index.html), used to check the verlet constraints hold
REST = [((-21.5,-20.5),(-40,-28),(-57,-36)), ((21.5,-20.5),(40,-28),(57,-36)), ((-10,24),(-17,39),(-26,55)), ((10,24),(17,39),(26,55))]
def bones(c, sc):
    (a, b, e) = REST[c]
    d = lambda p, q: ((p[0]-q[0])**2 + (p[1]-q[1])**2) ** .5
    return d(a, b) * sc, d(b, e) * sc

def run(html, level, px, py, dt, after, nofx, grab):
    """returns (frames, problems).  frames = [(step, state, PIL image or None)]"""
    probs, frames, landed = [], [], None
    with sync_playwright() as pw:
        b, pg, errs = open_page(pw, html, level, px, py)
        for step in range(int(10 / dt)):
            r = pg.evaluate(STEP, [dt, nofx])
            if landed is None and r['mode'] == 'stuck': landed = step
            want = grab and ((landed is not None and step - landed <= after) or (landed is None and step % 25 == 0 and r['mode'] == 'fly'))
            if want:
                u = pg.evaluate("()=>cv.toDataURL('image/png')")
                frames.append((step, r, Image.open(io.BytesIO(base64.b64decode(u.split(',')[1]))).convert('RGB')))
            else:
                frames.append((step, r, None)) if not grab else None
            # ---- checks ----
            for k in (r['sk'] or []):
                if abs(k) > 1.45 or k != k: probs.append(f'step {step}: belly skin point out of range ({k})'); break
            if r['rg']:
                for ci, chain in enumerate(r['rg']):
                    for p in chain[1:]:
                        if p[0] != p[0] or p[1] != p[1] or abs(p[0]) > 1e5: probs.append(f'step {step}: limb {ci} not finite'); break
                    if r['rk'] == 'st' and r['sc'] and landed is not None and (step - landed) * dt > .15:   # first moments are the landing squash
                        L1, L2 = bones(ci, r['sc'])
                        d1 = ((chain[1][0]-chain[0][0])**2 + (chain[1][1]-chain[0][1])**2) ** .5
                        d2 = ((chain[2][0]-chain[1][0])**2 + (chain[2][1]-chain[1][1])**2) ** .5
                        if abs(d1-L1) > .25*L1 + 1 or abs(d2-L2) > .35*L2 + 1:
                            probs.append(f'step {step}: limb {ci} bone stretched ({d1:.1f}/{L1:.1f}, {d2:.1f}/{L2:.1f})')
                    if r['rk'] == 'st' and r['wall'] and any(r['wall'][ci]) and step - (landed or 0) > 20:
                        probs.append(f'step {step}: limb {ci} stuck inside a solid voxel')
            if landed is not None and step - landed > after: break
        b.close()
    probs = errs + probs
    return frames, landed, probs

def sheet(frames, out):
    tiles = []
    for s, r, im in frames:
        if im is None: continue
        cx, cy = int(r['x']), int(r['y']); x0, y0 = max(0, cx-75), max(0, cy-80)
        tiles.append(im.crop((x0, y0, x0+150, y0+130)).resize((450, 390), Image.LANCZOS))
    if not tiles: print('no frames to draw'); return
    cols = 4; rows = (len(tiles)+cols-1)//cols
    sh = Image.new('RGB', (cols*450, rows*390), (30, 30, 30))
    for i, t in enumerate(tiles): sh.paste(t, ((i % cols)*450, (i//cols)*390))
    sh.save(out); print('saved', out, len(tiles))

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--html', default=DEFAULT_HTML); ap.add_argument('--level', type=int, default=1)
    ap.add_argument('--pull', type=float, nargs=2, default=[-60, 20]); ap.add_argument('--after', type=int, default=14)
    ap.add_argument('--dt', type=float, default=60, help='sim updates per second (default 60)')
    ap.add_argument('--no-fx', action='store_true'); ap.add_argument('--out', default='sim.png'); ap.add_argument('--check', action='store_true')
    a = ap.parse_args()
    if not os.path.exists(a.html): sys.exit('page not found: ' + a.html)
    if a.check:
        bad = 0; n = 0
        for lvl in (0, 1, 3, 5):
            for pull in ((-60, 20), (-85, 40), (-40, -10)):
                for fps in (30, 60, 120):
                    _, landed, probs = run(a.html, lvl, pull[0], pull[1], 1/fps, 90 * fps // 60, True, False)
                    n += 1; tag = f'level {lvl} pull {pull} @{fps}fps'
                    if probs: bad += 1; print('FAIL', tag, '|', probs[0], f'(+{len(probs)-1} more)' if len(probs) > 1 else '')
                    else: print('ok  ', tag, '| landed at step', landed)
        print(f'{n-bad}/{n} runs clean'); sys.exit(1 if bad else 0)
    frames, landed, probs = run(a.html, a.level, a.pull[0], a.pull[1], 1/a.dt, a.after, a.no_fx, True)
    print('landed at', landed, '| problems:', probs[:5] or 'none')
    for s, r, im in frames: print(s, r['mode'], round(r['x']), round(r['y']), r['j'] and [round(v, 2) for v in r['j']])
    sheet(frames, a.out)

if __name__ == '__main__': main()
