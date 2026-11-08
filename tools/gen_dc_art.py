#!/usr/bin/env python3
"""
Dreamcast art, baked from the HTML prototype's own drawing code (needs Playwright; only this tool does).

The Dreamcast build used to draw everything as flat coloured quads.  The PVR can do real textures, so this builds them:

  BGA   512x256  RGB565    the sky (gradient + sun), stretched over the screen
  BGB  1024x512  ARGB1555  mountains, city, hills, trees, ground and the sling post (640x480 screen pixels, 1:1)
  ATL   512x512  ARGB4444  building-block tiles (every material x 4 variants x 3 damage levels), landlord, cloud, soft puff, stars, head icon, font
  MAN  1024x512  ARGB4444  every man frame (the same frames as art/manspr.h), drawn smooth at 1.6x the screen size, no towel (the cloth is simulated)
  TTL  1024x512  RGB565    the title picture, 852x480 (the screen pans across it)

All of it goes into art/dcart.h as run-length coded 16-bit pixels (the game unpacks them into video memory at start-up), plus the tables
that say where everything is.  Texture memory is 16 bit; transparent pixels carry the colour of their neighbours so bilinear filtering
leaves no dark fringes.

Usage:  python3 tools/gen_dc_art.py [path/to/mandex.html]      (default: web/index.html)
"""
import base64, io, os, sys
import numpy as np
from PIL import Image
from playwright.sync_api import sync_playwright

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_manspr as G                       # the man frame list, blob table and the HTML path (same argv handling)

HTML = G.HTML
OUT_H = os.path.join(HERE, "..", "art", "dcart.h")
OUT_PNG = os.path.join(HERE, "..", "docs", "dc_art_sheet.png")
K = 1.6                                     # man sprite pixels per screen pixel-of-the-old-art
SS = 2                                      # supersample for the man
CX, CY = 480, 300
WSC = 0.625                                 # screen pixels per HTML pixel in the world (5 px cells / 8 px cells)
FONT_PX = 32
GLYPHS = list(range(32, 96))                # ' ' .. '_' (upper case only)


def grab(page, js, args=None):
    url = page.evaluate(js, args) if args is not None else page.evaluate(js)
    return Image.open(io.BytesIO(base64.b64decode(url.split(",")[1]))).convert("RGBA")


def bleed(a):
    """a: float RGBA array.  Give fully transparent pixels the colour of the nearest opaque neighbours (a few passes), alpha stays 0."""
    rgb = a[..., :3].copy(); al = a[..., 3]
    known = al > 0.5
    for _ in range(3):
        acc = np.zeros_like(rgb); cnt = np.zeros(al.shape, np.float32)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dx == 0 and dy == 0: continue
                k = np.roll(np.roll(known, dy, 0), dx, 1)
                v = np.roll(np.roll(rgb, dy, 0), dx, 1)
                acc += v * k[..., None]; cnt += k
        fill = (~known) & (cnt > 0)
        rgb[fill] = acc[fill] / cnt[fill][:, None]
        known = known | fill
    out = a.copy(); out[..., :3] = rgb
    return out


def to4444(im):
    a = bleed(np.asarray(im).astype(np.float32))
    r, g, b, al = [np.clip(np.rint(a[..., i] / 17.0), 0, 15).astype(np.uint16) for i in range(4)]
    return (al << 12) | (r << 8) | (g << 4) | b


def to1555(im):
    a = bleed(np.asarray(im).astype(np.float32))
    r, g, b = [np.clip(np.rint(a[..., i] / 255.0 * 31), 0, 31).astype(np.uint16) for i in range(3)]
    al = (a[..., 3] >= 128).astype(np.uint16)
    return (al << 15) | (r << 10) | (g << 5) | b


def to565(im):
    a = np.asarray(im.convert("RGB")).astype(np.float32)
    r = np.clip(np.rint(a[..., 0] / 255.0 * 31), 0, 31).astype(np.uint16)
    g = np.clip(np.rint(a[..., 1] / 255.0 * 63), 0, 63).astype(np.uint16)
    b = np.clip(np.rint(a[..., 2] / 255.0 * 31), 0, 31).astype(np.uint16)
    return (r << 11) | (g << 5) | b


def pad_to(arr, w, h):
    out = np.zeros((h, w), np.uint16); out[:arr.shape[0], :arr.shape[1]] = arr
    return out


def rle16(arr):
    flat = arr.reshape(-1)
    out, i, n = [], 0, len(flat)
    while i < n:
        j = i
        while j < n and flat[j] == flat[i] and j - i < 65535:
            j += 1
        out += [j - i, int(flat[i])]
        i = j
    return out


class Shelf:
    """tiny shelf packer: put(image) -> (x, y), 1 px gutter"""
    def __init__(self, w, h):
        self.w, self.h, self.x, self.y, self.rowh = w, h, 1, 1, 0
        self.img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    def put(self, im):
        w, h = im.size
        if self.x + w + 1 > self.w:
            self.x, self.y, self.rowh = 1, self.y + self.rowh + 1, 0
        if self.y + h + 1 > self.h:
            sys.exit("atlas full")
        self.img.paste(im, (self.x, self.y))
        pos = (self.x, self.y)
        self.x += w + 1; self.rowh = max(self.rowh, h)
        return pos


def tight(im):
    a = np.asarray(im)[..., 3]
    ys, xs = np.nonzero(a > 6)
    return im.crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)), (xs.min(), ys.min())


def carr(name, vals, per=16):
    s = "static const unsigned short %s[%d] = {\n" % (name, len(vals))
    for i in range(0, len(vals), per):
        s += "  " + ",".join(str(v) for v in vals[i:i + per]) + ",\n"
    return s + "};\n"


def main():
    with sync_playwright() as p:
        b = p.chromium.launch()
        pg = b.new_page(viewport={"width": 960, "height": 540})
        pg.add_init_script("window.requestAnimationFrame = () => 0;")
        errs = []
        pg.on("pageerror", lambda e: errs.append(str(e)))
        pg.goto("file://" + HTML)
        pg.wait_for_timeout(300)
        meta = pg.evaluate("MAN_FRAMES")
        pg.evaluate("NOTOWEL = true")

        # ---------------- backdrop.  The world sits at (20,150) with 5 px cells (0.625 px per HTML px); the scenery is a little bigger (0.9) and
        # anchored so the ground line meets the bottom of the world (y = 450).
        BS, TX = 0.9, -180
        TY = 450 - 480 * BS
        setup = f"cv.width=640;cv.height=480;ctx.setTransform(1,0,0,1,0,0);ctx.clearRect(0,0,640,480);ctx.imageSmoothingEnabled=true;ctx.imageSmoothingQuality='high';"
        bga = grab(pg, "() => {" + setup + f"ctx.fillStyle='#3d8fe8';ctx.fillRect(0,0,640,480);ctx.setTransform({BS},0,0,{BS},{TX},{TY});ctx.drawImage(bgA,0,0);ctx.setTransform(1,0,0,1,0,0);return cv.toDataURL('image/png')}}")
        bga = bga.resize((512, 256), Image.LANCZOS)
        bgb = grab(pg, "() => {" + setup + f"ctx.setTransform({BS},0,0,{BS},{TX},{TY});ctx.drawImage(bgB,0,0);ctx.setTransform({WSC},0,0,{WSC},20,150);drawSling(false);ctx.setTransform(1,0,0,1,0,0);return cv.toDataURL('image/png')}}")

        # ---------------- the title picture
        TT = 1.1
        ttl = grab(pg, "([w,h,t]) => {cv.width=w;cv.height=h;ctx.setTransform(1,0,0,1,0,0);drawTitlePicture(w,h,t,true);return cv.toDataURL('image/png')}", [852 * 2, 480 * 2, TT]).convert("RGB").resize((852, 480), Image.BOX)

        # ---------------- the atlas
        atl = Shelf(512, 512)
        tiles = grab(pg, """() => { const n = 8 * 4 * 3, c = 16, S = 18; cv.width = 18 * c; cv.height = 18 * Math.ceil(n / c); ctx.setTransform(1,0,0,1,0,0); ctx.clearRect(0,0,cv.width,cv.height);
          ctx.imageSmoothingEnabled = false;
          for (let m = 1; m <= 8; m++) for (let v = 0; v < 4; v++) for (let d = 0; d < 3; d++) { const t = ((m - 1) * 4 + v) * 3 + d, x = (t % c) * S, y = Math.floor(t / c) * S;
            ctx.save(); ctx.beginPath(); ctx.rect(x, y, S, S); ctx.clip();
            ctx.drawImage(SPR[m][v][d], x + 1, y + 1, 16, 16);                  // 2x nearest
            ctx.drawImage(SPR[m][v][d], 0, 0, 1, 8, x, y + 1, 1, 16); ctx.drawImage(SPR[m][v][d], 7, 0, 1, 8, x + 17, y + 1, 1, 16);   // clamp the edges so filtering never pulls in a neighbour
            ctx.drawImage(SPR[m][v][d], 0, 0, 8, 1, x + 1, y, 16, 1); ctx.drawImage(SPR[m][v][d], 0, 7, 8, 1, x + 1, y + 17, 16, 1);
            ctx.restore(); }
          return cv.toDataURL('image/png'); }""")
        t_pos = atl.put(tiles)
        # landlord: 1.25 px per HTML px (drawn at half size = 0.625 like the world)
        LS = 1.25
        ll = grab(pg, f"() => {{cv.width=90;cv.height=110;ctx.setTransform(1,0,0,1,0,0);ctx.clearRect(0,0,90,110);ctx.translate(45,70);ctx.scale({LS},{LS});drawLandlord({{t:0,x:0,y:0}});return cv.toDataURL('image/png')}}")
        ll_t, (llx, lly) = tight(ll)
        ll_pos = atl.put(ll_t); ll_anchor = (45 - llx, 70 - lly)
        cloud = grab(pg, "() => {cv.width=170;cv.height=100;ctx.setTransform(1,0,0,1,0,0);ctx.clearRect(0,0,170,100);ctx.globalAlpha=1;drawCloud({x:80,y:50,s:1,v:0});return cv.toDataURL('image/png')}")
        cl_t, (clx, cly) = tight(cloud)
        cl_pos = atl.put(cl_t)
        puff = grab(pg, """() => {cv.width=48;cv.height=48;ctx.setTransform(1,0,0,1,0,0);ctx.clearRect(0,0,48,48);const g=ctx.createRadialGradient(24,24,1,24,24,23);g.addColorStop(0,'rgba(255,255,255,1)');g.addColorStop(.55,'rgba(255,255,255,.65)');g.addColorStop(1,'rgba(255,255,255,0)');ctx.fillStyle=g;ctx.fillRect(0,0,48,48);return cv.toDataURL('image/png')}""")
        pf_pos = atl.put(puff)
        stars = grab(pg, "() => {cv.width=140;cv.height=70;ctx.setTransform(1,0,0,1,0,0);ctx.clearRect(0,0,140,70);star(35,37,28,true);star(105,37,28,false);return cv.toDataURL('image/png')}")
        st_pos = atl.put(stars)
        icon = grab(pg, "() => {cv.width=24;cv.height=26;ctx.setTransform(1,0,0,1,0,0);ctx.clearRect(0,0,24,26);const x=12;ell(x,15,6,6.5,SK,1.2);ctx.fillStyle=HAIR;ctx.fillRect(x-4,18,8,2.2);ctx.fillStyle=HAIR;ctx.beginPath();ctx.arc(x,11,6,3.3,6.1);ctx.fill();return cv.toDataURL('image/png')}")
        ic_pos = atl.put(icon)
        # font: every glyph in a 44 x 48 cell, white with a black outline (tinted by the vertex colour at run time)
        adv = []
        cells = []
        for g in GLYPHS:
            ch = chr(g)
            im = grab(pg, "([ch,sz]) => {cv.width=44;cv.height=48;ctx.setTransform(1,0,0,1,0,0);ctx.clearRect(0,0,44,48);txt(ch,22,36,sz,'#ffffff','center','#000000');return cv.toDataURL('image/png')}", [ch, FONT_PX])
            a = pg.evaluate("([ch,sz]) => {ctx.font='bold '+sz+'px \"Trebuchet MS\",Impact,sans-serif';return ctx.measureText(ch).width}", [ch, FONT_PX])
            adv.append(int(round(a + 2)))
            cells.append(atl.put(im))
        # ---------------- the man (HD)
        man = Shelf(1024, 512)
        frames = []
        has_towel = True
        legacy = {n: (c, pp) for n, c, pp in G.POSES}
        names = [pp[0] for pp in G.POSES] + G.NEW_FRAMES
        for name in names:
            if name in meta:
                call, ppu, head = f"drawManFrame('{name}')", meta[name]["ppu"], meta[name].get("head")
            elif name in legacy:
                (call, ppu), head = legacy[name], None
            else:
                (call, ppu), head = legacy[G.ALIAS[name]], None
            sc = SS * ppu * K
            js = f"""() => {{ ctx.setTransform(1,0,0,1,0,0); cv.width=960; cv.height=540; ctx.clearRect(0,0,960,540);
              ctx.save(); ctx.translate({CX},{CY}); ctx.scale({sc},{sc}); {call}; ctx.restore(); return cv.toDataURL('image/png'); }}"""
            a = np.asarray(grab(pg, js)).astype(np.float32)
            al = a[..., 3]; ys, xs = np.where(al > 8)
            x0 = CX - SS * int(np.ceil((CX - xs.min()) / SS)); x1 = CX + SS * int(np.ceil((xs.max() + 1 - CX) / SS))
            y0 = CY - SS * int(np.ceil((CY - ys.min()) / SS)); y1 = CY + SS * int(np.ceil((ys.max() + 1 - CY) / SS))
            c = a[y0:y1, x0:x1]; h, w = c.shape[0] // SS, c.shape[1] // SS
            pm = (c[..., :3] * (c[..., 3:4] / 255.0)).reshape(h, SS, w, SS, 3).mean(axis=(1, 3))
            alpha = (c[..., 3] / 255.0).reshape(h, SS, w, SS).mean(axis=(1, 3))
            rgb = np.where(alpha[..., None] > 0.004, pm / np.maximum(alpha[..., None], 1e-4), 0)
            out = np.dstack([np.clip(rgb, 0, 255), alpha * 255]).astype(np.uint8)
            pad = np.zeros((h + 2, w + 2, 4), np.uint8); pad[1:-1, 1:-1] = out
            im = Image.fromarray(pad, "RGBA")
            pos = man.put(im)
            ax, ay = (CX - x0) // SS + 1, (CY - y0) // SS + 1
            (bx, by, br), (rx, ry, rr) = G.BLOBS[G.family(name)]
            f = [pos[0], pos[1], w + 2, h + 2, ax, ay] + [int(round(v * ppu * K)) for v in (bx + 0, by + 0, br)] + [int(round(v * ppu * K)) for v in (rx, ry, rr)]
            # blob centres are measured from the anchor; the table stores them relative to the sprite's top left corner
            f[6] = ax + f[6]; f[7] = ay + f[7]; f[9] = ax + f[9]; f[10] = ay + f[10]
            f.append(int(round(ppu * K * 1000)))              # baked pixels per design unit * 1000
            frames.append((name, f))
        b.close()
        if errs: sys.exit("page errors: %s" % errs)

    # ---------------- write
    bga_a = pad_to(to565(bga), 512, 256)
    bgb_a = pad_to(to1555(bgb), 1024, 512)
    atl_a = to4444(atl.img)
    man_a = to4444(man.img)
    ttl_a = pad_to(to565(ttl), 1024, 512)
    H = ["/* GENERATED by tools/gen_dc_art.py from the HTML prototype's own drawing.  Do not edit by hand.",
         "   Textures are 16 bit pixels, run length coded as pairs (count, value); the game unpacks them into video memory.",
         "   Include after manspr.h (frame names) and define MX_DCART before including to get the data. */",
         "#ifndef MX_DCART_H", "#define MX_DCART_H", "#ifdef MX_DCART"]
    for nm, arr, w, h in (("BGA", bga_a, 512, 256), ("BGB", bgb_a, 1024, 512), ("ATL", atl_a, 512, 512), ("MAN", man_a, 1024, 512), ("TTL", ttl_a, 1024, 512)):
        r = rle16(arr)
        H.append("#define DCT_%s_W %d\n#define DCT_%s_H %d\n#define DCT_%s_RLE_LEN %d" % (nm, w, nm, h, nm, len(r)))
        H.append(carr("DCT_%s_RLE" % nm, r))
    H.append("/* world atlas (ATL): tiles are 16x16 inside 18x18 cells, index ((material-1)*4 + variant)*3 + damage, 16 per row, origin at TILE_X/Y */")
    H.append("#define DCA_TILE_X %d\n#define DCA_TILE_Y %d\n#define DCA_TILE_S 18" % (t_pos[0], t_pos[1]))
    H.append("/* x, y, w, h, anchor x, anchor y (the landlord's centre, 1.25 px per HTML pixel) */")
    H.append("static const short DCA_LANDLORD[6] = { %d, %d, %d, %d, %d, %d };" % (ll_pos[0], ll_pos[1], ll_t.size[0], ll_t.size[1], ll_anchor[0], ll_anchor[1]))
    H.append("static const short DCA_CLOUD[4] = { %d, %d, %d, %d };" % (cl_pos[0], cl_pos[1], cl_t.size[0], cl_t.size[1]))
    H.append("static const short DCA_PUFF[4] = { %d, %d, 48, 48 };" % pf_pos)
    H.append("static const short DCA_STARS[8] = { %d, %d, 70, 70, %d, %d, 70, 70 };" % (st_pos[0], st_pos[1], st_pos[0] + 70, st_pos[1]))
    H.append("static const short DCA_ICON[4] = { %d, %d, 24, 26 };" % ic_pos)
    H.append("/* font: ASCII 32..95, 44x48 cells, drawn at %d px; advance per glyph in px at that size */" % FONT_PX)
    H.append("#define DCA_FONT_SIZE %d\n#define DCA_FONT_FIRST 32\n#define DCA_FONT_N %d" % (FONT_PX, len(GLYPHS)))
    H.append("static const short DCA_FONT[%d][3] = {" % len(GLYPHS))
    H.append(",".join("{%d,%d,%d}" % (c[0], c[1], a) for c, a in zip(cells, adv)) + "};")
    H.append("/* man frames, in MSF_ order: x, y, w, h (in MAN), anchor x, y, belly blob cx, cy, r, rear blob cx, cy, r (sprite pixels), pixels per design unit * 1000 */")
    H.append("#define DCM_NFR %d" % len(frames))
    H.append("static const short DCM_FR[%d][13] = {" % len(frames))
    for nm, f in frames:
        H.append("  {%s}, /* %s */" % (",".join(str(v) for v in f), nm))
    H.append("};")
    H.append("#define DCM_SCALE %.4f   /* screen pixels per baked pixel: the old art drew 0.45 px per design unit at 0.36, i.e. 1.25 * ppu */" % (1.25 / K))
    H.append("#endif /* MX_DCART */\n#endif")
    open(OUT_H, "w").write("\n".join(H) + "\n")

    sheet = Image.new("RGB", (1024 + 512 + 8, 512 * 2 + 8), (40, 40, 60))
    for im, xy in ((man.img, (0, 0)), (atl.img, (1032, 0)), (ttl.resize((512, 288)), (1032, 520))):
        im = im.convert("RGBA"); sheet.paste(im, xy, im)
    sheet.paste(bgb.resize((512, 256)), (0, 520), bgb.resize((512, 256)))
    sheet.save(OUT_PNG)
    tot = sum(len(rle16(a)) for a in (bga_a, bgb_a, atl_a, man_a, ttl_a))
    print("frames:", len(frames), " rle values:", tot, " header KB:", os.path.getsize(OUT_H) // 1024)
    print("wrote", os.path.relpath(OUT_H), os.path.relpath(OUT_PNG))


main()
