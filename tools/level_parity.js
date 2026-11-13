// Compares every level the shared C core builds with the web game's: men, landlord positions and the whole grid after load().
// usage: gcc -std=c99 -O2 -Icore core/mx.c test/level_dump.c -lm -o /tmp/level_dump && /tmp/level_dump > /tmp/c_levels.txt
//        node tools/level_parity.js /tmp/c_levels.txt [first] [last] [webIndices]   (core level l is compared with web LEVELS[webIndices[l-first]]; default same index.
//        The consoles have 34 levels: 0-19 story, 20-23 = web 20,21,23,24 (tubes, hourglass, two slingshots), 24-33 = web 27-36.  Walk-Up Wreckage and the two fight-back levels are web-only.)
//        node tools/level_parity.js /tmp/c_levels.txt 0 33 0,1,...,19,20,21,23,24,27,28,...,36
const {chromium}=require('playwright'),fs=require('fs'),path=require('path');
(async()=>{
 const cfile=process.argv[2],lo=+(process.argv[3]||0),hi=+(process.argv[4]||29),map=(process.argv[5]||'').split(',').filter(x=>x!=='').map(Number);
 const lines=fs.readFileSync(cfile,'utf8').split('\n');const C={};
 for(let i=0;i<lines.length;){const m=/^L (\d+) men (\d+) targets (\d+)/.exec(lines[i]);if(!m){i++;continue}
  const l=+m[1],nt=+m[3];const T=[];for(let k=0;k<nt;k++){const t=lines[i+1+k].split(' ');T.push([+t[1],+t[2]])}
  const rows=lines.slice(i+1+nt,i+1+nt+60);C[l]={men:+m[2],T,rows};i+=1+nt+60}
 const b=await chromium.launch();const p=await b.newPage({viewport:{width:960,height:540}});
 const errs=[];p.on('pageerror',e=>errs.push(e.message));
 await p.goto('file://'+path.resolve(__dirname,'../web/index.html'));await p.waitForTimeout(300);
 let bad=0;
 for(let l=lo;l<=hi;l++){
  const H=await p.evaluate(`(function(){draw=function(){};load(${map.length?map[l-lo]:l});const rows=[];for(let y=0;y<ROWS;y++){let s='';for(let x=0;x<COLS;x++)s+=grid[y*COLS+x];rows.push(s)}
    return{men:CL.men,name:CL.name,T:targets.map(t=>[Math.round(t.x/4),Math.round(t.y)]),rows}})()`);
  const c=C[l];let msg=[];
  if(!c){console.log(l,'missing in C dump');bad++;continue}
  if(H.men!==c.men)msg.push(`men web ${H.men} core ${c.men}`);
  if(JSON.stringify(H.T)!==JSON.stringify(c.T))msg.push(`targets web ${JSON.stringify(H.T)} core ${JSON.stringify(c.T)}`);
  let diff=0,first=null;for(let y=0;y<H.rows.length;y++)for(let x=0;x<H.rows[y].length;x++)if(H.rows[y][x]!==c.rows[y][x]){diff++;if(!first)first=[x,y,H.rows[y][x],c.rows[y][x]]}
  if(diff)msg.push(`${diff} cells differ, first at x=${first[0]} y=${first[1]} (web ${first[2]}, core ${first[3]})`);
  console.log(String(l).padStart(2),H.name.padEnd(24),msg.length?'DIFF: '+msg.join('; '):'identical');if(msg.length)bad++}
 console.log(bad?`${bad} level(s) differ`:'all compared levels identical');if(errs.length)console.log('page errors:',errs.slice(0,3));
 await b.close();process.exit(bad?1:0)})();
