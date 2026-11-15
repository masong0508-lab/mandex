#!/usr/bin/env python3
"""Contact sheet of the web cutscenes: python3 tools/cut_shots.py out.png origin|wolf|end1|end2 [scene ...]   (each scene shown at 3 moments)"""
import sys, os, base64
from playwright.sync_api import sync_playwright
out, which = sys.argv[1], sys.argv[2]; scenes = [int(a) for a in sys.argv[3:]]
html = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'web', 'index.html')
JS = """([which,scenes])=>{CUTS=which==='wolf'?CUT3:which==='end1'?CUT_E1:which==='end2'?CUT_E2:CUT;const n=CUTS.length;if(!scenes.length)for(let i=0;i<n;i++)scenes.push(i);
 const w=cv.width,h=cv.height;const tmp=document.createElement('canvas');tmp.width=w*3/2;tmp.height=h*scenes.length/2;const tc=tmp.getContext('2d');
 scenes.forEach((s,row)=>{const S=CUTS[s];const rt=cutRT?cutRT.call(null):0;[.45,.5,.95].forEach((f,col)=>{cutI=s;cutFx=0;const total=S.cuts[S.cuts.length-1]+2.2;cutT=f*total;state='cutscene';ctx.setTransform(1,0,0,1,0,0);ctx.clearRect(0,0,w,h);try{drawCutscene()}catch(e){console.log('ERR',s,e.message)}tc.drawImage(cv,col*w/2,row*h/2,w/2,h/2)})});
 return tmp.toDataURL('image/png')}"""
with sync_playwright() as p:
    b = p.chromium.launch(); pg = b.new_page(viewport={'width': 1000, 'height': 600})
    pg.on('pageerror', lambda e: print('PAGEERR', e)); pg.on('console', lambda m: print(m.text) if m.type == 'error' else None)
    pg.goto('file://' + os.path.abspath(html)); pg.wait_for_timeout(400)
    pg.evaluate("()=>{window.requestAnimationFrame=()=>0}"); pg.wait_for_timeout(100)
    open(out, 'wb').write(base64.b64decode(pg.evaluate(JS, [which, scenes]).split(',')[1])); b.close(); print('saved', out)
