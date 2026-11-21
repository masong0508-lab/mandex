/* Render production room dressing with real paper and voxel coordinates.
   Destroying the room's backing paper or replacing it with material must
   hide the window/furniture; the camera must reject out-of-view rooms. */
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const src=fs.readFileSync(require('path').join(__dirname,'../web/index.html'),'utf8');
const begin=src.indexOf('function drawRoomDetails('),end=src.indexOf('function draw(){',begin);
assert(begin>0&&end>begin,'production room dressing function must exist');
const boot=`
const COLS=50,ROWS=32,V=8,level=0;
const interiors=[{x:10,y:5,w:24,h:12},{x:1,y:4,w:8,h:6}];
const paper=new Uint16Array(COLS*ROWS),grid=new Uint8Array(COLS*ROWS);
const idx=(x,y)=>y*COLS+x,inb=(x,y)=>x>=0&&y>=0&&x<COLS&&y<ROWS;
let strokes=0;
const ctx={fillStyle:'',fillRect(x,y,w,h){if(!(w>0&&h>0))throw Error('invalid dressing quad');strokes++}};
for(let r=0;r<interiors.length;r++){let a=interiors[r];for(let y=a.y;y<a.y+a.h;y++)for(let x=a.x;x<a.x+a.w;x++)paper[idx(x,y)]=r+1}
`+src.slice(begin,end)+`
drawRoomDetails(0,50,0,32);const intact=strokes;
if(intact<25)throw Error('room detail absent');
strokes=0;drawRoomDetails(40,50,0,32);
if(strokes)throw Error('offscreen rooms still draw');
strokes=0;
// Intact walls occlude every candidate window/furniture anchor.
const a=interiors[0],n=a.w>=18?3:a.w>=11?2:1;
for(let k=0;k<n;k++){let wx=a.x+Math.floor(a.w*(2*k+1)/(2*n)),wy=a.y+(a.h>>1);grid[idx(wx,wy)]=2}
grid[idx(a.x+2,a.y+a.h-2)]=2;
drawRoomDetails(a.x,a.x+a.w,a.y,a.y+a.h);
if(strokes)throw Error('voxels failed to occlude damaged interior');
grid.fill(0);
strokes=0;
for(let y=a.y;y<a.y+a.h;y++)for(let x=a.x;x<a.x+a.w;x++)paper[idx(x,y)]=0;
drawRoomDetails(a.x,a.x+a.w,a.y,a.y+a.h);
if(strokes)throw Error('collapsed room left floating windows or furniture');
'PASS: camera culling, destructible occlusion, wallpaper collapse';
`;
const result=vm.runInNewContext(boot,{});
assert(/PASS/.test(result));console.log(result);
