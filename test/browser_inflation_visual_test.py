#!/usr/bin/env python3
"""Render actual game art with the browser's real Canvas, including low presets."""
from pathlib import Path
from playwright.sync_api import sync_playwright

root=Path(__file__).resolve().parents[1]
with sync_playwright() as p:
    b=p.chromium.launch()
    page=b.new_page(viewport={"width":960,"height":540})
    errors=[]
    page.on("pageerror",lambda e: errors.append(str(e)))
    page.add_init_script("window.requestAnimationFrame = () => 0;")
    page.goto((root/"web/index.html").as_uri())
    result=page.evaluate("""() => {
      load(0); const counts=[];
      for(const tier of [0,3]){
        perfTier=tier;perfApply();
        ctx.setTransform(1,0,0,1,0,0);ctx.clearRect(0,0,cv.width,cv.height);
        drawValveSupport(36,420,480);
        const support=ctx.getImageData(12,375,50,105).data;
        let solid=0;for(let i=3;i<support.length;i+=4)if(support[i]>127)solid++;
        if(solid<50)throw Error('station support missing');
        for(const mode of ['seated','aim','fly','stuck','drink']){
          for(const f of [0,.25,.6,.95,2.2,20]){
            for(const ph of mode==='drink'?[1,2,3,4,5]:[0]){
              man.mode=mode==='seated'||mode==='drink'?'aim':mode;
              man.x=480;man.y=270;man.inf=f;man.bel=0;man.infVisual=inflationTarget(f);
              man.infRaw=f;man.infPump=.8;man.infPhase=1;man.expressionBlush=.8;
              man.seated=mode==='seated';man.vlv=man.seated;
              man.chug=mode==='drink'?{ph,th:ph===1?1:ph===2?Math.PI:ph===3?4.4:0,t:ph===2?.7:1.5,keep:0}:null;
              man.ang=.24;man.face=1;man.t=.2;man.a=1;man.dive=false;man.pop=0;
              ctx.setTransform(1,0,0,1,0,0);ctx.clearRect(0,0,cv.width,cv.height);
              renderMan();
              const data=ctx.getImageData(360,140,240,240).data;
              let n=0;for(let i=3;i<data.length;i+=4)if(data[i]>127)n++;
              if(n<50||n>20000)throw Error('missing or unbounded silhouette: '+mode+' '+f+' '+n);
              counts.push(n);
            }
          }
        }
      }
      let resultFrames=0;
      for(const tier of [0,3]){
        perfTier=tier;perfApply();
        for(const win of [true,false])for(let variant=0;variant<3;variant++)for(const t of [0,.22,.6,1.1,1.9,2.5,3.1]){
          ctx.setTransform(1,0,0,1,0,0);ctx.clearRect(0,0,cv.width,cv.height);
          ctx.save();ctx.translate(480,250);drawResultActor(win,t,variant);ctx.restore();
          const pixels=ctx.getImageData(380,140,200,220).data;let n=0;
          for(let i=3;i<pixels.length;i+=4)if(pixels[i]>127)n++;
          if(n<100||n>30000)throw Error('bad result silhouette '+win+' '+variant+' '+t);
          resultFrames++;
        }
      }
      return {frames:counts.length,resultFrames,min:Math.min(...counts),max:Math.max(...counts)};
    }""")
    assert not errors,errors
    print("INFLATION_CANVAS_PASS",result)
    b.close()
