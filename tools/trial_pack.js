'use strict';
function generateTrials(){
  const WIDTH=120,HEIGHT=60;
  const groups=[
    {label:"Tenement Row",prefix:"Tenement",wall:2,floor:1,roof:2,kind:[0,1,0,2],tip:"BREAK THE LOWER WALLS"},
    {label:"Glass Quarter",prefix:"Glass",wall:4,floor:6,roof:4,kind:[0,2,3,1],tip:"SHATTER GLASS, DROP FLOORS"},
    {label:"Rail Yards",prefix:"Rail",wall:6,floor:1,roof:2,kind:[1,2,0,4],tip:"DROP THE CONNECTING DECK"},
    {label:"Sand Works",prefix:"Sand",wall:2,floor:1,roof:3,kind:[0,1,2,0],tip:"RELEASE THE SAND BANK"},
    {label:"Powder District",prefix:"Powder",wall:6,floor:2,roof:1,kind:[3,2,4,0],tip:"HIT THE ORANGE TNT SUPPORTS"},
    {label:"Spring Heights",prefix:"Spring",wall:2,floor:1,roof:7,kind:[1,5,2,0],tip:"BANK OFF THE RUBBER ROOFS"},
    {label:"Delivery Works",prefix:"Delivery",wall:2,floor:6,roof:1,kind:[2,1,6,4],tip:"USE THE TUBE TO CROSS THE YARD"},
    {label:"Night Patrol",prefix:"Night",wall:3,floor:2,roof:6,kind:[4,5,6,7],tip:"EVICT GUARDS BEFORE THE BOSS"}
  ];
  const shapes=["Court","Stacks","Bridge","Terrace"];
  const palettes=["#e8d3a8","#d7dfe9","#d2b6b4","#b9d6be"];
  const levels=[],distinct=new Set();
  const fail=(msg)=>{throw Error(msg)};
  for(let district=0;district<8;district++)for(let shape=0;shape<4;shape++)for(let tier=0;tier<5;tier++){
    const g=groups[district],grid=new Uint8Array(WIDTH*HEIGHT),targets=[],rooms=[],pads=[],tubes=[],houses=[];
    const get=(x,y)=>grid[y*WIDTH+x];
    function fill(x,y,w,h,m){
      if(x<28||y<0||x+w>WIDTH||y+h>HEIGHT||w<1||h<1)fail("invalid rectangle "+[district,shape,tier,x,y,w,h,m]);
      for(let j=y;j<y+h;j++)grid.fill(m,j*WIDTH+x,j*WIDTH+x+w);
    }
    function target(x,y,kind,range=6){targets.push([x,y,kind,range])}
    function house(x,w,floors,fh,wall,floor,roof){
      const base=58,top=base-floors*fh;
      const H={x,w,top,base,fh,floors};houses.push(H);
      fill(x,base,w,2,3);
      for(let f=1;f<=floors;f++)fill(x,base-f*fh,w,2,f===floors?roof:floor);
      for(let f=0;f<floors;f++){
        const low=base-f*fh,high=low-fh;
        fill(x,high+2,2,fh-2,wall);fill(x+w-2,high+2,2,fh-2,wall);
        // Wide rooms have a central structural post and two usable bays.
        if(w>=22)fill(x+(w>>1),high+2,1,fh-2,wall===6?6:1);
        if(wall!==4){fill(x,low-5,1,2,4);fill(x+w-1,low-5,1,2,4)}
        rooms.push([x+2,high+2,w-4,fh-2,(district+houses.length-1)%4]);
        if(f===0||tier>=1||f===floors-1){
          const tx=x+Math.floor(w/4)+1;
          target(tx,low,g.kind[(f+houses.length+tier)%g.kind.length],Math.min(8,Math.floor(w/3)));
          if(w>=22&&tier>=3&&f===0)target(x+w-6,low,g.kind[(f+tier+1)%4],5);
        }
      }
      return H;
    }
    // Four different silhouettes per district; difficulty changes heights,
    // widths, spans and occupied floors, not just enemy count or names.
    const x=32+((district*3+tier)%5),fh=8+(tier%3)*2;
    if(shape===0){
      house(x,18+(tier%3)*2,1+(tier>>1),fh,g.wall,g.floor,g.roof);
      house(68+tier*2,24-(tier%3)*2,2+(tier>=3?1:0),fh,g.wall,g.floor,g.roof);
    }else if(shape===1){
      house(x+5,16+(tier%2)*2,3+(tier>=3?1:0),fh,g.wall,g.floor,g.roof);
      house(78-tier*2,20+(tier%3)*2,2+(tier%2),fh,g.wall,g.floor,g.roof);
    }else if(shape===2){
      const A=house(x,16+(tier%2)*2,3,fh,g.wall,g.floor,g.roof);
      const B=house(90-tier*2,18,3+(tier>=3?1:0),fh,g.wall,g.floor,g.roof);
      const y=58-fh*(tier%2+1),start=A.x+A.w,end=B.x;
      fill(start,y,end-start,2,g.floor);
      target(Math.floor((start+end)/2),y,g.kind[tier%4],6);
    }else{
      house(x,15,1+(tier%2),fh,g.wall,g.floor,g.roof);
      house(59+(tier%2),17,2+(tier>=3?1:0),fh,g.wall,g.floor,g.roof);
      house(91-(tier%2),17,3+(tier>=4?1:0),fh,g.wall,g.floor,g.roof);
    }
    // District mechanics are actually present in the voxel map and use
    // the existing material simulation rather than decorative props.
    for(const H of houses){
      if(district===0||district===4||district===7){fill(H.x,56,2,2,5);if(tier>=2)fill(H.x+H.w-2,56,2,2,5)}
      if(district===1){fill(H.x,56,2,2,4);fill(H.x+H.w-2,56,2,2,4)}
      if(district===5){const px=H.x+3;fill(px,H.top,4,1,7);pads.push([px,H.top,4])}
    }
    if(district===3){
      // Low, supported sand bank in the yard, out of every doorway.
      const A=houses[0],B=houses[1],start=A.x+A.w+2,space=B.x-start-2;
      const width=Math.min(space,12+tier*2);
      if(width>=5)for(let j=0;j<Math.min(5+tier,Math.ceil(width/2));j++)if(width-2*j>0)fill(start+j,59-j,width-2*j,1,8);
      fill(A.x,56,2,2,5);
    }
    if(district===6){
      const A=houses[0],B=houses[1],exitX=A.x+A.w+5,exitY=Math.max(8,Math.min(A.top,B.top)-5);
      tubes.push([29,35-tier*2,exitX,exitY]);
    }
    if(district===7&&tier>=3){
      // One boss on a ground-level platform, not a crowd of bosses.
      for(const t of targets)if(t[2]===7)t[2]=4;
      targets[0][2]=7;
    }else if(district===7)for(const t of targets)if(t[2]===7)t[2]=2;
    const name=g.prefix+" "+shapes[shape]+" "+(tier+1);
    const tip=shape===2&&district!==5&&district!==6?"CUT THE DECK OR ITS PIERS":g.tip;
    const men=Math.min(12,Math.max(5,Math.ceil(targets.length*.8)+tier+4));
    const walk=(district===5||district===7)&&tier>=2;
    if(targets.length<2||targets.length>20||rooms.length>20||pads.length>3||tubes.length>3)fail("entity cap "+name);
    const keys=new Set();
    for(const [tx,ty] of targets){
      if(tx<30||tx>=118||ty<5||ty>=59||!get(tx,ty)||!get(tx,ty+1))fail("unsupported target "+name+" "+[tx,ty]);
      for(let yy=ty-5;yy<ty;yy++)for(let xx=tx-2;xx<=tx+2;xx++)if(get(xx,yy))fail("obstructed target "+name+" "+[tx,ty,xx,yy]);
      const k=ty*WIDTH+tx;if(keys.has(k))fail("duplicate target "+name);keys.add(k);
    }
    // Verify every occupied voxel has a four-connected path to the ground.
    const seen=new Uint8Array(grid.length),queue=[];
    for(let cx=0;cx<WIDTH;cx++){const i=(HEIGHT-1)*WIDTH+cx;if(grid[i]){seen[i]=1;queue.push(i)}}
    for(let n=0;n<queue.length;n++){
      const i=queue[n],cx=i%WIDTH,cy=(i/WIDTH)|0;
      for(const [dx,dy] of [[1,0],[-1,0],[0,1],[0,-1]]){
        const xx=cx+dx,yy=cy+dy;if(xx<0||xx>=WIDTH||yy<0||yy>=HEIGHT)continue;
        const j=yy*WIDTH+xx;if(grid[j]&&!seen[j]){seen[j]=1;queue.push(j)}
      }
    }
    let occupied=0;for(let i=0;i<grid.length;i++)if(grid[i]){occupied++;if(!seen[i])fail("floating voxel "+name+" "+i)}
    const rows=Array.from({length:HEIGHT},(_,y)=>Array.from(grid.subarray(y*WIDTH,(y+1)*WIDTH)).join(''));
    const key=rows.join('');if(distinct.has(key))fail("duplicate geometry "+name);distinct.add(key);
    levels.push({id:60+levels.length,name,district:g.label,tip,men,walk,rows,targets,rooms,pads,tubes,occupied});
  }
  return levels;
}
function emitTrials(levels){
  const WIDTH=120,HEIGHT=60,palettes=["#e8d3a8","#d7dfe9","#d2b6b4","#b9d6be"],kinds=["","walker","thrower","bat","shield","sprinter","healer","boss"];
  const commands=[],table=[],districts=[];
  const collect=(l)=>{
    const grid=Uint8Array.from(l.rows.join(''),c=>Number(c)),rects=[];
    // Greedy exact rectangular tiling: each occupied voxel is emitted once.
    // All dimensions fit bytes, offsets fit uint32_t.
    for(let y=0;y<HEIGHT;y++)for(let x=28;x<WIDTH;x++){
      const i=y*WIDTH+x,m=grid[i];if(!m)continue;
      let w=1,h=1;while(x+w<WIDTH&&grid[i+w]===m)w++;
      outer:while(y+h<HEIGHT){for(let k=0;k<w;k++)if(grid[(y+h)*WIDTH+x+k]!==m)break outer;h++}
      for(let yy=y;yy<y+h;yy++)grid.fill(0,yy*WIDTH+x,yy*WIDTH+x+w);
      rects.push([x,y,w,h,m]);
    }
    const off=commands.length;for(const r of rects)commands.push(...r);
    const roomOff=commands.length;for(const room of l.rooms)commands.push(...room);
    const targetOff=commands.length;for(const t of l.targets)commands.push(...t);
    const padOff=commands.length;for(const p of l.pads)commands.push(...p);
    const tubeOff=commands.length;for(const t of l.tubes)commands.push(...t);
    return {off,rects:rects.length,roomOff,rooms:l.rooms.length,targetOff,targets:l.targets.length,padOff,pads:l.pads.length,tubeOff,tubes:l.tubes.length,name:l.name,tip:l.tip,men:l.men,walk:l.walk?1:0};
  };
  for(const l of levels)table.push(collect(l));
  for(let d=0;d<8;d++){
    const rows=levels.slice(d*20,(d+1)*20);
    districts.push(JSON.stringify({format:"MXTRIAL1",width:WIDTH,height:HEIGHT,district:rows[0].district,levels:rows},null,1)+"\n");
  }
  const rows=table.map(t=>`  {${JSON.stringify(t.name)},${JSON.stringify(t.tip)},${t.off},${t.rects},${t.roomOff},${t.rooms},${t.targetOff},${t.targets},${t.padOff},${t.pads},${t.tubeOff},${t.tubes},${t.men},${t.walk}}`).join(",\n");
  const header=`/* Generated by tools/trial_pack.js; edit the source generator and regenerate. */
#ifndef MX_TRIAL_LEVELS_H
#define MX_TRIAL_LEVELS_H
#include <stdint.h>
typedef struct {
  const char *name,*tip;
  uint32_t rect_off; uint16_t rect_n;
  uint32_t room_off; uint8_t room_n;
  uint32_t target_off; uint8_t target_n;
  uint32_t pad_off; uint8_t pad_n;
  uint32_t tube_off; uint8_t tube_n;
  uint8_t men,walk;
} MxTrialLevel;
static const uint8_t MX_TRIAL_DATA[] = {
${Array.from({length:Math.ceil(commands.length/32)},(_,i)=>"  "+commands.slice(i*32,(i+1)*32).join(",")).join(",\n")}
};
static const MxTrialLevel MX_TRIALS[] = {
${rows}
};
#endif
`;
  // ASCII code units 33..153; JSON escapes all sensitive characters. JS
  // reads only the chosen stage, so no giant raw grid allocations at boot.
  const packed=JSON.stringify(String.fromCharCode(...commands));
  const web=`// BEGIN MANDEX TRIAL DATA
const TRIAL_START=LEVELS.length;
const TRIAL_BYTES=${packed};
const TRIAL_META=${JSON.stringify(table.map(t=>[t.name,t.tip,t.off,t.rects,t.roomOff,t.rooms,t.targetOff,t.targets,t.padOff,t.pads,t.tubeOff,t.tubes,t.men,t.walk]))};
const TRIAL_DISTRICTS=${JSON.stringify(levels.filter((_,i)=>i%20===0).map(l=>l.district))};
function buildTrial(i){
  const a=TRIAL_META[i],read=j=>TRIAL_BYTES.charCodeAt(j),pals=${JSON.stringify(palettes)},kinds=${JSON.stringify(kinds)};
  for(let k=a[2],end=k+a[3]*5;k<end;k+=5)rect(read(k),read(k+1),read(k+2),read(k+3),read(k+4));
  for(let k=a[4],end=k+a[5]*5;k<end;k+=5)interiors.push({x:read(k),y:read(k+1),w:read(k+2),h:read(k+3),p:pals[read(k+4)]});
  for(let k=a[6],end=k+a[7]*4;k<end;k+=4)tgt(read(k),read(k+1),kinds[read(k+2)],read(k+3));
  for(let k=a[8],end=k+a[9]*3;k<end;k+=3)pads.push({x:read(k)*V,y:read(k+1)*V,w:read(k+2)*V});
  for(let k=a[10],end=k+a[11]*4;k<end;k+=4)tube('#ffb45d',read(k)*V,read(k+1)*V,read(k+2)*V,read(k+3)*V,0);
}
TRIAL_META.forEach((a,i)=>LEVELS.push({name:a[0],tip:a[1],men:a[12],walk:!!a[13],build(){buildTrial(i)}}));
// END MANDEX TRIAL DATA
`;
  return {header,web,districts,commandBytes:commands.length,occupied:levels.reduce((n,l)=>n+l.occupied,0)};
}
if(typeof module!=='undefined')module.exports={generateTrials,emitTrials};
