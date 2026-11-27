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
    {label:"Night Patrol",prefix:"Night",wall:3,floor:2,roof:6,kind:[4,5,6,7],tip:"EVICT GUARDS BEFORE THE BOSS"},
    {label:"Cascade Works",prefix:"Cascade",wall:3,floor:6,roof:7,kind:[1,3,5,6],tip:"CHAIN BLASTS AND RUBBER REBOUNDS"},
    {"label":"Harbor Freight","prefix":"Harbor","wall":6,"floor":1,"roof":2,"kind":[1,2,3,4],"tip":"CUT THE STEEL GANTRY","tnt":1,"pad":0,"tube":1,"sand":0,"walk":0},
    {"label":"Foundry Lane","prefix":"Foundry","wall":2,"floor":6,"roof":1,"kind":[2,3,4,0],"tip":"SET OFF THE FOUNDATION CHARGES","tnt":1,"pad":0,"tube":0,"sand":0,"walk":0},
    {"label":"Dune Depot","prefix":"Dune","wall":3,"floor":1,"roof":2,"kind":[0,1,5,2],"tip":"RELEASE THE YARD SAND","tnt":0,"pad":0,"tube":0,"sand":1,"walk":0},
    {"label":"Skyline Arcade","prefix":"Skyline","wall":6,"floor":6,"roof":7,"kind":[1,5,3,6],"tip":"REBOUND FROM THE ROOFLINE","tnt":0,"pad":1,"tube":0,"sand":0,"walk":1},
    {"label":"Brick Market","prefix":"Market","wall":2,"floor":2,"roof":1,"kind":[0,4,1,2],"tip":"CLEAR THE LOWER SHOPS","tnt":1,"pad":0,"tube":0,"sand":0,"walk":1},
    {"label":"Pneumatic Post","prefix":"Post","wall":3,"floor":6,"roof":2,"kind":[2,6,1,4],"tip":"RIDE THE DELIVERY TUBE","tnt":0,"pad":0,"tube":1,"sand":0,"walk":0},
    {"label":"Railyard Switch","prefix":"Switch","wall":6,"floor":1,"roof":6,"kind":[1,4,2,5],"tip":"SEVER THE BRIDGE PIERS","tnt":1,"pad":0,"tube":0,"sand":0,"walk":0},
    {"label":"Quarry Exchange","prefix":"Quarry","wall":3,"floor":6,"roof":3,"kind":[4,1,0,6],"tip":"TOPPLE THE STONE STORAGE","tnt":1,"pad":0,"tube":0,"sand":1,"walk":0},
    {"label":"Canal Station","prefix":"Canal","wall":2,"floor":6,"roof":7,"kind":[2,1,5,3],"tip":"BANK ACROSS THE CANAL","tnt":0,"pad":1,"tube":1,"sand":0,"walk":0},
    {"label":"Clocktower Court","prefix":"Clock","wall":3,"floor":1,"roof":6,"kind":[3,5,4,6],"tip":"HIT THE TOWER FROM BELOW","tnt":0,"pad":0,"tube":0,"sand":0,"walk":1},
    {"label":"Glass Terminal","prefix":"Terminal","wall":4,"floor":6,"roof":4,"kind":[1,2,5,0],"tip":"BREAK THE FRAME UNDER THE GLASS","tnt":1,"pad":0,"tube":0,"sand":0,"walk":0},
    {"label":"Warehouse Nine","prefix":"Warehouse","wall":6,"floor":2,"roof":1,"kind":[2,4,6,3],"tip":"USE THE CHARGED CORNER POSTS","tnt":1,"pad":0,"tube":1,"sand":0,"walk":1},
    {"label":"Rubber Reservoir","prefix":"Reservoir","wall":2,"floor":1,"roof":7,"kind":[5,3,1,6],"tip":"USE ROOF PADS TO REACH THE FAR SIDE","tnt":0,"pad":1,"tube":0,"sand":1,"walk":1},
    {"label":"Steel Causeway","prefix":"Causeway","wall":6,"floor":6,"roof":2,"kind":[4,3,2,6],"tip":"DROP THE LOAD-BEARING DECK","tnt":1,"pad":0,"tube":0,"sand":0,"walk":0},
    {"label":"Rooftop Gardens","prefix":"Garden","wall":2,"floor":1,"roof":7,"kind":[0,5,6,1],"tip":"CROSS THE GARDEN ROOFS","tnt":0,"pad":1,"tube":1,"sand":0,"walk":1},
    {"label":"Factory Annex","prefix":"Annex","wall":3,"floor":2,"roof":6,"kind":[3,4,2,1],"tip":"BREAK THE SUPPORTS IN SEQUENCE","tnt":1,"pad":0,"tube":0,"sand":1,"walk":0},
    {"label":"Midnight Exchange","prefix":"Midnight","wall":3,"floor":6,"roof":1,"kind":[4,5,6,7],"tip":"EVICT THE GUARDS THEN THE BOSS","tnt":1,"pad":0,"tube":1,"sand":0,"walk":1},
    {"label":"Grand Demolition","prefix":"Grand","wall":6,"floor":6,"roof":7,"kind":[4,6,3,7],"tip":"COMBINE REBOUNDS AND BLAST SUPPORTS","tnt":1,"pad":1,"tube":1,"sand":0,"walk":1},
    {"label":"Switchback Depot","prefix":"Switchback","wall":2,"floor":1,"roof":2,"kind":[0,1,2,3],"tip":"CLIMB THE YARD STEPS; PUNCH WOOD","tnt":0,"pad":0,"tube":0,"sand":0,"walk":2},
    {"label":"Rooftop Relay","prefix":"Relay","wall":3,"floor":6,"roof":7,"kind":[1,5,2,4],"tip":"CLIMB THE STEPS; REBOUND OVER WALLS","tnt":0,"pad":1,"tube":1,"sand":0,"walk":2},
    {"label":"Three-Stage Circuit","prefix":"Circuit","wall":6,"floor":2,"roof":7,"kind":[4,6,3,7],"tip":"BREAK THE SUPPORTS; FINISH THE CIRCUIT","tnt":1,"pad":1,"tube":1,"sand":0,"walk":1}
  ].concat([
  {
    "label": "Rubble Raceway",
    "prefix": "Raceway",
    "wall": 2,
    "floor": 1,
    "roof": 7,
    "kind": [
      1,
      5,
      2,
      0
    ],
    "tip": "BANK AND RUN THROUGH THE RUBBLE",
    "tnt": 0,
    "pad": 1,
    "tube": 0,
    "sand": 0,
    "walk": 2
  },
  {
    "label": "Pressure Foundry",
    "prefix": "Pressure",
    "wall": 6,
    "floor": 2,
    "roof": 1,
    "kind": [
      3,
      2,
      4,
      1
    ],
    "tip": "DIVE WITH QUAKE AT THE POSTS",
    "tnt": 1,
    "pad": 0,
    "tube": 0,
    "sand": 0,
    "walk": 0
  },
  {
    "label": "Airship Docks",
    "prefix": "Airship",
    "wall": 4,
    "floor": 6,
    "roof": 7,
    "kind": [
      2,
      1,
      5,
      3
    ],
    "tip": "USE THE BEANS RECOVERY JET",
    "tnt": 0,
    "pad": 1,
    "tube": 1,
    "sand": 0,
    "walk": 0
  },
  {
    "label": "Silt Refinery",
    "prefix": "Silt",
    "wall": 3,
    "floor": 1,
    "roof": 2,
    "kind": [
      0,
      2,
      6,
      4
    ],
    "tip": "RELEASE SILT THEN BREAK THE FRAME",
    "tnt": 1,
    "pad": 0,
    "tube": 0,
    "sand": 1,
    "walk": 1
  },
  {
    "label": "Brick Observatory",
    "prefix": "Observatory",
    "wall": 2,
    "floor": 6,
    "roof": 4,
    "kind": [
      2,
      3,
      1,
      0
    ],
    "tip": "SHATTER THE TOWER WINDOWS",
    "tnt": 0,
    "pad": 0,
    "tube": 1,
    "sand": 0,
    "walk": 0
  },
  {
    "label": "Spring Foundry",
    "prefix": "Springforge",
    "wall": 6,
    "floor": 1,
    "roof": 7,
    "kind": [
      5,
      4,
      2,
      6
    ],
    "tip": "REBOUND ABOVE THE STEEL FRAME",
    "tnt": 1,
    "pad": 1,
    "tube": 0,
    "sand": 0,
    "walk": 2
  },
  {
    "label": "Courier Alley",
    "prefix": "Courier",
    "wall": 2,
    "floor": 2,
    "roof": 1,
    "kind": [
      2,
      1,
      5,
      6
    ],
    "tip": "PARRY CASES AND CLIMB THE STEPS",
    "tnt": 0,
    "pad": 0,
    "tube": 1,
    "sand": 0,
    "walk": 2
  },
  {
    "label": "Marble Exchange",
    "prefix": "Marble",
    "wall": 3,
    "floor": 6,
    "roof": 3,
    "kind": [
      4,
      3,
      6,
      7
    ],
    "tip": "BREAK SUPPORTS BEFORE THE BOSS",
    "tnt": 1,
    "pad": 0,
    "tube": 0,
    "sand": 1,
    "walk": 1
  },
  {
    "label": "Glass Viaduct",
    "prefix": "Viaduct",
    "wall": 4,
    "floor": 6,
    "roof": 4,
    "kind": [
      1,
      2,
      3,
      5
    ],
    "tip": "CUT THE PIERS UNDER THE BRIDGE",
    "tnt": 1,
    "pad": 0,
    "tube": 1,
    "sand": 0,
    "walk": 0
  },
  {
    "label": "Sandstone Works",
    "prefix": "Sandstone",
    "wall": 3,
    "floor": 2,
    "roof": 2,
    "kind": [
      0,
      1,
      2,
      4
    ],
    "tip": "RELEASE SAND INTO THE YARD",
    "tnt": 0,
    "pad": 0,
    "tube": 0,
    "sand": 1,
    "walk": 1
  },
  {
    "label": "Bumper Gardens",
    "prefix": "Bumper",
    "wall": 2,
    "floor": 1,
    "roof": 7,
    "kind": [
      5,
      6,
      1,
      2
    ],
    "tip": "USE RUBBER PADS TO CROSS ROOFS",
    "tnt": 0,
    "pad": 1,
    "tube": 1,
    "sand": 0,
    "walk": 2
  },
  {
    "label": "Iron Courthouse",
    "prefix": "Ironcourt",
    "wall": 6,
    "floor": 6,
    "roof": 2,
    "kind": [
      4,
      3,
      2,
      7
    ],
    "tip": "BREAK THE COURT FOUNDATIONS",
    "tnt": 1,
    "pad": 0,
    "tube": 0,
    "sand": 0,
    "walk": 1
  },
  {
    "label": "Night Freight",
    "prefix": "Nightfreight",
    "wall": 3,
    "floor": 6,
    "roof": 1,
    "kind": [
      2,
      5,
      6,
      4
    ],
    "tip": "PARRY THE THROWERS ON FOOT",
    "tnt": 1,
    "pad": 0,
    "tube": 1,
    "sand": 0,
    "walk": 2
  },
  {
    "label": "Shatter Plaza",
    "prefix": "Shatter",
    "wall": 4,
    "floor": 1,
    "roof": 4,
    "kind": [
      1,
      2,
      0,
      3
    ],
    "tip": "BREAK GLASS AND DROP THE FLOORS",
    "tnt": 0,
    "pad": 0,
    "tube": 0,
    "sand": 0,
    "walk": 0
  },
  {
    "label": "Floodplain Yard",
    "prefix": "Floodplain",
    "wall": 2,
    "floor": 6,
    "roof": 7,
    "kind": [
      0,
      5,
      4,
      6
    ],
    "tip": "BANK ABOVE THE SAND BANK",
    "tnt": 0,
    "pad": 1,
    "tube": 0,
    "sand": 1,
    "walk": 1
  },
  {
    "label": "Powder Junction",
    "prefix": "Junction",
    "wall": 6,
    "floor": 2,
    "roof": 6,
    "kind": [
      2,
      3,
      4,
      7
    ],
    "tip": "CHAIN TNT THEN DIVE WITH QUAKE",
    "tnt": 1,
    "pad": 0,
    "tube": 1,
    "sand": 0,
    "walk": 0
  },
  {
    "label": "Finale: Arrival",
    "prefix": "Arrival",
    "wall": 2,
    "floor": 1,
    "roof": 7,
    "kind": [
      1,
      2,
      5,
      0
    ],
    "tip": "REBOUND INTO THE FIRST SECTOR",
    "tnt": 0,
    "pad": 1,
    "tube": 1,
    "sand": 0,
    "walk": 0
  },
  {
    "label": "Finale: Furnace",
    "prefix": "Furnace",
    "wall": 6,
    "floor": 2,
    "roof": 1,
    "kind": [
      3,
      2,
      4,
      6
    ],
    "tip": "COLLAPSE THE FURNACE SUPPORTS",
    "tnt": 1,
    "pad": 0,
    "tube": 0,
    "sand": 0,
    "walk": 1
  },
  {
    "label": "Finale: Sandbank",
    "prefix": "Sandbank",
    "wall": 3,
    "floor": 1,
    "roof": 2,
    "kind": [
      0,
      1,
      2,
      4
    ],
    "tip": "RELEASE THE SAND AND ADVANCE",
    "tnt": 1,
    "pad": 0,
    "tube": 0,
    "sand": 1,
    "walk": 0
  },
  {
    "label": "Finale: Skybridge",
    "prefix": "Skybridge",
    "wall": 4,
    "floor": 6,
    "roof": 7,
    "kind": [
      2,
      5,
      6,
      3
    ],
    "tip": "RECOVER FLIGHT ACROSS THE BRIDGE",
    "tnt": 0,
    "pad": 1,
    "tube": 1,
    "sand": 0,
    "walk": 2
  },
  {
    "label": "Finale: Guardhouse",
    "prefix": "Guardhouse",
    "wall": 6,
    "floor": 6,
    "roof": 2,
    "kind": [
      4,
      3,
      2,
      7
    ],
    "tip": "PARRY AND BREAK THE GUARDHOUSE",
    "tnt": 1,
    "pad": 0,
    "tube": 0,
    "sand": 0,
    "walk": 2
  },
  {
    "label": "Six-Stage Grand Circuit",
    "prefix": "Grand Circuit",
    "wall": 3,
    "floor": 6,
    "roof": 7,
    "kind": [
      4,
      6,
      3,
      7
    ],
    "tip": "FINISH ALL SIX SECTORS",
    "tnt": 1,
    "pad": 1,
    "tube": 1,
    "sand": 0,
    "walk": 1
  }
]);
  const shapes=["Court","Stacks","Bridge","Terrace"];
  const palettes=["#e8d3a8","#d7dfe9","#d2b6b4","#b9d6be"];
  const levels=[],distinct=new Set();
  const fail=(msg)=>{throw Error(msg)};
  for(let district=0;district<groups.length;district++)for(let shape=0;shape<4;shape++)for(let tier=0;tier<5;tier++){
    if(district===51&&shape===3&&tier===4)continue; // final district has 19 maps
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
      for(let f=1;f<=floors;f++){
        const y=base-f*fh,m=f===floors?roof:floor;fill(x,y,w,2,m);
        if(m===4)fill(x,y+1,w,1,6); // load-bearing frame under the glazing
      }
      for(let f=0;f<floors;f++){
        const low=base-f*fh,high=low-fh;
        fill(x,high+2,2,fh-2,wall);fill(x+w-2,high+2,2,fh-2,wall);
        // Wide rooms have a central structural post and two usable bays.
        if(w>=16)fill(x+(w>>1),high+2,1,fh-2,wall===6||wall===4?6:1);
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
    if(district>=9){
      const d=district-9, fy=8+2*((d+tier)%2);
      if(shape===0){
        house(31+(d%4)+tier%2,16+2*((d+tier)%3),1+(tier>>1),fy,g.wall,g.floor,g.roof);
        house(69+(d%3)*3+tier,18+2*((d+2*tier)%3),2+(tier>=2?1:0),fy,g.wall,g.floor,g.roof);
      }else if(shape===1){
        if(d%2===0){
          house(32+d%4,16+2*(tier%2),2+(tier>=3?1:0),fy,g.wall,g.floor,g.roof);
          house(70+d%5,20+2*(tier%2),3+(tier>=2?1:0)+(tier===4?1:0),fy,g.wall,g.floor,g.roof);
        }else{
          house(34+d%4,20+2*(tier%2),3+(tier>=2?1:0)+(tier===4?1:0),fy,g.wall,g.floor,g.roof);
          house(77+d%5,16+2*(tier%2),2+(tier>=3?1:0),fy,g.wall,g.floor,g.roof);
        }
      }else if(shape===2){
        const A=house(30+d%5,16+2*(tier%2),3+(tier===4?1:0),fy,g.wall,g.floor,g.roof);
        const B=house(87-d%4-tier,18+2*(d%2),3+(tier>=3?1:0),fy,g.wall,g.floor,g.roof);
        const y=58-fy*(1+((tier+d)%2)),start=A.x+A.w,end=B.x;
        fill(start,y,end-start,2,6);
        for(let px=start+4;px<end-1;px+=8)fill(px,y+2,2,58-y,6);
        target(Math.floor((start+end)/2),y,g.kind[tier%4],5);
      }else{
        house(30+d%3,14+2*(tier%2),1+(tier%2),fy,g.wall,g.floor,g.roof);
        house(57+d%3,16+2*(d%2),2+(tier>=2?1:0),fy,g.wall,g.floor,g.roof);
        house(88+d%4,16+2*(tier%2),3+(tier>=3?1:0),fy,g.wall,g.floor,g.roof);
      }
      for(const H of houses){
        if(g.tnt){fill(H.x,56,2,2,5);if(tier>=2)fill(H.x+H.w-2,56,2,2,5)}
        if(g.pad){fill(H.x+3,H.top,4,1,7);pads.push([H.x+3,H.top,4])}
      }
      if(g.tube&&(shape===2||shape===3||tier>=2)){
        const A=houses[0],B=houses[1];tubes.push([29,36-tier*2,A.x+A.w+4,Math.max(8,Math.min(A.top,B.top)-5)]);
      }
      if(g.sand&&shape!==2){
        const A=houses[0],B=houses[1],start=A.x+A.w+2,width=Math.min(B.x-start-2,10+tier*2);
        if(width>=5)for(let y=0;y<Math.min(4+tier,Math.ceil(width/2));y++)for(let cx=start+y;cx<start+width-y;cx++)if(!get(cx,59-y))fill(cx,59-y,1,1,8);
      }
      if(g.kind.includes(7)){for(const q of targets)if(q[2]===7)q[2]=4;if(tier>=3)targets[0][2]=7}
    }else if(shape===0){
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
      // Piers carry the span according to the wood/brick/steel load limits.
      for(let px=start+5;px<end-1;px+=10)fill(px,y+2,2,58-y,6);
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
      if(width>=5)for(let j=0;j<Math.min(5+tier,Math.ceil(width/2));j++)for(let cx=start+j;cx<start+width-j;cx++)if(!get(cx,59-j))fill(cx,59-j,1,1,8);
      fill(A.x,56,2,2,5);
    }
    if(district===8){
      // Mixed-mechanic district: blast supports, spring roofs, and a tube on bridges.
      for(const H of houses){fill(H.x,56,2,2,5);if(tier>=2)fill(H.x+H.w-2,56,2,2,5);pads.push([H.x+3,H.top,4]);}
      const A=houses[0],B=houses[1];
      if(shape===2||shape===3)tubes.push([29,36-tier*2,A.x+A.w+5,Math.max(8,Math.min(A.top,B.top)-5)]);
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
    // Walker-first routes: solid yard steps (two-cell rise, three/four-cell
    // tread), a breakable entrance and a rubber launch ledge where safe.
    // Geometry stays in the same 120x60 working grid. No invisible
    // platforms, scripted teleports or extra collision systems.
    if(district>=27){
      const A=houses[0],B=houses[1],left=A.x+A.w+1,right=B.x-1;
      const clearance=(x,y)=>targets.every(([tx,ty])=>x<tx-2||x>tx+2||y<ty-5||y>=ty);
      // Choose a contiguous clear bay rather than overwriting a pier,
      // tube mouth, target or house. Every column grows from ground.
      let start=-1,span=0;
      for(let x=left;x<right;){
        let end=x;while(end<right&&Array.from({length:9},(_,j)=>59-j).every(y=>!get(end,y)&&clearance(end,y)))end++;
        if(end-x>span){span=end-x;start=x}
        x=end===x?x+1:end+1;
      }
      const steps=Math.min(4,Math.floor(span/3));
      if(steps>=2){
        const tread=Math.min(4,Math.floor(span/steps));
        for(let k=0;k<steps;k++){
          const h=2*(k+1),sx=start+k*tread;
          fill(sx,60-h,tread,h,3);
          // The highest tread is a real rubber tile, checked by walk
          // physics even when the decorative pad list is already full.
          if(g.pad&&k===steps-1){fill(sx,60-h,tread,1,7);if(pads.length<3)pads.push([sx,60-h,tread])}
        }
      }
      // Timber entry panel is intentionally punchable; the frame stays
      // supported until the player actually breaks it.
      if(district<29)for(const H of houses){
        for(let y=H.base-5;y<H.base;y++)for(let x=H.x;x<H.x+2;x++)
          if(get(x,y)&&get(x,y)!==4&&clearance(x,y))fill(x,y,1,1,1);
      }
    }
    if(district>=30){
      // Short grounded entry steps: vary the first approach and give
      // walkers a low punchable ledge, without intruding on the sling.
      const d=district-30,h=1+d%3,hh=1+Math.floor(d/3)%3,mat=1+Math.floor(d/9);
      fill(28,60-h,1,h,mat);fill(29,60-hh,1,hh,mat);
    }
    const name=g.prefix+" "+shapes[shape]+" "+(tier+1);
    const tip=district>=9?g.tip:district===8?(shape===2||shape===3?"TUBE ACROSS; BREAK SUPPORTS":"BLAST SUPPORTS, BANK OFF ROOFS"):shape===2&&district!==5&&district!==6?"CUT THE DECK OR ITS PIERS":g.tip;
    const men=Math.min(12,Math.max(5,Math.ceil(targets.length*.8)+tier+4));
    const walk=(district===5||district===7)&&tier>=2 || district===8&&tier>=3 || district>=9&&(g.walk===2||!!g.walk&&tier>=2);
    if(targets.length<2||targets.length>20||rooms.length>20||pads.length>3||tubes.length>3)fail("entity cap "+name);
    const keys=new Set();
    for(const [tx,ty] of targets){
      if(tx<30||tx>=118||ty<5||ty>=59||!get(tx,ty)||!get(tx,ty+1))fail("unsupported target "+name+" "+[tx,ty]);
      for(let yy=ty-5;yy<ty;yy++)for(let xx=tx-2;xx<=tx+2;xx++)if(get(xx,yy))fail("obstructed target "+name+" "+[tx,ty,xx,yy]);
      const k=ty*WIDTH+tx;if(keys.has(k))fail("duplicate target "+name);keys.add(k);
    }
    // Match the core support flood: upward transfers reset lateral span;
    // sideways/downward transfers must fit each material's actual limit.
    const lim=[0,6,9,14,2,2,16,8,8],span=new Uint8Array(grid.length);span.fill(255);
    const support=[];for(let x=0;x<WIDTH;x++){const i=(HEIGHT-1)*WIDTH+x;if(grid[i]){span[i]=0;support.push(i)}}
    for(let n=0;n<support.length;n++){
      const i=support[n],cx=i%WIDTH,cy=(i/WIDTH)|0,h=span[i];
      for(const [dx,dy] of [[1,0],[-1,0],[0,1],[0,-1]]){
        const xx=cx+dx,yy=cy+dy;if(xx<0||xx>=WIDTH||yy<0||yy>=HEIGHT)continue;
        const j=yy*WIDTH+xx,m=grid[j],next=dy===-1?0:h+1;
        if(m&&next<span[j]&&next<=lim[m]){span[j]=next;support.push(j)}
      }
    }
    for(let i=0;i<grid.length;i++)if(grid[i]&&grid[i]!==8&&span[i]===255)fail('unsupported material span '+name+' '+[i%WIDTH,(i/WIDTH)|0,grid[i]]);
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
  for(let d=0;d<levels.length/20;d++){
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
  // Pack binary commands into script-safe ASCII. Only the selected map is
  // decoded; no uncompressed level grid is retained at startup.
  const alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  let encoded="";
  for(let i=0;i<commands.length;i+=3){
    const a=commands[i],b=commands[i+1]||0,c=commands[i+2]||0;
    encoded+=alphabet[a>>2]+alphabet[((a&3)<<4)|(b>>4)]+
      (i+1<commands.length?alphabet[((b&15)<<2)|(c>>6)]:"=")+
      (i+2<commands.length?alphabet[c&63]:"=");
  }
  const packed=JSON.stringify(encoded);
  const web=`// BEGIN MANDEX TRIAL DATA
const TRIAL_START=LEVELS.length;
const TRIAL_BYTES=${packed};
const TRIAL_META=${JSON.stringify(table.map(t=>[t.name,t.tip,t.off,t.rects,t.roomOff,t.rooms,t.targetOff,t.targets,t.padOff,t.pads,t.tubeOff,t.tubes,t.men,t.walk]))};
const TRIAL_DISTRICTS=${JSON.stringify(levels.filter((_,i)=>i%20===0).map(l=>l.district))};
function buildTrial(i){
  const a=TRIAL_META[i],digit=c=>c>=65&&c<=90?c-65:c>=97&&c<=122?c-71:c>=48&&c<=57?c+4:c===43?62:63,read=j=>{
    const off=Math.floor(j/3)*4,part=j%3,p=digit(TRIAL_BYTES.charCodeAt(off+part)),q=digit(TRIAL_BYTES.charCodeAt(off+part+1));
    return part===0?(p<<2)|(q>>4):part===1?((p&15)<<4)|(q>>2):((p&3)<<6)|q;
  },pals=${JSON.stringify(palettes)},kinds=${JSON.stringify(kinds)};
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

/* Regenerate the editable maps and both runtime representations together.
 * The HTML remains standalone and is never loaded from a remote endpoint. */
if(typeof require==='function'&&require.main===module){
  const fs=require('fs'),path=require('path'),root=path.join(__dirname,'..');
  const out=emitTrials(generateTrials());
  const dir=path.join(root,'content/trials');fs.mkdirSync(dir,{recursive:true});
  out.districts.forEach((d,i)=>fs.writeFileSync(path.join(dir,'district-'+(i+1)+'.json'),d));
  fs.writeFileSync(path.join(root,'core/trial_levels.h'),out.header);
  const webPath=path.join(root,'web/index.html'),html=fs.readFileSync(webPath,'utf8');
  const a=html.indexOf('// BEGIN MANDEX TRIAL DATA'),marker='// END MANDEX TRIAL DATA';
  const b=html.indexOf(marker,a);
  if(a<0||b<a)throw Error('trial data markers missing from game');
  const end=b+marker.length+(html[b+marker.length]==='\n'?1:0);
  fs.writeFileSync(webPath,html.slice(0,a)+out.web+html.slice(end));
  console.log('Generated '+generateTrials().length+' trials; '+out.commandBytes+' binary command bytes.');
}
