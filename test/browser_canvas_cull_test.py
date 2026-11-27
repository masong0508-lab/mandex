#!/usr/bin/env python3
"""Compare production Canvas rendering with wallpaper/rubble culling disabled.

Optional browser gate (stdlib + Playwright only):
    python3 test/browser_canvas_cull_test.py --out-dir /tmp/mandex-canvas-cull

Use --chromium PATH or CHROMIUM_EXECUTABLE for a system browser; otherwise
Playwright's installed Chromium is used. No golden screenshots, network, game
source edits, or timing/FPS thresholds are involved. Every RGBA byte of both
complete draw() results is compared in the browser, including antialiasing.
"""

import argparse
import base64
import json
import os
from pathlib import Path
import sys


INIT = r"""
(() => {
  // Install BEFORE the game captures Math.random as its `rnd` alias.
  let seed = 0x1435beef, first = [], calls = 0;
  window.__cullRng = {
    reset(value, prefix = []) { seed = value >>> 0; first = prefix.slice(); calls = 0; },
    calls() { return calls; }
  };
  Math.random = () => {
    calls++;
    if (first.length) return first.shift();
    seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
    return seed / 4294967296;
  };
  window.requestAnimationFrame = () => 0;
  window.cancelAnimationFrame = () => {};
  window.setInterval = () => 0;
  // Audio timers cannot affect a deterministic, silent rendering fixture.
  window.setTimeout = () => 0;
  try { localStorage.clear(); } catch (_) {}
})();
"""


HARNESS = r"""
(() => {
  const source = draw.toString();
  const patches = [
    ['const paperCull=camOn,', 'const paperCull=false,'],
    ['const tileCull=camOn,', 'const tileCull=false,'],
    ['const rubbleCull=camOn,', 'const rubbleCull=false,'],
    ['const actorCull=camOn,', 'const actorCull=false,']
  ];
  let referenceSource = source;
  for (const [from, to] of patches) {
    if (source.split(from).length !== 2)
      throw Error('Expected exactly one production culling switch: ' + from);
    referenceSource = referenceSource.replace(from, to);
  }
  // Global lexical game variables remain visible to the reference function.
  // All other rendering is intact; reference also draws all standing tiles.
  const reference = new Function('return (' + referenceSource + ')')();
  const nativeDrawImage = ctx.drawImage, nativeFillRect = ctx.fillRect;
  let counts = null;
  ctx.drawImage = function(...args) {
    if (counts) counts.drawImage++;
    return nativeDrawImage.apply(this, args);
  };
  ctx.fillRect = function(...args) {
    if (counts) counts.fillRect++;
    return nativeFillRect.apply(this, args);
  };

  function clothCopy(tw) {
    // structuredClone preserves typed arrays, but not Cloth.prototype.step.
    return { ...tw, c: tw.c
      ? Object.assign(Object.create(Cloth.prototype), structuredClone(tw.c))
      : null };
  }
  function drawState() {
    return { spin: clothCopy(TW_SPIN), back: clothCopy(TW_BACK),
             toasts: structuredClone(toasts) };
  }
  function restoreDrawState(saved) {
    Object.assign(TW_SPIN, clothCopy(saved.spin));
    Object.assign(TW_BACK, clothCopy(saved.back));
    toasts.splice(0, toasts.length, ...structuredClone(saved.toasts));
    frameDt = 0;
  }
  function png(pixels) {
    const c = document.createElement('canvas'); c.width = W; c.height = H;
    c.getContext('2d').putImageData(pixels, 0, 0);
    return c.toDataURL('image/png').split(',')[1];
  }
  function stats() {
    return { level: level + 1, levelName: CL.name, state, manMode: man.mode,
      chunks: chunks.length,
      liveCells: chunks.reduce((n, ch) => n + ch.cells.filter(a => !a.dead).length, 0),
      paperCells: paper.reduce((n, p) => n + !!p, 0), paperTorn,
      rooms: interiors.length, width: W, height: H, camera: { ...CAM } };
  }

  function prepare(config) {
    __cullRng.reset(0x7345abcd);
    PD.opt = { collapse: true, tool: 'none', shake: 1 };
    PD.st = ST0(); PD.ach = {}; PD.stars = {}; PD.best = {};
    toasts.length = 0;
    time = 0; shake = 0; freeze = 0; slow = 0; flash = 0;
    dragging = false; hint = 0; frameDt = 0;
    load(config.level === 'wide' ? LEVELS.findIndex(l => l.cols > 120) : config.level);
    W = config.width; H = config.height;
    WEB_UI.portrait = W < H;
    cv.width = W; cv.height = H;
    Object.assign(CAM, config.camera, { reset: false, snap: false, hold: 0 });
    Object.assign(TW_SPIN, { c: null, seen: -9, T: 0, acc: 0 });
    Object.assign(TW_BACK, { c: null, seen: -9, T: 0, acc: 0 });
    banner = 0; hint = 0;
    if (config.fixtures) {
      // Use real room/paper rendering, including torn seams and furniture.
      for (const rm of interiors)
        paperHit(rm.x + Math.floor(rm.w / 3), rm.y + Math.floor(rm.h / 2));
      // Exercise masonry displaced by the largest legal spring wobble.
      // The reference draws every tile, so both row and column edges matter.
      wobOn=true;
      for(let i=0;i<grid.length;i++)if(grid[i]){
        ox[i]=(i%3-1)*WOB_OMAX;oy[i]=((i/3|0)%3-1)*WOB_OMAX;
      }
      addEdgeFixtures();
    }
    return stats();
  }
  function addEdgeFixtures() {
    const x0 = CAM.x, x1 = x0 + W / CAM.z;
    const y0 = CAM.y, y1 = y0 + H / CAM.z;
    // All landlord types, windup/swing poses, HP/marker text and rotating
    // briefcases at fractional viewport edges and far outside the camera.
    const base=targets[0], kinds=['','walker','bat','thrower','boss','shield','sprinter','healer'];
    if(base)for(let k=0;k<kinds.length;k++){
      const d=[-170,-128,-90,-24,24,90,128,170][k];
      for(const [x,y] of [[x0+d,(y0+y1)/2],[x1-d,(y0+y1)/2],
                          [(x0+x1)/2,y0+d],[(x0+x1)/2,y1-d]]){
        targets.push({...base,x,y,alive:true,kind:kinds[k],face:k%2?1:-1,
          t:k*.19,wk:k*.4,mv:k%3,pan:k%3===0?.2:0,wind:k%3===1?.25:0,
          swing:k%3===2?.2:0,hpMax:3,hp:2,armor:k===5?1:0,
          collapseRolled:k%2?2:0});
        cases.push({x,y,rot:k*.47});
      }
    }
    for(let k=0;k<16;k++){
      const d=[-15,-14,-10,-1,0,1,10,15][k%8];
      for(const [x,y] of [[x0+d,(y0+y1)/2],[x1-d,(y0+y1)/2],
                          [(x0+x1)/2,y0+d],[(x0+x1)/2,y1-d]])
        cases.push({x,y,rot:k*Math.PI/8});
    }
    // Wide multi-cell fragments exercise both whole-chunk and individual-cell
    // rejection, all materials/damage states, and dead-cell skipping.
    for (let k = 0; k < 48; k++) {
      const x = k < 8 ? x0 + 30 + rnd() * (x1-x0) : rnd() * WW * 1.8 - WW * .4;
      const y = k < 8 ? y0 + 30 + rnd() * (y1-y0) : rnd() * 1400 - 450;
      const ang = (rnd()-.5) * Math.PI * 2, c = Math.cos(ang), s = Math.sin(ang);
      const cells = []; let rad = 0;
      for (let i = 0; i < 25; i++) {
        const lx = (i % 5 - 2) * 16, ly = (Math.floor(i / 5) - 2) * 16;
        const m = 1 + (i + k) % 8;
        rad = Math.max(rad, Math.hypot(lx, ly) + V);
        cells.push({ m, h: MAT[m].hp * [1, .5, .2][i % 3], lx, ly,
          wx: x + lx*c - ly*s, wy: y + lx*s + ly*c, dead: i % 19 === 0 });
      }
      chunks.push({ x, y, rad, ang, cells });
    }
    // Fractions, rotations, and +/-64px shake expose errors at every edge.
    for (const d of [-72, -38, -25, -18, -8, -.5, .5, 8, 18, 25, 38, 72]) {
      for (const [x, y] of [[x0+d,(y0+y1)/2], [x1+d,(y0+y1)/2],
          [(x0+x1)/2,y0+d], [(x0+x1)/2,y1+d],
          [x0+d,y0+d], [x1+d,y1+d]]) {
        for (const ang of [0, Math.PI/4, Math.PI/2, Math.PI*.83]) {
          chunks.push({ x, y, rad: V, ang,
            cells: [{ m: 4, h: MAT[4].hp*.5, lx: 0, ly: 0, wx: x, wy: y }] });
        }
      }
    }
  }
  function compare(config) {
    shake = config.shake; OPT().shake = config.setting;
    const saved = drawState();
    const render = fn => {
      restoreDrawState(saved);
      // Resizing resets ALL canvas state, paths and the backing pixels.
      cv.width = W; cv.height = H;
      __cullRng.reset(0x91b47d23, config.random);
      counts = { drawImage: 0, fillRect: 0 };
      fn();
      const result = { calls: counts, randomCalls: __cullRng.calls(),
                       pixels: ctx.getImageData(0, 0, W, H) };
      counts = null;
      return result;
    };
    // Alternate order so cache warm-up cannot consistently favor either path.
    let optimized, baseline;
    if (config.reverse) { baseline = render(reference); optimized = render(draw); }
    else { optimized = render(draw); baseline = render(reference); }
    restoreDrawState(saved);
    const a = optimized.pixels.data, b = baseline.pixels.data;
    let differentBytes = 0, differentPixels = 0, first = null;
    for (let i = 0; i < a.length; i += 4) {
      let different = false;
      for (let c = 0; c < 4; c++) if (a[i+c] !== b[i+c]) {
        differentBytes++; different = true;
      }
      if (different) {
        differentPixels++;
        if (!first) first = { x: (i/4) % W, y: Math.floor(i/4/W),
          optimized: Array.from(a.slice(i,i+4)), reference: Array.from(b.slice(i,i+4)) };
      }
    }
    const result = { ...config, ...stats(), optimized: optimized.calls,
      reference: baseline.calls, differentPixels, differentBytes,
      rgbaBytesCompared: a.length, first,
      randomCalls: [optimized.randomCalls, baseline.randomCalls] };
    if (differentPixels) {
      const diff = new ImageData(W,H);
      for (let i=0; i<a.length; i+=4) {
        diff.data[i] = Math.max(...[0,1,2].map(c => Math.abs(a[i+c]-b[i+c]))) ? 255 : 0;
        diff.data[i+3] = 255;
      }
      result.images = { optimized: png(optimized.pixels), reference: png(baseline.pixels), diff: png(diff) };
    } else if (config.capture) result.images = { optimized: png(optimized.pixels) };
    return result;
  }
  window.__canvasCull = { prepare, compare, stats,
    launch() { man.x = SX - 78; man.y = SY + 35; launch(); },
    tick(n) { for (let i=0; i<n; i++) update(1/60); return stats(); },
    collapse() {
      // A real support fracture, then the unchanged production physics.
      const rm = interiors[0];
      if (!rm) throw Error('Collapse fixture requires a real room');
      blast(rm.x + Math.floor(rm.w/2), rm.y + rm.h, rm.w/2 + 3, 800);
      supportPass();
      return stats();
    }
  };
  return { wideLevel: LEVELS.findIndex(l => l.cols > 120) + 1,
           switches: patches.map(p => p[0]) };
})();
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, help="Save JSON evidence and selected PNGs")
    parser.add_argument("--chromium", default=os.environ.get("CHROMIUM_EXECUTABLE"),
                        help="Chromium executable (default: CHROMIUM_EXECUTABLE or Playwright)")
    args = parser.parse_args()
    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        parser.error("Playwright is required: pip install playwright; playwright install chromium")
    if args.out_dir:
        args.out_dir.mkdir(parents=True, exist_ok=True)

    report = {"comparison": "complete production draw() RGBA; paper/tile/rubble/actor culling disabled in reference",
              "samples": []}
    errors = []

    def save_report():
        if args.out_dir:
            (args.out_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n")

    page = None
    with sync_playwright() as pw:
        try:
            launch = {"headless": True,
                      "args": ["--disable-gpu", "--disable-accelerated-2d-canvas"]}
            if args.chromium:
                launch["executable_path"] = args.chromium
            browser = pw.chromium.launch(**launch)
            report["browser"] = browser.version
            page = browser.new_page(viewport={"width": 960, "height": 1240}, device_scale_factor=1)
            page.on("pageerror", lambda error: errors.append("pageerror: " + str(error)))
            page.on("console", lambda message: errors.append("console.error: " + message.text)
                    if message.type == "error" else None)
            page.add_init_script(INIT)
            page.goto((Path(__file__).resolve().parents[1] / "web/index.html").as_uri(), wait_until="load")
            report["harness"] = page.evaluate(HARNESS)

            def compare(name, amplitude, setting, random_pair, capture=False):
                sample = page.evaluate("c => __canvasCull.compare(c)", {
                    "name": name, "shake": amplitude, "setting": setting,
                    "random": random_pair, "reverse": bool(len(report["samples"]) % 2),
                    "capture": capture and args.out_dir is not None,
                })
                images = sample.pop("images", {})
                if args.out_dir:
                    for kind, data in images.items():
                        filename = f"{name}-{kind}.png"
                        (args.out_dir / filename).write_bytes(base64.b64decode(data))
                report["samples"].append(sample)
                if sample["differentPixels"]:
                    save_report()
                    raise AssertionError(f"{name}: {sample['differentPixels']} differing pixels; first={sample['first']}")
                if sample["randomCalls"][0] != sample["randomCalls"][1]:
                    raise AssertionError(f"{name}: culling changed RNG consumption")
                if sample["optimized"]["drawImage"] > sample["reference"]["drawImage"]:
                    raise AssertionError(f"{name}: culling increased image submissions")
                if not sample["camera"]["on"] and sample["optimized"] != sample["reference"]:
                    raise AssertionError(f"{name}: culling changed no-camera drawing")
                return sample

            layouts = [
                ("portrait", 0, 540, 960, {"on": True, "x": 280.25, "y": -110.5, "z": .9}),
                ("tall-portrait-zoom", 5, 540, 1240, {"on": True, "x": 400.125, "y": 110.375, "z": 1.8}),
                ("landscape-zoom", 5, 960, 540, {"on": True, "x": 285.25, "y": 95.375, "z": 1.8}),
                ("wide-finale", "wide", 960, 540, {"on": True, "x": 1800.375, "y": 65.25, "z": .55}),
                ("no-camera", 5, 960, 540, {"on": False, "x": 0, "y": 0, "z": 1}),
            ]
            for name, level, width, height, camera in layouts:
                page.evaluate("c => __canvasCull.prepare(c)", {
                    "level": level, "width": width, "height": height,
                    "camera": camera, "fixtures": True,
                })
                start = len(report["samples"])
                for amplitude in (0, 4, 16, 128):
                    for setting in (0, .5, 1):
                        pairs = [(0, 0)] if amplitude * setting == 0 else [
                            (0, 0), (.999999, .999999), (0, .999999), (.999999, 0)]
                        for direction, pair in enumerate(pairs):
                            compare(f"{name}-shake{amplitude}-option{setting}-dir{direction}",
                                    amplitude, setting, pair)
                group = report["samples"][start:]
                print(f"{name}: {len(group)} full RGBA comparisons passed; "
                      f"drawImage {group[0]['reference']['drawImage']} -> "
                      f"{group[0]['optimized']['drawImage']} (zero shake)", flush=True)

            # Real, unsynthesized load/launch/update scenes supplement the boundary
            # fixtures. Rendering does not advance the physics or the cloth clock.
            live_chunks = 0
            for name, level, width, height, camera in (layouts[0], layouts[2], layouts[3]):
                page.evaluate("c => __canvasCull.prepare(c)", {
                    "level": level, "width": width, "height": height,
                    "camera": camera, "fixtures": False,
                })
                compare(f"live-{name}-loaded", 0, 1, (.5, .5), capture=True)
                page.evaluate("__canvasCull.launch()")
                previous = 0
                for frame in (1, 24, 48, 96, 180):
                    state = page.evaluate("n => __canvasCull.tick(n)", frame - previous)
                    previous = frame
                    live_chunks = max(live_chunks, state["chunks"])
                    compare(f"live-{name}-shot-frame{frame}", 16, 1, (0, .999999), capture=frame == 48)
                page.evaluate("__canvasCull.collapse()")
                for frame in (1, 12, 30):
                    state = page.evaluate("n => __canvasCull.tick(n)", frame)
                    live_chunks = max(live_chunks, state["chunks"])
                    compare(f"live-{name}-collapse-plus{frame}", 16, .5, (.999999, 0), capture=frame == 12)
                print(f"live-{name}: load, launch, flight and production collapse frames passed", flush=True)

            if live_chunks == 0:
                raise AssertionError("Live physics coverage never produced a rubble chunk")
            if errors:
                raise AssertionError("Browser errors: " + "; ".join(errors))
            samples = report["samples"]
            image_saved = sum(s["reference"]["drawImage"] - s["optimized"]["drawImage"] for s in samples)
            fill_saved = sum(s["reference"]["fillRect"] - s["optimized"]["fillRect"] for s in samples)
            if image_saved <= 0 or fill_saved <= 0:
                raise AssertionError("Fixtures must exercise actual rubble and wallpaper draw reduction")
            report["summary"] = {"passed": len(samples), "differentPixels": 0,
                "rgbaBytesCompared": sum(s["rgbaBytesCompared"] for s in samples),
                "drawImageCallsSaved": image_saved, "fillRectCallsSaved": fill_saved,
                "maxLivePhysicsChunks": live_chunks, "pageErrors": errors}
            save_report()
            print("BROWSER_CANVAS_CULL_PASS " + json.dumps(report["summary"], sort_keys=True))
            browser.close()
        except Exception as exc:
            report["error"] = str(exc)
            report["pageErrors"] = errors
            if args.out_dir and page is not None and not page.is_closed():
                try:
                    current = page.evaluate("document.getElementById('c')?.toDataURL('image/png').split(',')[1]")
                    if current:
                        (args.out_dir / "failure-current.png").write_bytes(base64.b64decode(current))
                except Exception as screenshot_error:
                    report["failureScreenshotError"] = str(screenshot_error)
            raise
        finally:
            save_report()



if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"BROWSER_CANVAS_CULL_FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
