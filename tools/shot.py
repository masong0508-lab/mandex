#!/usr/bin/env python3
"""Draw poses side by side at 3.4x from web/index.html's own drawing code (needs Playwright).
  python3 tools/shot.py out.png "drawFront('star','dizzy',0.4)" "drawSide(0.1,'scream')" ...   [--html path]"""
import sys, os, base64
from playwright.sync_api import sync_playwright
args = sys.argv[1:]; html = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'web', 'index.html')
if '--html' in args: i = args.index('--html'); html = args[i+1]; del args[i:i+2]
if len(args) < 2: sys.exit(__doc__)
out, poses = args[0], args[1:]
JS = """(poses)=>{cv.width=Math.max(600,poses.length*420);cv.height=520;ctx.setTransform(1,0,0,1,0,0);ctx.fillStyle='#8ecdf5';ctx.fillRect(0,0,cv.width,cv.height);
 poses.forEach((e,i)=>{ctx.save();ctx.translate(210+i*420,300);ctx.scale(3.4,3.4);try{(0,eval)(e)}catch(err){console.log('ERR',e,err.message)}ctx.restore()});
 return cv.toDataURL('image/png')}"""
with sync_playwright() as p:
    b = p.chromium.launch(); pg = b.new_page(viewport={'width': 1000, 'height': 600})
    pg.on('pageerror', lambda e: print('PAGEERR', e)); pg.on('console', lambda m: print(m.text) if m.type == 'error' else None)
    pg.goto('file://' + os.path.abspath(html)); pg.wait_for_timeout(300)
    pg.evaluate("()=>{window.requestAnimationFrame=()=>0}"); pg.wait_for_timeout(100)
    open(out, 'wb').write(base64.b64decode(pg.evaluate(JS, poses).split(',')[1])); b.close(); print('saved', out)
