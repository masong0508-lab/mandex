/* Extract the production browser room-support functions and exercise
 * real logic on a synthetic structure; no mocked replacement algorithm. */
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync(require('path').join(__dirname,'../web/index.html'),'utf8');
const start=source.indexOf('function roomFrameSupport(r){');
const end=source.indexOf('function kill(x,y,bvx,bvy)',start);
assert(start>=0&&end>start,'room-support functions missing');
const program=`
const COLS=30,ROWS=18,grid=new Uint8Array(COLS*ROWS);
const paper=new Uint16Array(COLS*ROWS),interiors=[{x:7,y:4,w:12,h:8}];
let paperFrameInit=[],paperDirty=false;
const idx=(x,y)=>y*COLS+x,inb=(x,y)=>x>=0&&y>=0&&x<COLS&&y<ROWS;
`+source.slice(start,end)+`
const r=interiors[0];for(let x=r.x;x<r.x+r.w;x++){grid[idx(x,r.y-1)]=1;grid[idx(x,r.y+r.h)]=1}
for(let y=r.y;y<r.y+r.h;y++){grid[idx(r.x-1,y)]=1;grid[idx(r.x+r.w,y)]=1}
for(let y=r.y;y<r.y+r.h;y++)for(let x=r.x;x<r.x+r.w;x++)paper[idx(x,y)]=1;
paperFrameInit=interiors.map(roomFrameSupport);
const original=paperFrameInit[0];if(original<10)throw Error('missing frame');
paperDirty=true;paperRoomUpdate();
if(!paper[idx(13,8)])throw Error('intact room lost wallpaper');
paperHit(13,8);if(paper[idx(13,8)])throw Error('direct impact failed to tear wallpaper');
paper.fill(1);let removed=0;
for(let x=r.x;x<r.x+r.w&&removed<original/4;x++){
  if(grid[idx(x,r.y-1)]){grid[idx(x,r.y-1)]=0;removed++}
}
paperDirty=true;paperRoomUpdate();
if(!paper[idx(13,8)]||paperFrameInit[0]!==original)throw Error('minor damage destroyed room');
grid.fill(0);paperDirty=true;paperRoomUpdate();
if(paperFrameInit[0]!==0)throw Error('unsupported browser room did not collapse');
for(let y=r.y;y<r.y+r.h;y++)for(let x=r.x;x<r.x+r.w;x++)
  if(paper[idx(x,y)])throw Error('floating browser wallpaper remained');
'web room backdrop: impacts, partial damage and collapse passed';
`;
const result=vm.runInNewContext(program,{});
assert(/passed/.test(result));
console.log(result);
