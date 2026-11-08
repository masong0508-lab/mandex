#!/usr/bin/env python3
"""
Builds the console title pictures from the HTML prototype's own drawing code (drawTitlePicture in web/index.html):
the man dropping butt-first on a landlord.

  1. loads the HTML in headless Chromium (needs Playwright; only this tool needs it)
  2. draws the picture 4x too big, box-filters it down, quantises it to a small palette (no dithering: flat cartoon colours)
  3. writes art/title.h with
       DS : 341x192 (the WHOLE title picture), run-length coded, <= 192 colours.  Shown on the top screen, which pans across it;
            palette slots 64..255 of the sub engine (the text colours below slot 64 stay as they are).  The logo is drawn by the game.
       DC : 426x240 drawn at 2x as flat PVR quads (the Dreamcast build has no font and no textures yet); the screen pans across it.
            The MANDEX logo is baked in.  A 640x480 "PRESS START" strip is a second, tiny set of quads that the game blinks.
     and a preview PNG.

Usage:  python3 tools/gen_title.py [path/to/mandex.html]   (default: web/index.html)
"""
import base64, io, os, sys
import numpy as np
from PIL import Image
from playwright.sync_api import sync_playwright

HERE = os.path.dirname(os.path.abspath(__file__))
HTML = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "..", "web", "index.html")
OUT_H = os.path.join(HERE, "..", "art", "title.h")
OUT_PNG = os.path.join(HERE, "..", "docs", "title_art.png")
SS = 4
T = 1.1                      # the moment of the animation that is frozen into the picture
DS_W, DS_H, DS_COLORS, DS_BASE = 341, 192, 190, 64      # the whole 16:9 picture; the 256-wide screen pans across it
DC_W, DC_H, DC_COLORS = 426, 240, 104


def render(page, w, h, words):
    url = page.evaluate("""([w,h,t,words])=>{cv.width=w;cv.height=h;drawTitlePicture(w,h,t,words);return cv.toDataURL('image/png')}""",
                        [w * SS, h * SS, T, words])
    im = Image.open(io.BytesIO(base64.b64decode(url.split(",")[1]))).convert("RGB")
    return im.resize((w, h), Image.BOX)


KEEP = [(255, 126, 176), (255, 79, 143), (244, 183, 142), (217, 143, 100), (224, 36, 58), (255, 210, 63), (122, 26, 26), (255, 255, 255)]   # towel, skin, boxers, logo: never merged away


def quant(im, n):
    q = im.quantize(colors=n - len(KEEP), method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = [tuple(c) for c in np.array(q.getpalette()[: 3 * (n - len(KEEP))], dtype=np.uint8).reshape(-1, 3)] + KEEP
    pimg = Image.new("P", (1, 1))
    flat = [v for c in pal for v in c]
    pimg.putpalette(flat + list(pal[0]) * (256 - len(pal)))          # unused slots repeat colour 0, so nothing maps to a stray black
    idx = np.array(im.quantize(palette=pimg, dither=Image.Dither.NONE), dtype=np.uint8)
    return np.where(idx >= len(pal), 0, idx).astype(np.uint8), np.array(pal, dtype=np.uint8)


def rle(idx):
    flat = idx.reshape(-1)
    out, i = [], 0
    while i < len(flat):
        j = i
        while j < len(flat) and flat[j] == flat[i] and j - i < 255:
            j += 1
        out += [j - i, int(flat[i])]
        i = j
    return out


def rects(idx, skip=None):
    """Flat quads: every horizontal run of one colour, merged with the identical run on the row below."""
    h, w = idx.shape
    done, open_ = [], {}
    for y in range(h):
        runs, x = [], 0
        while x < w:
            x1 = x
            while x1 < w and idx[y, x1] == idx[y, x]:
                x1 += 1
            if skip is None or idx[y, x] != skip:
                runs.append((x, x1, int(idx[y, x])))
            x = x1
        nxt = {}
        for (a, b, c) in runs:
            r = open_.pop((a, b, c), None)
            if r:
                r[3] += 1
                nxt[(a, b, c)] = r
            else:
                nxt[(a, b, c)] = [a, y, b - a, 1, c]
        done += list(open_.values())
        open_ = nxt
    done += list(open_.values())
    return done


def carr(name, ctype, vals, per=24):
    s = "static const %s %s[%d] = {\n" % (ctype, name, len(vals))
    for i in range(0, len(vals), per):
        s += "  " + ",".join(str(v) for v in vals[i:i + per]) + ",\n"
    return s + "};\n"


def carr2(name, ctype, rows, per=5):
    s = "static const %s %s[%d][5] = {\n" % (ctype, name, len(rows))
    for i in range(0, len(rows), per):
        s += "  " + ",".join("{%s}" % ",".join(str(int(v)) for v in r) for r in rows[i:i + per]) + ",\n"
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
        ds = render(pg, DS_W, DS_H, False)
        dc = render(pg, DC_W, DC_H, True)
        # PRESS START at the Dreamcast's own 640x480 resolution, hard edged: 0 = clear, 1 = black outline, 2 = white
        url = pg.evaluate("""()=>{cv.width=360;cv.height=44;ctx.setTransform(1,0,0,1,0,0);ctx.clearRect(0,0,360,44);txt('PRESS START',180,34,32,'#ffffff','center','#000000');return cv.toDataURL('image/png')}""")
        st = Image.open(io.BytesIO(base64.b64decode(url.split(",")[1]))).convert("RGBA")
        b.close()
        if errs:
            sys.exit("page errors: %s" % errs)

    ds_idx, ds_pal = quant(ds, DS_COLORS)
    dc_idx, dc_pal = quant(dc, DC_COLORS)
    a = np.array(st)
    st_idx = np.zeros(a.shape[:2], dtype=np.uint8)
    opaque = a[:, :, 3] > 110
    st_idx[opaque] = np.where(a[:, :, :3].sum(axis=2)[opaque] > 600, 2, 1)
    ys, xs = np.nonzero(st_idx)
    st_idx = st_idx[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    st_x, st_y = (640 - st_idx.shape[1]) // 2, 400

    ds_r = rle(ds_idx)
    dc_r = rects(dc_idx)
    st_r = rects(st_idx, skip=0)

    h = "/* GENERATED by tools/gen_title.py from the HTML title (web/index.html, drawTitlePicture).  Do not edit by hand.\n"
    h += "   Define MX_TITLE_DS and/or MX_TITLE_DC before including to get the data for that console. */\n"
    h += "#ifdef MX_TITLE_DS\n"
    h += "#define TITLE_DS_W %d\n#define TITLE_DS_H %d\n#define TITLE_DS_NCOL %d\n#define TITLE_DS_BASE %d   /* first sub-engine palette slot the picture uses */\n" % (DS_W, DS_H, len(ds_pal), DS_BASE)
    h += carr("TITLE_DS_PAL", "unsigned char", [int(v) for row in ds_pal for v in row], 18)
    h += "/* pairs: run length 1..255, palette index.  Unpacks to exactly TITLE_DS_W * TITLE_DS_H bytes. */\n"
    h += carr("TITLE_DS_RLE", "unsigned char", ds_r, 24)
    h += "#define TITLE_DS_RLE_LEN %d\n#endif\n" % len(ds_r)
    h += "#ifdef MX_TITLE_DC\n"
    h += "#define TITLE_DC_W %d\n#define TITLE_DC_H %d   /* drawn at 2x */\n#define TITLE_DC_NCOL %d\n" % (DC_W, DC_H, len(dc_pal))
    h += carr("TITLE_DC_PAL", "unsigned char", [int(v) for row in dc_pal for v in row], 18)
    h += "/* x, y, w, h, palette index */\n#define TITLE_DC_NRECT %d\n" % len(dc_r)
    h += carr2("TITLE_DC_RECT", "unsigned short", dc_r)
    h += "#define TITLE_START_X %d\n#define TITLE_START_Y %d   /* 640x480 pixels, drawn 1:1; colour 1 = black, 2 = white */\n#define TITLE_START_NRECT %d\n" % (st_x, st_y, len(st_r))
    h += carr2("TITLE_START_RECT", "unsigned short", st_r)
    h += "#endif\n"
    open(OUT_H, "w").write(h)

    prev = Image.new("RGB", (max(DS_W, DC_W) * 2 + 16, DS_H * 2 + DC_H * 2 + 24), (30, 30, 40))
    prev.paste(Image.fromarray(ds_pal[ds_idx]).resize((DS_W * 2, DS_H * 2), Image.NEAREST), (8, 0))
    prev.paste(Image.fromarray(dc_pal[dc_idx]).resize((DC_W * 2, DC_H * 2), Image.NEAREST), (8, DS_H * 2 + 16))
    prev.save(OUT_PNG)
    print("DS: %d colours, %d RLE bytes (%d%% of raw)" % (len(ds_pal), len(ds_r), 100 * len(ds_r) // (DS_W * DS_H)))
    print("DC: %d colours, %d rects (+%d for PRESS START)" % (len(dc_pal), len(dc_r), len(st_r)))
    print("wrote", os.path.relpath(OUT_H), "and", os.path.relpath(OUT_PNG))


main()
