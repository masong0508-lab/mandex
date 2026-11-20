/* Browser cutscene cinematography: keep the authored camera framing stable.
 * The test isolates cutCam from the HTML script (no browser needed), and
 * verifies that impact shake dies away while the shot stays composed. */
const fs=require('fs'),assert=require('assert');
const h=fs.readFileSync('web/index.html','utf8');
const start=h.indexOf('function cutCam(i,t){');
const end=h.indexOf('// Gerald seen from behind',start);
assert(start>=0&&end>start,'cutCam source is present');
const cases=[
 {cuts:[0,2.3,4.1],shake:[0,0,0],pan:[[0,0],[10,4],[0,0]],push:.04},
 {cuts:[0,1.6,3.2],shake:[0,5,8],tilt:[0,-.07,.05]},
 {cuts:[0,1.1,2.4,3.8],shake:[10,6,8,16],tilt:[0,-.08,.08,.04]},
 {cuts:[0,2.1,4.3],shake:[0,4,10],push:.02}
];
const cgK=(cuts,t)=>{let k=0;for(let j=0;j<cuts.length;j++)if(t>=cuts[j])k=j;return k};
const camera=Function('CUTS','cgK','return ('+h.slice(start,end).trim()+')')(cases,cgK);
let samples=0;
for(let i=0;i<cases.length;i++){
  const sc=cases[i];
  for(let k=0;k<sc.cuts.length;k++){
    const begin=sc.cuts[k];
    for(let age of [0,1/60,2/60,.12,.25,.5,1,1.8,3]){
      const t=(k+1<sc.cuts.length)?Math.min(begin+age,sc.cuts[k+1]-.00001):begin+age;
      if(t<begin)continue;
      const v=camera(i,t);
      for(const n of ['x','y','z','r','sx','sy','fl'])assert(Number.isFinite(v[n]),'non-finite '+n);
      assert(v.z>=.999&&v.z<=1.013,'shot zoom exceeded locked-camera envelope');
      assert(Math.abs(v.x-480)<=7.01&&Math.abs(v.y-270)<=5.01,'background drifts away from actors');
      assert(Math.abs(v.r)<=.0061,'jarring rotation');
      assert(Math.abs(v.sx)<=3.01&&Math.abs(v.sy)<=2.01,'excessive shake');
      assert(v.fl>=0&&v.fl<=.141,'excessive flash');
      if(t-begin>=1)assert(Math.abs(v.sx)+Math.abs(v.sy)<.003,'camera must settle after impact');
      samples++;
    }
  }
}
assert(h.includes('ctx.fillRect(W-260,26,115,25);'),'shot counter must clear the SKIP control');
assert(h.includes("txt('DAY '+day,W-36,88,38"),'day counter must clear the left scene heading');
assert(h.includes('Costume follows physical body anchors'),'costume placement lost its body anchoring');
assert(h.includes('short ballistic arc'),'transformation buttons lost physical trajectories');
assert(h.includes('On wide shots, the feet share a real contact shadow'),'grounding was removed');
assert(!h.includes('ctx.restore();cutDust(cutT);'),'random particles back in the film');
console.log('PASS: '+samples+' camera samples, costume/body grounding and timed impact motion');
