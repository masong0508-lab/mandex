#!/usr/bin/env python3
"""Live Chromium test of the real 20 three-part circuits, wide HQ
reward accounting, walk tubes, save boundaries and responsive directory."""
import argparse, json, os
from pathlib import Path
from playwright.sync_api import sync_playwright

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--out-dir",default="out/browser-relay")
    parser.add_argument("--chromium",default=os.environ.get("CHROMIUM_EXECUTABLE"))
    a=parser.parse_args()
    out=Path(a.out_dir);out.mkdir(parents=True,exist_ok=True)
    html=(Path(__file__).resolve().parents[1]/"web/index.html")
    script=r"""() => {
      let checks=0,transitions=0,walks=0;
      function ok(v,s){checks++;if(!v)throw Error(s)}
      OPT().fight=false;OPT().collapse=false;
      for(let k=0;k<20;k++){
        const l=640+k;load(l);ok(level===l&&CL.relayPhase===0&&CL.walk,'first phase');
        const old=PD.stars[l]||0;
        men=0;score=12345;lv.shots=5;lv.booms=2;combo=2;comboPeak=3;comboLeft=2;
        let last=score;
        for(let d=0;d<3;d++){
          ok(level===l&&CL.relayPhase===d&&state==='play','phase id');
          ok(targets.length===TRIAL_META[l-40+20*d-60][7],'actual phase target layout');
          if(CL.walk)walks++;
          for(const t of targets)t.alive=false;
          winT=1.61;update(1/60);
          if(d<2){
            transitions++;
            last+=3000;ok(state==='play'&&CL.relayPhase===d+1&&men===(d+1)*6&&score===last,'phase bonus exactly once');
            ok(lv.shots===5&&lv.booms===2&&(PD.stars[l]||0)===old,'retain run stats but no early save');
            ok(man.mode==='aim'&&!dragging&&chunks.length===0&&cases.length===0,'fresh stage input and debris');
          }else ok(state==='win'&&score===last+men*3000&&(PD.stars[l]||0)>=1,'final award only once');
        }
        retry();ok(level===l&&CL.relayPhase===0&&score===0,'retry returns to first phase');
      }
      load(39);const base=men;
      goSling(2);for(const t of targets)if(t.zone<2)t.alive=false;
      const before=score;zoneAdvance(false);ok(men===base+8&&score===before+6000,'HQ skipped-sling rewards');
      zoneAdvance(false);ok(men===base+8&&score===before+6000,'HQ rewards never duplicate');
      load(620);enterWalk();man.x=120;man.y=200;
      tubes=[{ax:120,ay:200,bx:180,by:120,r:26,ang:0,sp:480,col:'#fff'}];
      const clock=man.wt;walkTubeStep(man,1/60);
      ok(man.mode==='walk'&&man.x===180&&man.y===120&&man.wt===clock&&man.vx===176&&man.tubeCd>.8,'walk tube transfers safely');
      const x=man.x;walkTubeStep(man,1/60);ok(man.x===x,'transfer cooldown');
      state='select';selTab=4;WEB_UI.districtPicker=true;trialDirectoryPage=0;layoutTick();
      // Exercise each real page and each district callback, redrawing between
      // gestures exactly as a user does. Never require all 53 on one screen.
      const savedBtn=btn;
      let controls=[];
      btn=function(x,y,w,h,label,fn,o){controls.push({x,y,w,h,label,fn});return savedBtn(x,y,w,h,label,fn,o)};
      try {
        let visited=new Set(),pages=0;
        for(let page=0;page<TRIAL_DISTRICTS.length;page++){
          WEB_UI.districtPicker=true;trialDirectoryPage=page;controls=[];draw();
          const districtButtons=controls.filter(c=>c.label==='');
          ok(districtButtons.length>0,'directory page has district cards');
          pages++;
          for(let k=0;k<districtButtons.length;k++){
            districtButtons[k].fn();
            ok(trialDistrict>=0&&trialDistrict<TRIAL_DISTRICTS.length&&!visited.has(trialDistrict),'directory visits a distinct district');
            visited.add(trialDistrict);
            WEB_UI.districtPicker=true;
          }
          const next=controls.find(c=>c.label==='PAGE >');
          if(visited.size===TRIAL_DISTRICTS.length)break;
          ok(!!next,'remaining districts have a next-page control');next.fn();
          ok(trialDirectoryPage===page+1,'directory next page advances once');
        }
        ok(visited.size===53&&pages>=3,'all 53 districts accessible');
      } finally {btn=savedBtn;}
      return {checks,transitions,walkPhases:walks,levels:LEVELS.length}
    }"""
    results=[]
    with sync_playwright() as p:
        browser=p.chromium.launch(headless=True, executable_path=a.chromium or None,args=["--no-sandbox"])
        for width,height in [(960,540),(390,844)]:
            page=browser.new_page(viewport={"width":width,"height":height})
            errors=[]
            page.on("pageerror",lambda e:errors.append(str(e)))
            page.add_init_script("window.requestAnimationFrame=()=>0;window.setInterval=()=>0;try{localStorage.clear()}catch(e){}")
            page.goto(html.as_uri(),wait_until="load")
            result=page.evaluate(script)
            assert not errors,errors
            result["viewport"]=[width,height];results.append(result)
            page.screenshot(path=str(out/("directory-%sx%s.png"%(width,height))))
            page.close()
        browser.close()
    (out/"report.json").write_text(json.dumps(results,indent=2))
    print("BROWSER_RELAY_LIVE_PASS",json.dumps(results))
if __name__=="__main__":
    main()
