#!/usr/bin/env python3
"""
Builds the console man sprites from the HTML prototype's own drawing code.

  1. loads mandex.html in headless Chromium (needs Playwright; only this tool needs it)
  2. draws each pose at 4x with the HTML's drawFront / drawSide / drawBall
  3. box-filters down to chunky pixel art, adds a 1 px outline, quantises to one shared palette
  4. writes art/manspr.h (used by the DS and the Dreamcast front-ends) and a preview PNG

Usage:  python3 tools/gen_manspr.py [path/to/mandex.html]   (default: web/index.html)
"""
import base64, io, sys, os
import numpy as np
from PIL import Image
from playwright.sync_api import sync_playwright

HTML = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "web", "index.html")
HERE = os.path.dirname(os.path.abspath(__file__))
OUT_H = os.path.join(HERE, "..", "art", "manspr.h")
OUT_PNG = os.path.join(HERE, "..", "docs", "man_sprites.png")
SS = 4                      # supersample
NCOL = 30                   # palette entries for the man (shared by every pose)
OUTLINE = (84, 44, 26)      # 1 px rim

# name, js call, px per HTML unit
POSES = [
    ("IDLE_A",  "drawFront('up','cool',0.3927)",  0.36),
    ("IDLE_B",  "drawFront('up','cool',1.1781)",  0.36),
    ("DRAG",    "drawFront('up','scream',0.3927)",  0.36),
    ("FLEX_A",  "drawFront('flex','cool',0.3927)",  0.36),
    ("FLEX_B",  "drawFront('flex','cool',1.1781)",  0.36),
    ("FLY_A",   "drawSide(0.0982,'scream')",  0.40),
    ("FLY_B",   "drawSide(0.2945,'scream')",  0.40),
    ("DIVE",    "drawBall(0.1,'scream')",  0.40),
    ("STUCK_A", "drawFront('star','dizzy',0.3927)", 0.27),
    ("STUCK_B", "drawFront('star','dizzy',1.1781)", 0.27),
]
CX, CY = 480, 300

# every frame the consoles know, in enum order.  Frames the HTML cannot draw yet fall back to an alias (see ALIAS).
SPIN_NAMES = ["SPIN_%d" % k for k in range(13)] + ["SPIN_R%d" % k for k in range(1, 12)] + ["SPIN_BL", "SPIN_BR"]
NEW_FRAMES = ["BELLY_A", "BELLY_B", "SCRATCH_A", "SCRATCH_B", "BURP_A", "BURP_B", "PICK_A", "PICK_B", "YAWN",
              "SLEEP_A", "SLEEP_B", "SAD", "BACK_C", "BACK_L", "BACK_R", "FART_A", "FART_B", "CAPE_A"] + SPIN_NAMES
def spin_th(name):
    """turn angle (degrees) and head lead of a spin frame, the same numbers as spinKey() in the HTML"""
    if name in ("SPIN_BL", "SPIN_BR"): return 180, 0
    if name.startswith("OOPS_"): return 180, OOPS_LD[name]
    th = int(name[6:] if name.startswith("SPIN_R") else name[5:]) * 15
    m = min(1.0, th / 20.0, (180 - th) / 20.0)
    return th, (-12 if name.startswith("SPIN_R") else 12) * m
# the rare "oops" gag (boxers slip off in the cape taunt): drawn by the HTML as part of the spin poses; the head's lead angle of each frame, as in OOPS_POSE
OOPS_NAMES = ["OOPS_" + c for c in "ABCDEFGH"]
OOPS_LD = dict(zip(OOPS_NAMES, (0, 0, 0, -112, -122, -118, -120, -104)))
NEW_FRAMES += OOPS_NAMES
def spiny(name): return name.startswith("SPIN_") or name.startswith("OOPS_")      # frames that turn around: shared head / body handling
ALIAS = {n: "IDLE_A" for n in NEW_FRAMES}
ALIAS.update({"CAPE_A": "FLEX_A", "BACK_C": "FLEX_A", "BACK_L": "FLEX_A", "BACK_R": "FLEX_B", "FART_A": "FLEX_A", "FART_B": "FLEX_B"})
ALIAS.update({n: ("IDLE_A" if spin_th(n)[0] < 90 else "FLEX_B") for n in SPIN_NAMES})
ALIAS.update({n: "FLEX_B" for n in OOPS_NAMES})
# jiggle blobs in design units (centre x, y, radius): belly and rear, per body family
BLOBS = {"front": ((0, -2, 21), (0, 10, 21)), "back": ((0, -6, 20), (0, 10, 21)),
         "fly": ((2, -2, 19), (-13, 6, 14)), "dive": ((3, -2, 17), (-13, 6, 14)), "star": ((0, -2, 21), (0, 12, 20))}
def family(name):
    if name.startswith("BACK") or name.startswith("FART") or (name.startswith("SPIN_") and spin_th(name)[0] >= 120) or name.startswith("OOPS_"): return "back"
    if name.startswith("FLY"): return "fly"
    if name == "DIVE": return "dive"
    if name.startswith("STUCK"): return "star"
    return "front"


def render(page, call, ppu):
    scale = SS * ppu
    js = f"""() => {{
      ctx.setTransform(1,0,0,1,0,0); ctx.clearRect(0,0,cv.width,cv.height);
      ctx.save(); ctx.translate({CX},{CY}); ctx.scale({scale},{scale});
      {call};
      ctx.restore(); return cv.toDataURL('image/png'); }}"""
    url = page.evaluate(js)
    img = Image.open(io.BytesIO(base64.b64decode(url.split(",")[1]))).convert("RGBA")
    return np.asarray(img).astype(np.float32)


def downsample(a):
    """crop to the figure, align to the SS grid around the origin, premultiplied box filter"""
    al = a[..., 3]
    ys, xs = np.where(al > 8)
    x0 = CX - SS * int(np.ceil((CX - xs.min()) / SS)); x1 = CX + SS * int(np.ceil((xs.max() + 1 - CX) / SS))
    y0 = CY - SS * int(np.ceil((CY - ys.min()) / SS)); y1 = CY + SS * int(np.ceil((ys.max() + 1 - CY) / SS))
    c = a[y0:y1, x0:x1]
    h, w = c.shape[0] // SS, c.shape[1] // SS
    pm = c[..., :3] * (c[..., 3:4] / 255.0)
    pm = pm.reshape(h, SS, w, SS, 3).mean(axis=(1, 3))
    alpha = (c[..., 3] / 255.0).reshape(h, SS, w, SS).mean(axis=(1, 3))
    rgb = np.where(alpha[..., None] > 0.01, pm / np.maximum(alpha[..., None], 1e-3), 0)
    return rgb, alpha, (CX - x0) // SS, (CY - y0) // SS


def outline(rgb, alpha, ax, ay):
    """pad by 1, threshold, then rim every empty pixel that touches the figure"""
    h, w = alpha.shape
    P = 2
    A = np.zeros((h + 2 * P, w + 2 * P), bool); A[P:-P, P:-P] = alpha >= 0.5
    C = np.zeros((h + 2 * P, w + 2 * P, 3), np.float32); C[P:-P, P:-P] = rgb
    rim = np.zeros_like(A)
    rim[1:] |= A[:-1]; rim[:-1] |= A[1:]; rim[:, 1:] |= A[:, :-1]; rim[:, :-1] |= A[:, 1:]
    rim &= ~A
    C[rim] = OUTLINE
    A |= rim
    # trim empty border
    ys, xs = np.where(A)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    return C[y0:y1, x0:x1], A[y0:y1, x0:x1], ax + P - x0, ay + P - y0


import sys as _s; _s.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import manhead

def put(idx, x, y, col):
    if 0 <= y < idx.shape[0] and 0 <= x < idx.shape[1]: idx[y, x] = col

def wipe_head(idx, hx, hy, ppu):
    """clear the generated head (skull + knot) with an ellipse, not a rectangle: a raised arm next to the head must keep its pixels"""
    H, W = idx.shape
    yy, xx = np.mgrid[0:H, 0:W]
    skull = ((xx - hx) / (17.0 * ppu + 1.2)) ** 2 + ((yy - hy) / (15.0 * ppu + 1.2)) ** 2 <= 1
    knot = ((xx - hx) / (6.5 * ppu + 1.6)) ** 2 + ((yy - (hy - 18.5 * ppu)) / (5.6 * ppu + 1.6)) ** 2 <= 1
    idx[skull | knot] = 0

HK = 0.82                   # his head is drawn this much smaller than the first hand-pixelled version (the user found it too big)
def stamp(idx, rows, left, top, hk, k=None):
    """paint a hand-pixelled head; it is shrunk by HK about the middle of its bottom edge so the chin stays on the shoulders"""
    k = HK if k is None else k
    h, w = len(rows), len(rows[0])
    if k != 1:
        nw, nh = max(4, int(round(w * k))), max(4, int(round(h * k)))
        rows = manhead.shrink(rows, nw, nh)
        left += (w - nw) // 2; top += h - nh
        w, h = nw, nh
    for j, r in enumerate(rows):
        for i, ch in enumerate(r):
            if ch != '.': put(idx, left + i, top + j, hk[ch])
    return left, top, w, h

def paint(idx, x0, y0, rows, key, PI):
    """rows: list of strings; '.' leaves the pixel alone, letters map through key"""
    for j, r in enumerate(rows):
        for i, ch in enumerate(r):
            if ch != '.': put(idx, x0 + i, y0 + j, PI[key[ch]])
# face blocks for the new front-view poses: (rows, dx, dy) relative to the cool-face block (12 wide, top at head centre - 1)
_MUS = ["hhSShhhhSShh", "hhhhhhhhhhhh"]
def _face(brow, eyes, skin, mouth, chin):
    return [brow, eyes, skin] + _MUS + [mouth, chin]
_HAPPY = ".sBBs..sBBs."
_OPEN  = ".wBw...wBw.."
_CROSS = ".wwB....Bww."
_SKIN  = "SSSSSsSSSSSS"
_CHIN  = "..SSoooSSS.."
FACES = {
    "BELLY_A":   (_face("..hhh..hhh..", _HAPPY, _SKIN, ".SwwwwwwwwS.", _CHIN), 0, 0),
    "BELLY_B":   (_face("..hhh..hhh..", _OPEN,  _SKIN, ".SmmmmmmmmS.", "..SmttmSSS.."), 0, 0),
    "SCRATCH_A": (_face("..hhh..hhh..", _HAPPY, _SKIN, "..SSwwwwSS..", _CHIN), 0, 0),
    "SCRATCH_B": (_face("..hhh..hhh..", ".SBBS..SBBS.", _SKIN, "..SmmmmmmS..", "..SSmmSSSS.."), 0, 0),
    "BURP_A":    (_face("..hhh..hhh..", ".sBBBS.SBBBs", "kSSSSsSSSSSk", ".kSmmmmmmSk.", _CHIN), 0, 0),
    "BURP_B":    (_face("..hhh..hhh..", _OPEN,  _SKIN, ".SmmmmmmmmS.", "..SmttttmS.."), 0, 0),
    "PICK_A":    (_face("..hhh..hhh..", _CROSS, _SKIN, "..SSmmmmSS..", _CHIN), 0, 0),
    "PICK_B":    (_face("..hhh..hhh..", ".SBBS..SBBS.", _SKIN, "..SSmmmmSS..", _CHIN), 0, 0),
    "YAWN":      (_face(".hhh....hhh..", ".sBBs..sBBs.", _SKIN, ".SmmmmmmmmS.", "..SmttttmS.."), 0, 0),
    "SLEEP_A":   (_face("..hhh..hhh..", _HAPPY, _SKIN, "..SSmmmmSS..", "..SSmmSSS.."), 0, 0),
    "SLEEP_B":   (_face("..hhh..hhh..", _HAPPY, _SKIN, "..SmmmmmmS..", "..SmmmmmS..."), 0, 0),
    "SAD":       (_face(".hhh....hhh..", _OPEN,  "SwSSSsSSSwSS", "..SSmmmmSS..", ".SmSSSSSSmS."), 0, 0),
}

def shade_butt(idx, PI, ay):
    """repaint the red briefs of the back views as two round cheeks: light from the top left, a dark cleft, a crease underneath"""
    red = (idx == PI['BR']) | (idx == PI['BRD']) | (idx == PI['BRH'])
    red[:ay + 1] = False                                           # briefs only (the flag is also red)
    ys, xs = np.nonzero(red)
    if len(ys) == 0: return
    y0, y1, x0, x1 = ys.min(), ys.max(), xs.min(), xs.max()
    xm = (x0 + x1) / 2.0; bw = x1 - x0 + 1; bh = y1 - y0 + 1
    for y, x in zip(ys, xs):
        k = -1 if x < xm else 1
        cx = xm + k * bw * 0.25; cy = y0 + bh * 0.52
        u = (x - cx) / (bw * 0.27); v = (y - cy) / (bh * 0.62)
        d = u * u + v * v
        col = PI['BR']
        if abs(x - xm) < 0.75 or (k < 0 and x > xm - 1.2) or (k > 0 and x < xm + 1.2): col = PI['BRD']     # the cleft
        elif d > 0.80 or y >= y1 - 0: col = PI['BRD']                                                       # underside / outer edge in shade
        elif u * k > 0 and d > 0.45: col = PI['BRD'] if v > 0.1 else PI['BR']
        if col == PI['BR'] and -0.75 < u * (-k) < -0.05 + 0.0 and -0.75 < v < -0.15: col = PI['BRH']       # highlight, upper left of each cheek
        if col == PI['BR'] and u * (-k) > -0.05 and u * (-k) < 0.0: col = PI['BR']
        idx[y, x] = col

def paint_face(name, idx, ax, ay, PI, ppu=0.36, head=None):
    """the generated heads are only ~10 px wide, so the face is pixelled by hand on top of the HTML shapes"""
    key = {'B': 'BLK', 'G': 'GOLD', 'g': 'GOLD2', 'w': 'WH', 'h': 'HAIR', 'H': 'HAIR2', 'o': 'RIM', 'm': 'MOUTH', 't': 'TONG', 's': 'SKD', 'k': 'SKM', 'S': 'SK'}
    hx, hy = (int(round(ax + head[0] * ppu)), int(round(ay + head[1] * ppu))) if head else (ax, ay - 16)   # head centre
    if spiny(name) and name not in ("SPIN_BL", "SPIN_BR"):   # the turn: the head has its own angle (th + lead); front, narrowing, profile, then the back
        th, ld = spin_th(name); a = abs(th + ld)
        if a < 125:
            hk = {k: PI[v] for k, v in manhead.KEY.items()}
            if a < 22: rows, ox = manhead.head("open", "grin"), 9
            elif a < 40: rows = manhead.scaled(manhead.head("open", "grin"), 17, 19); ox = 8
            elif a < 56: rows = manhead.scaled(manhead.head("open", "grin"), 14, 19); ox = 7
            else: rows = manhead.side_head("dive"); ox = 8
            top = 11 if a < 56 else 7
            wipe_head(idx, hx, hy, ppu)
            stamp(idx, rows, hx - ox, hy - top, hk)
            return
    if name.startswith("BACK") or name.startswith("FART") or spiny(name):          # the back of a proper head, replacing the muddy generated one
        if spiny(name) and head:
            # the spin frames have a raised hand / towel next to the head, so the top-row search below can lock onto the wrong thing:
            # take the head position straight from the model instead
            xc = hx; yt = int(round(ay + (head[1] - 24.0) * ppu))        # top of the top-knot
        else:
            win = idx[:, max(0, ax - 8):ax + 9]
            rows_ = np.nonzero(win.any(axis=1))[0]
            yt = int(rows_[0]); xs_ = np.nonzero(win[yt])[0] + max(0, ax - 8); xc = int(round(xs_.mean()))
        if spiny(name) and head: wipe_head(idx, hx, hy, ppu)
        else: idx[yt:yt + 11, max(0, xc - 8):xc + 9] = 0
        hk = {k: PI[v] for k, v in manhead.KEY.items()}
        bh = manhead.back_head()                                   # a round head is about as wide from every side, so one width for every turn angle
        l, t, w_, h_ = stamp(idx, bh, xc - len(bh[0]) // 2, yt - 1, hk)
        if name.startswith("FART"):                                # guilty glance over the shoulder
            for j, r in enumerate(manhead.PEEK):
                for i, ch in enumerate(r):
                    if ch != '.': put(idx, l + w_ - 3 + i, t + h_ // 2 - 1 + j, hk[ch])
        return
    if name in ("FLY_A", "FLY_B", "DIVE"):                         # side-view big head, facing right
        rows = manhead.side_head("fly" if name != "DIVE" else "dive")
        hk = {k: PI[v] for k, v in manhead.KEY.items()}
        idx[max(0, hy - 9):hy + 7, max(0, hx - 7):hx + 10] = 0
        stamp(idx, rows, hx - 8, hy - 7, hk)
        return
    if name in ("STUCK_A", "STUCK_B"):                             # small dizzy head
        hy = ay - 12
        rows = manhead.scaled(manhead.head("dizzy", "tongue"), 15, 15)
        hk = {k: PI[v] for k, v in manhead.KEY.items()}
        idx[max(0, hy - 9):hy + 7, max(0, hx - 8):hx + 9] = 0
        stamp(idx, rows, hx - 7, hy - 8, hk)
        return
    if name in manhead.FRAME_HEADS:                                # big hand-pixelled head replaces the tiny generated one
        eyes, mouth, extra = manhead.FRAME_HEADS[name]
        idx[max(0, hy - 12):hy + 5, max(0, hx - 7):hx + 8] = 0     # wipe the old head (arms are further out)
        hk = {k: PI[v] for k, v in manhead.KEY.items()}
        stamp(idx, manhead.head(eyes, mouth, extra), hx - 9, hy - 11, hk)
        if name in ("SLEEP_A", "SLEEP_B"):                         # snot bubble at the right nostril: small, then big
            bub = [".oo.", "owwo", "owwo", ".oo."] if name == "SLEEP_A" else ["..oo..", ".owwo.", "owwwwo", "owwwwo", ".owwo.", "..oo.."]
            paint(idx, hx + 2, hy + 2, bub, key, PI)
        return
    if name in ("IDLE_A", "IDLE_B", "FLEX_A", "FLEX_B"):          # cool: gold-rimmed shades, handlebar, smug grin
        rows = ["gBBBBGGBBBBg",
                ".BwBBGGBwBB.",
                "SSSSSsSSSSSS",
                "hhSShhhhSShh",
                "hhhhhhhhhhhh",
                ".SwwwwwwwwS.",
                "..SSoooSSS.."]
        # lens rows replace the eye band; mustache over the lip; grin below
        paint(idx, hx - 6, hy - 1, rows, key, PI)
    elif name == "DRAG":                                           # scream: wide eyes, brows, open mouth
        rows = ["hhhS.SShhh",
                ".wwS..Swwh",
                ".wBS..SBw.",
                "SSSSsSSSSS",
                "hhhhhhhhhh",
                "hhSmmmmShh",
                "..SmttmS..",
                "...SmmS..."]
        paint(idx, hx - 5, hy - 2, rows, key, PI)
    elif name in ("STUCK_A", "STUCK_B"):                           # dizzy: swirly eyes, tongue out (head is smaller here)
        hy = ay - 12
        rows = ["wwS..Sww",
                "wBS..SBw",
                "SSSsSSSS",
                "hhhhhhhh",
                "hSSmmSSh",
                "..SmtS.."]
        paint(idx, hx - 4, hy - 2, rows, key, PI)
        if name == "STUCK_B": put(idx, hx - 4, hy - 1, PI['BLK']); put(idx, hx + 3, hy - 1, PI['WH'])
    elif name in FACES:                                            # the new idle / gross-out poses: 12 px wide face block
        rows, dx, dy = FACES[name]
        paint(idx, hx - 6 + dx, hy - 1 + dy, rows, key, PI)
        if name in ("SLEEP_A", "SLEEP_B"):                         # snot bubble at the right nostril: small, then big
            bub = [".oo.", "owwo", "owwo", ".oo."] if name == "SLEEP_A" else ["..oo..", ".owwo.", "owwwwo", "owwwwo", ".owwo.", "..oo.."]
            paint(idx, hx + 3, hy + 2, bub, key, PI)
    elif name in ("FLY_A", "FLY_B"):                               # side view, facing right
        ex, ey = ax + 14, ay - 8 - (0 if name == "FLY_A" else 0)
        paint(idx, ex - 1, ey - 2, ["hhh", "wwB", "wwB"], key, PI)       # brow + wide eye
        paint(idx, ex - 1, ey + 1, ["SSS", "hhhh", "SmmS", "SmtS"], key, PI)  # mustache, mouth
    elif name == "DIVE":
        paint(idx, ax + 8, ay - 7, ["hhh", "wwB", "wwB"], key, PI)
        paint(idx, ax + 8, ay - 4, ["hhhh", "SmmS", "SmtS"], key, PI)


def main():
    hands = {}
    frames = []
    with sync_playwright() as p:
        b = p.chromium.launch()
        pg = b.new_page(viewport={"width": 960, "height": 540})
        pg.add_init_script("window.requestAnimationFrame = () => 0;")
        errs = []
        pg.on("pageerror", lambda e: errs.append(str(e)))
        pg.goto("file://" + HTML)
        pg.wait_for_timeout(300)
        has_api = pg.evaluate("typeof drawManFrame === 'function' && typeof MAN_FRAMES === 'object'")
        meta = pg.evaluate("MAN_FRAMES") if has_api else {}
        # the towel is a real cloth the consoles simulate at runtime, so the sprites are drawn WITHOUT it; the HTML says where the hand is
        has_towel = bool(has_api and pg.evaluate("typeof TOWEL_HAND === 'object' && typeof NOTOWEL !== 'undefined'"))
        if has_towel:
            pg.evaluate("NOTOWEL = true")
            hands = pg.evaluate("TOWEL_HAND")
        else:
            print("  (this HTML has no TOWEL_HAND / NOTOWEL: the old baked flag stays in the sprites and no cloth is drawn)")
        legacy = {n: (c, p) for n, c, p in POSES}
        for name in [p[0] for p in POSES] + NEW_FRAMES:
            src = name
            if has_api and name in meta:
                call, ppu, head = f"drawManFrame('{name}')", meta[name]["ppu"], meta[name].get("head")
                if spiny(name) and head and pg.evaluate("typeof spinSway === 'function'"):
                    # the body sways and crouches over the planted feet, so the head is not at the same place in every frame
                    head = pg.evaluate("(n) => { const q = SPIN_POSE[n], h = q.hip || 0, s = spinSway(q.th, q.psi || 0, h); return [s.shx - h * 0.35, -44 + s.bob]; }", name)
            elif name in legacy:
                (call, ppu), head = legacy[name], None
            else:
                src = ALIAS[name]; (call, ppu), head = legacy[src], None
                print("  (no HTML art yet for %s, aliasing %s)" % (name, src))
            rgb, alpha, ax, ay = downsample(render(pg, call, ppu))
            frames.append((name, src, ppu, head) + outline(rgb, alpha, ax, ay))
        b.close()
        if errs: print("page errors:", errs)

    # hand-picked palette (nearest-colour mapping keeps the skin shading clean instead of 30 near-duplicates)
    pal_list = [OUTLINE, (244, 183, 142), (205, 135, 100), (255, 215, 180), (226, 160, 118),    # rim, skin, skin shade, skin light, skin mid
                (224, 36, 58), (160, 20, 40), (242, 96, 108),                                     # briefs, shade, highlight
                (255, 255, 255), (196, 196, 210), (74, 45, 26), (36, 22, 14), (20, 20, 28),       # white, grey, hair, dark hair, black
                (255, 126, 176), (255, 79, 143), (205, 58, 118),                                  # towel, stripe, shade
                (255, 210, 63), (226, 181, 60), (138, 90, 43), (92, 54, 24),                       # gold, gold shade, shoe, shoe shade
                (92, 16, 16), (255, 107, 138)]                                                    # mouth, tongue
    NBASE = len(pal_list)                                         # colours the body is matched against
    pal_list += [(120, 76, 44), (236, 140, 125), (100, 60, 34), (176, 48, 64), (222, 222, 236)]   # extras used only by the hand-painted faces: hair shine, blush, iris, lip, tooth
    # the runtime towel's colours: front / back side x plain / stripe x (dark, mid, bright) = 12 more entries (only the cloth uses them)
    CL_COL = {}
    for (face, stripe), c in (((1, 0), (255, 126, 176)), ((0, 0), (234, 102, 156)), ((1, 1), (255, 84, 146)), ((0, 1), (208, 60, 122))):
        for sh, m in enumerate((0.70, 0.86, 1.0)):
            pal_list.append(tuple(int(v * m) for v in c)); CL_COL[(face, stripe, sh)] = len(pal_list)     # 1-based, like the pixels
    pal = np.array(pal_list[:NBASE], np.uint8)
    PI = {n: i + 1 for i, n in enumerate("RIM SK SKD SKH SKM BR BRD BRH WH GR HAIR HAIR2 BLK TOW TOW2 TOWD GOLD GOLD2 SHOE SHOED MOUTH TONG HAIRL BLUSH IRIS LIP TOOTH".split())}

    def index_of(rgb):
        d = ((pal[None, :, :].astype(np.int32) - rgb[:, None, :].astype(np.int32)) ** 2).sum(-1)
        return d.argmin(1) + 1                                    # 0 = transparent

    out = []
    for name, src, ppu, head, C, A, ax, ay in frames:
        h, w = A.shape
        idx = np.zeros((h, w), np.uint8)
        idx[A] = index_of(C[A].astype(np.int32))
        idx = np.pad(idx, ((6, 0), (5, 5))); ax += 5; ay += 6         # room for the bigger head, trimmed again below
        paint_face(src, idx, int(ax), int(ay), PI, ppu, head)
        if name.startswith("BACK") or name.startswith("FART") or name.startswith("SPIN_"): shade_butt(idx, PI, int(ay))   # (the oops frames keep the HTML's own bare / boxer shading)
        ys, xs = np.nonzero(idx); y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        idx = idx[y0:y1, x0:x1]; ax -= x0; ay -= y0; h, w = idx.shape
        (bx, by, br), (rx, ry, rr) = BLOBS[family(name)]
        blobs = [int(round(ax + bx * ppu)), int(round(ay + by * ppu)), int(round(br * ppu)),
                 int(round(ax + rx * ppu)), int(round(ay + ry * ppu)), int(round(rr * ppu))]
        out.append((name, w, h, int(ax), int(ay), idx, blobs))

    # ---- header
    L = ["/* GENERATED by tools/gen_manspr.py from the HTML prototype's own man drawing. Do not edit by hand.",
         "   Chunky pixel art at 4 pixels per cell; index 0 = transparent, index k = MS_PAL[k-1]. */",
         "/* #define MS_ENUM_ONLY before including to get just the frame names (MSF_*) without the pixel data. */",
         "#ifndef MX_MANSPR_H", "#define MX_MANSPR_H", f"#define MS_NPAL {len(pal_list)}"]
    L += ["enum {" + ", ".join(f"MSF_{o[0]}" for o in out) + ", MSF_COUNT };", "#ifndef MS_ENUM_ONLY",
          "static const unsigned char MS_PAL[MS_NPAL][3] = {"]
    L += ["  {%d,%d,%d}," % c for c in pal_list]
    L += ["};"]
    for name, w, h, ax, ay, idx, blobs in out:
        L.append(f"static const unsigned char MS_{name}[{w * h}] = {{")
        for y in range(h):
            L.append("  " + ",".join(str(int(v)) for v in idx[y]) + ",")
        L.append("};")
    L.append("typedef struct { const unsigned char *px; short w, h, ax, ay; short bcx, bcy, brad, rcx, rcy, rrad; } MsFrame;")
    L.append("/* ax,ay = where the man's centre sits; b* = belly jiggle blob, r* = rear jiggle blob (centre x,y and radius in sprite pixels) */")
    L.append("static const MsFrame MS_FRAMES[MSF_COUNT] = {")
    for name, w, h, ax, ay, idx, bl in out:
        L.append(f"  {{ MS_{name}, {w}, {h}, {ax}, {ay}, {bl[0]}, {bl[1]}, {bl[2]}, {bl[3]}, {bl[4]}, {bl[5]} }},")
    L += ["};"]
    L.append("/* the towel the front-ends simulate (core/cloth.c): palette values (1-based, like the pixels) by [face 1=front][stripe][shade 0 dark..2 bright], and its outline */")
    L.append("static const unsigned char MS_CL_COL[2][2][3] = {")
    for face in (0, 1):
        L.append("  { " + ", ".join("{ %d, %d, %d }" % tuple(CL_COL[(face, st, sh)] for sh in (0, 1, 2)) for st in (0, 1)) + " },")
    L.append("};")
    L.append(f"#define MS_CL_OUT {PI['HAIR2']}")
    L.append("/* where the raised hand is in each frame, in quarter design units {x, y, z} (centre of the man = 0,0,0; z toward the viewer); 32767 = no towel in this frame */")
    L.append("static const short MS_HAND[MSF_COUNT][3] = {")
    for name, w, h, ax, ay, idx, bl in out:
        hd = hands.get(name)
        L.append("  { %d, %d, %d }," % tuple(int(round(v * 4)) for v in hd) if hd else "  { 32767, 0, 0 },")
    L += ["};", "#endif /* MS_ENUM_ONLY */", "#endif", ""]
    os.makedirs(os.path.dirname(OUT_H), exist_ok=True)
    open(OUT_H, "w").write("\n".join(L))

    # ---- preview sheet: every frame at 4x on a sky colour, 6 per row
    sky = (120, 175, 235)
    cell = 4
    per = 6
    rows = [out[i:i + per] for i in range(0, len(out), per)]
    sheet_w = max(sum(o[1] * cell + 14 for o in r) for r in rows) + 14
    sheet_h = sum(max(o[2] for o in r) * cell + 16 for r in rows) + 8
    sheet = Image.new("RGB", (sheet_w, sheet_h), sky)
    y = 8
    for r in rows:
        x = 14
        for name, w, h, ax, ay, idx, bl in r:
            im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            px = im.load()
            for yy in range(h):
                for xx in range(w):
                    k = idx[yy, xx]
                    if k: px[xx, yy] = pal_list[k - 1] + (255,)
            big = im.resize((w * cell, h * cell), Image.NEAREST)
            sheet.paste(big, (x, y), big)
            x += w * cell + 14
        y += max(o[2] for o in r) * cell + 16
    os.makedirs(os.path.dirname(OUT_PNG), exist_ok=True)
    sheet.save(OUT_PNG)
    for name, w, h, ax, ay, idx, bl in out:
        print(f"{name:8s} {w}x{h}  anchor ({ax},{ay})")
    print("palette entries:", len(pal_list), "->", os.path.relpath(OUT_H))


if __name__ == "__main__":
    main()
