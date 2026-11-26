#!/usr/bin/env python3
"""Actual Canvas and DOM audit across phone, tablet and landscape sizes."""
from pathlib import Path
from playwright.sync_api import sync_playwright
root=Path(__file__).resolve().parents[1]
html=(root/"web/index.html").read_text()
sizes=[(280,480),(320,568),(360,640),(390,844),(412,915),(360,1000),(768,1024),(1024,768),(568,320),(844,390),(1280,720)]
with sync_playwright() as p:
    b=p.chromium.launch()
    total=[0,0,0]
    for w,h in sizes:
        page=b.new_page(viewport={"width":w,"height":h},has_touch=True)
        errors=[]
        page.on("pageerror",lambda e: errors.append(str(e)))
        page.evaluate("window.requestAnimationFrame=()=>0")
        page.set_content(html)
        result=page.evaluate("""() => {
          const failures=[];let screens=0,controls=0,plans=0;
          const drawText=uiText;
          uiText=function(v,x,y,w,h,o){
            const q=drawText(v,x,y,w,h,o);
            if(q){plans++;ctx.save();ctx.font=uiFont(q.size);
              for(const l of q.lines)if(ctx.measureText(l).width>w-3.9)failures.push('text width '+v);
              ctx.restore();
              if(x<-.01||y<-.01||x+w>W+.01||y+h>H+.01)failures.push('text bounds '+state+' '+v);
            }return q;
          };
          function inspect(label){layoutTick();camTick(0);frameDt=0;draw();screens++;
            for(const a of hits){controls++;if(a.x<-.01||a.y<-.01||a.x+a.w>W+.01||a.y+a.h>H+.01||a.w<=0||a.h<=0)failures.push('button bounds '+label);}
            for(let i=0;i<hits.length;i++)for(let j=i+1;j<hits.length;j++){const a=hits[i],b=hits[j];if(Math.min(a.x+a.w,b.x+b.w)-Math.max(a.x,b.x)>.1&&Math.min(a.y+a.h,b.y+b.h)-Math.max(a.y,b.y)>.1)failures.push('button overlap '+label);}
          }
          layoutTick();goMenu();inspect('menu');openLevelLibrary();inspect('library');
          PD.custom.push({n:'A deliberately very long imported building name with words that wrap correctly on compact phones — Test',m:4,c:'',t:[]});
          for(const tab of [-2,0,2,3,4,1]){selectRoute(tab);inspect('library '+tab);WEB_UI.page.select=999;inspect('last page '+tab);}
          selectRoute(4,17);inspect('district');selectRoute(1,-1,PD.custom[PD.custom.length-1]);inspect('custom');
          state='ach';inspect('ach');WEB_UI.page.ach=999;inspect('ach last');state='outfit';inspect('outfit');WEB_UI.page.outfit=999;inspect('outfit last');
          openEditor(false);inspect('editor brush');WEB_UI.page.edit=1;inspect('editor actions');E.starters=true;inspect('editor starters');E.starters=false;
          load(0);banner=1;inspect('aim banner');banner=0;combo=100;comboLeft=1;tntLiveFuses=88;inspect('aim alerts');
          man.mode='fly';man.pwr='beans';man.beaned=false;inspect('fly');man.pwr='burp';man.burped=false;inspect('fly burp');
          load(20);man.mode='walk';man.wt=20;man.gr=true;man.face=1;man.gait=0;man.turn=0;inspect('walk');
          state='win';inspect('win');state='lose';inspect('lose');startCut();
          for(const i of [0,1,2]){state='cutscene';cutI=i;cutT=3;inspect('cutscene '+i)}
          goMenu();openSettings();
          for(const t of ['performance','gameplay','audio','data']){settingsTab=t;drawSettings();const el=document.getElementById('settings-content');if(el.scrollWidth>el.clientWidth+1)failures.push('settings overflow '+t);}
          closeSettings();return {screens,controls,plans,failures};
        }""")
        assert not errors,(w,h,errors)
        assert not result["failures"],(w,h,result["failures"])
        for i,k in enumerate(("screens","controls","plans")):total[i]+=result[k]
        page.close()
    print("RESPONSIVE_TEXT_PASS",dict(viewports=len(sizes),screens=total[0],controls=total[1],text_layouts=total[2]))
    b.close()
