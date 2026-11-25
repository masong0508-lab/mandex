/* Browser loop regression: gameplay time must be monitor-independent and
 * backlog work must remain bounded after a suspended / slow frame.
 * This executes the exact scheduler lifted from the standalone HTML, so the
 * web build gains no runtime dependency or allocations. */
'use strict';
const fs=require('fs');
const path=require('path');
const assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const begin=html.indexOf('const WEB_SIM_DT=');
const end=html.indexOf('let last=performance.now()',begin);
assert(begin>0&&end>begin,'inline fixed-step scheduler exists');
assert(/\bticks\s*=\s*webSimSteps\(raw\)/.test(html),'game loop actually uses scheduler');
assert(html.includes('update(WEB_SIM_DT);'),'game logic uses fixed timestep');
assert(html.includes('frameMeterSims+=sim;'),'FPS overlay counts real simulation calls');
const build=new Function(html.slice(begin,end)+'\nreturn {tick:webSimSteps, carry:()=>webSimCarry};');
function run(fps,seconds){
  const clock=build();
  let steps=0;
  for(let i=0;i<fps*seconds;i++)steps+=clock.tick(1/fps);
  assert(Math.abs(clock.carry())<1e-7,'no runaway carry at '+fps+' Hz');
  return steps;
}
for(const fps of [24,30,60,75,90,120,144]){
  assert.strictEqual(run(fps,10),600,'10 seconds = 600 simulation steps at '+fps+' Hz');
}
const lag=build();
assert.strictEqual(lag.tick(.5),4,'long frame has a four-step budget');
assert(lag.carry()<1e-7,'long frame discards excess backlog');
assert.strictEqual(lag.tick(1/60),1,'normal pacing recovers immediately after a stall');
assert.strictEqual(lag.tick(0),0,'zero elapsed time does not simulate');
const jitter=build();
let elapsed=0,steps=0;
for(let i=0;i<600;i++){
  const dt=[.007,.024,.016,.011,.018,.021][i%6];
  elapsed+=dt;steps+=jitter.tick(dt);
}
assert(Math.abs(steps/60-elapsed)<1/60+1e-6,'jitter preserves average elapsed time');
const script=html.slice(html.indexOf('<script>')+8,html.indexOf('</script>',html.indexOf('<script>')));
new Function(script); /* Full browser script still parses. */
console.log('ok: browser fixed-step clock stable at 24-144Hz, under jitter, and after stalls');
