#!/usr/bin/env python3
"""Live gadget selection, paused simulation and one-shot stock checks."""
import argparse
import json
from pathlib import Path
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]
INIT = """window.requestAnimationFrame=()=>0; try{localStorage.clear()}catch(_){}"""
HARNESS = r"""
() => {
  muted=true; OPT().muted=true; OPT().fight=false;
  OPT().performance='quality'; OPT().renderScale=1;
  load(0); PD.unlockAll=false;
  layoutTick(); camTick(0); frameDt=0; draw();
  function point(x,y){const r=cv.getBoundingClientRect();return {x:r.left+x*r.width/W,y:r.top+y*r.height/H}}
  function snap(){return {picker:WEB_UI.toolPicker,mode:man.mode,time,men,shots:lv.shots,tool:OPT().tool,
    selected:pw.sel,stock:{...pw.left},pwr:man.pwr,dragging,x:man.x,y:man.y,portrait:isPortrait()}}
  window.__toolLive={snap,toolbar:()=>point(130,H-28),card(id){const c=toolPickerLayout(W,H).cards.find(c=>c.id===id);return point(c.x+c.w/2,c.y+c.h/2)},
    sling(){const p=toScreen(man.x,man.y);return point(p[0],p[1])},
    pull(){const p=toScreen(man.x-70,man.y+8);return point(p[0],p[1])},
    tick(n){for(let i=0;i<n;i++){webNextDraw=-Infinity;loop(last+1000/60)}return snap()},
    paint(){frameDt=0;draw();return snap()}
  };
  return snap();
}
"""

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out-dir',type=Path,required=True)
    out=parser.parse_args().out_dir
    out.mkdir(parents=True,exist_ok=True)
    reports=[]
    with sync_playwright() as p:
        browser=p.chromium.launch(headless=True)
        for width,height in [(960,540),(390,844)]:
            page=browser.new_page(viewport={'width':width,'height':height},device_scale_factor=1,has_touch=height>width)
            errors=[]
            page.on('pageerror',lambda e:errors.append(str(e)))
            page.on('console',lambda m: errors.append(m.text) if m.type=='error' else None)
            page.add_init_script(INIT)
            page.goto((ROOT/'web/index.html').as_uri(),wait_until='load')
            initial=page.evaluate(HARNESS)
            def click(point):
                if height>width: page.touchscreen.tap(point['x'],point['y'])
                else: page.mouse.click(point['x'],point['y'])
            def snap(): return page.evaluate('__toolLive.snap()')
            click(page.evaluate('__toolLive.toolbar()'))
            assert snap()['picker'],snap()
            frozen=page.evaluate('__toolLive.tick(12)')
            assert frozen['picker'] and frozen['time']==initial['time'],(initial,frozen)
            assert frozen['stock']==initial['stock'] and frozen['shots']==initial['shots'],frozen
            page.screenshot(path=str(out/f'picker-{width}x{height}.png'))
            click(page.evaluate("__toolLive.card('valve')"))
            assert snap()['picker'] and snap()['selected'] is None,snap()
            click(page.evaluate("__toolLive.card('turbo')"))
            armed=page.evaluate('__toolLive.paint()')
            assert not armed['picker'] and armed['selected']=='turbo',armed
            assert armed['stock']==initial['stock'] and armed['men']==initial['men'],armed
            if width>height:
                start=page.evaluate('__toolLive.sling()')
                end=page.evaluate('__toolLive.pull()')
                page.mouse.move(start['x'],start['y'])
                page.mouse.down()
                page.mouse.move(end['x'],end['y'],steps=5)
                assert snap()['dragging'],snap()
                page.mouse.up()
                shot=snap()
                assert shot['mode']=='fly' and shot['pwr']=='turbo',shot
                assert shot['stock']['turbo']==0 and shot['shots']==initial['shots']+1 and shot['men']==initial['men']-1,shot
            else:
                page.keyboard.press('KeyT')
                assert snap()['picker'],snap()
                page.keyboard.press('Escape')
                assert not snap()['picker'] and snap()['selected']=='turbo',snap()
            assert not errors,errors
            reports.append({'viewport':[width,height],'frozen':frozen,'armed':armed,'final':snap()})
            page.close()
        browser.close()
    (out/'report.json').write_text(json.dumps(reports,indent=2)+'\n')
    print('PASS: live gadget picker, paused frames, locked valve, touch choice and charged launch')

if __name__=='__main__':
    main()
