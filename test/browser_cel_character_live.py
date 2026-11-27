#!/usr/bin/env python3
"""Render real Golden Cels gameplay character poses in headless Chromium.
The canvas belongs to the game; no external concept art or mock assets."""
import argparse
import base64
import json
from pathlib import Path
from playwright.sync_api import sync_playwright

SCRIPT = r"""() => {
  const ok=(v,m)=>{if(!v)throw Error(m)};
  const frames=[];
  load(0);time=.35;JIG=null;NOTOWEL=true;
  for(let i=0;i<CH1-1;i++)PD.stars[i]=1;
  ok(!artUnlocked()&&!cycleArtStyle(),'art must remain locked before final chapter stage');
  PD.stars[CH1-1]=1;
  ok(artUnlocked(),'chapter completion unlocks art');
  const shots=[
    ['front',()=>drawFront('flex','smirk',.35,{nocape:true})],
    ['profile',()=>drawSide(.35,'scream')],
    ['inflated',()=>drawSeatSide(1,.35)],
    ['recovered',()=>drawSeatSide(.05,.35)]
  ];
  for(let a=0;a<=360;a+=30)
    shots.push(['turn-'+a,()=>drawSpin(a*Math.PI/180,{ld:0,face:'base',dir:1,hip:0})]);
  // Compare the actual two-bottle sequence, including the release recovery.
  // Restore global pressure / outfit state after each isolated drawing.
  for(const t of [CT_A+.12,CT_BK+.10,CT_BK+.31,CT_C+.12,CT_TB+.18,CT_D+.04,CT_END-.02]){
    const ph=t<CT_BK?1:t<CT_C?2:t<CT_TB?3:t<CT_D?4:5;
    shots.push(['drink-'+Math.round(t*1000),()=>{
      const th=ph===1?Math.PI*easeIO((t-CT_A)/(CT_BK-CT_A)):ph===2?Math.PI:ph===3?Math.PI+Math.PI*easeIO((t-CT_C)/(CT_TB-CT_C)):0;
      drawChugMan({inf:.15,bel:.36,chug:{t,ph,th,keep:0,rel:true}});
    }]);
  }

  const paint=(fn)=>{
    cv.width=960;cv.height=540;ctx.setTransform(1,0,0,1,0,0);
    ctx.clearRect(0,0,960,540);ctx.save();ctx.translate(480,270);ctx.scale(2,2);
    INF=0;JIG=null;fn();ctx.restore();
    const data=ctx.getImageData(0,0,960,540).data;
    let count=0,bounds=[960,540,-1,-1],hash=2166136261;
    for(let i=0;i<data.length;i+=4){
      for(let k=0;k<4;k++)hash=Math.imul(hash^data[i+k],16777619)>>>0;
      if(data[i+3]>8){count++;const p=i/4,x=p%960,y=(p/960)|0;
        bounds[0]=Math.min(bounds[0],x);bounds[1]=Math.min(bounds[1],y);
        bounds[2]=Math.max(bounds[2],x);bounds[3]=Math.max(bounds[3],y);}
    }
    ok(count>500&&bounds[0]>0&&bounds[1]>0&&bounds[2]<959&&bounds[3]<539,'empty or clipped pose');
    return {count,bounds,hash,png:cv.toDataURL('image/png').split(',')[1]};
  };
  OPT().artStyle='classic';refreshArt();
  for(const [name,fn] of shots)frames.push({name,classic:paint(fn)});
  ok(cycleArtStyle()&&CEL_ACTIVE,'Golden Cels activation');
  for(let i=0;i<shots.length;i++){
    const f=frames[i];f.cel=paint(shots[i][1]);ok(f.classic.hash!==f.cel.hash,'unchanged art: '+f.name);
    // Drinking is new choreography rather than just a reskin, so use a
    // wider silhouette bound for articulated prop and pressure poses.
    const lo=f.name.startsWith('drink-')?.45:.72,hi=f.name.startsWith('drink-')?2:1.28;
    ok(f.cel.count/f.classic.count>lo&&f.cel.count/f.classic.count<hi,'character proportions changed excessively: '+f.name);
  }
  ok(cycleArtStyle()&&!CEL_ACTIVE,'classic restored');
  const restored=paint(shots[0][1]);ok(restored.hash===frames[0].classic.hash,'classic front art changed');
  return {frames,classicRestored:true,chapter:CH1};
}"""

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--out-dir",default="out/browser-canvas/character")
    args=parser.parse_args()
    out=Path(args.out_dir)
    out.mkdir(parents=True,exist_ok=True)
    html=Path(__file__).resolve().parents[1]/"web/index.html"
    with sync_playwright() as p:
        browser=p.chromium.launch(headless=True,args=["--no-sandbox"])
        page=browser.new_page(viewport={"width":960,"height":540})
        errors=[]
        page.on("pageerror",lambda e:errors.append(str(e)))
        page.add_init_script("window.requestAnimationFrame=()=>0;window.setInterval=()=>0;try{localStorage.clear()}catch(e){}")
        page.goto(html.as_uri(),wait_until="load")
        report=page.evaluate(SCRIPT)
        for frame in report["frames"]:
            for style in ("classic","cel"):
                (out/(frame["name"]+"-"+style+".png")).write_bytes(base64.b64decode(frame[style].pop("png")))
        report["errors"]=errors
        (out/"report.json").write_text(json.dumps(report,indent=2))
        assert not errors,errors
        print("GOLDEN_CELS_LIVE_PASS",len(report["frames"]),"real poses, classic restored")
        browser.close()

if __name__=="__main__":
    main()
