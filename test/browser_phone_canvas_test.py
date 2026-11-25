#!/usr/bin/env python3
"""Real Chromium regression: phone settings, saved low-end preset, scaled input.

Run with Playwright + Chromium; this checks behavior, not hardware FPS.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out-dir', type=Path, default=ROOT / 'out' / 'phone-performance')
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    from playwright.sync_api import sync_playwright
    report = {'status': 'fail', 'errors': [], 'checks': []}
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page(viewport={'width': 390, 'height': 844}, has_touch=True,
                                device_scale_factor=1)
        page.on('pageerror', lambda e: report['errors'].append(str(e)))
        page.on('console', lambda m: report['errors'].append(m.text) if m.type == 'error' else None)
        # Manually advance actual game loop to avoid machine-dependent timing
        # thresholds and guarantee a fresh paint after each tap.
        page.add_init_script("""window.requestAnimationFrame=()=>0;
          try{localStorage.clear()}catch(e){}""")
        try:
            page.goto((ROOT/'web'/'index.html').as_uri(), wait_until='load')
            page.evaluate("""() => {
              state='options';WEB_UI.page.options=0;muted=true;
              window.phonePaint=()=>{webNextDraw=-Infinity;last=performance.now();
                loop(last+1000/60)};
              window.phoneHit=(i)=>{const h=hits[i],r=cv.getBoundingClientRect();
                return {x:r.left+(h.x+h.w/2)*r.width/W,y:r.top+(h.y+h.h/2)*r.height/H}};
              phonePaint();
            }""")
            def tap(i):
                point = page.evaluate('i => phoneHit(i)', i)
                page.touchscreen.tap(point['x'], point['y'])
                page.evaluate('phonePaint()')
            tap(2)  # PERFORMANCE tab
            assert page.evaluate('settingsPage()') == 1, 'performance page unavailable'
            for _ in range(3):
                tap(5)  # Auto -> Quality -> Balanced -> Low End
            assert page.evaluate("webProfile(OPT())") == 'low'
            assert page.evaluate("JSON.parse(localStorage.getItem(SAVE_KEY)).opt.performance") == 'low'
            assert page.evaluate('cv.width===W&&cv.height===H'), 'settings should stay crisp'
            page.screenshot(path=str(args.out_dir/'01-portrait-performance.png'))
            report['checks'].append('trusted touch and persisted low-end profile')
            page.evaluate("""() => {
              load(-1,{name:'Phone performance smoke',men:3,custom:true,build(){
                rect(40,35,2,25,WOOD);rect(37,34,8,1,WOOD);tgt(95,59);
              }});
              banner=0;phonePaint();CAM.hold=0;camTick(0);phonePaint();
            }""")
            snapshot = page.evaluate("""() => ({logical:[W,H],buffer:[cv.width,cv.height],
              cap:webRenderCap(OPT()),budget:fxLimit(),lighting:webDetailedLighting,
              men,shots:lv.shots,mode:man.mode,portrait:isPortrait()})""")
            assert snapshot['portrait'] and snapshot['cap'] == 30
            assert snapshot['budget'] == 220 and not snapshot['lighting']
            assert snapshot['buffer'] == [round(v * .6) for v in snapshot['logical']]
            assert snapshot['mode'] == 'aim'
            start = page.evaluate("""() => {
              const [x,y]=toScreen(man.x,man.y),r=cv.getBoundingClientRect();
              return {x:r.left+x*r.width/W,y:r.top+y*r.height/H,
                sx:CAM.z*r.width/W,sy:CAM.z*r.height/H};
            }""")
            page.mouse.move(start['x'], start['y'])
            page.mouse.down()
            assert page.evaluate('dragging'), 'scaled pointer missed sling'
            page.mouse.move(start['x']-40*start['sx'],start['y']+10*start['sy'],steps=6)
            page.mouse.up()
            assert page.evaluate("man.mode==='fly'&&!dragging"), 'scaled pointer did not launch'
            assert page.evaluate('men') == snapshot['men'] - 1
            assert page.evaluate('lv.shots') == snapshot['shots'] + 1
            page.evaluate("() => {for(let i=0;i<20;i++)phonePaint()}")
            page.screenshot(path=str(args.out_dir/'02-low-end-gameplay.png'))
            report['checks'].append('scaled canvas, real pointer launch, protected fixed simulation')
            page.set_viewport_size({'width':844,'height':390})
            page.evaluate("() => {state='options';WEB_UI.page.options=1;phonePaint()}")
            assert page.evaluate('!isPortrait()&&W===960&&H===540&&cv.width===W&&cv.height===H')
            page.screenshot(path=str(args.out_dir/'03-landscape-performance.png'))
            report['checks'].append('landscape settings at full logical resolution')
            assert not report['errors'], report['errors']
            report['status'] = 'pass'
        finally:
            browser.close()
            (args.out_dir/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print('BROWSER_PHONE_CANVAS_PASS', json.dumps(report))

if __name__ == '__main__':
    main()
