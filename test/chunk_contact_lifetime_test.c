/* A fragment can shatter during a solver substep, before end-of-frame
 * compaction. Exercise the real private solver: the fatal contact counts,
 * but that fragment must not acquire another contact or reenter the hash. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../core/mx.c"

static MxWorld w;
static int failures;
#define CHECK(c,msg) do { if(!(c)){fprintf(stderr,"FAIL chunk lifetime: %s\n",msg);failures++;} } while(0)

static void scene(void){
  init_tables();
  memset(&w,0,sizeof w);
  w.state=MX_PLAY;w.bh_lo=0;w.bh_hi=MX_ROWS+3;
}

static MxChunk *body(int index,real x,real y,int material,real health,int rest){
  MxChunk *ch=&w.chunks[index];
  ch->active=1;ch->count=1;ch->x=x;ch->y=y;ch->rest=(uint8_t)rest;
  ch->rad=RI(1);ch->cs=RI(1);
  MxCell *a=&ch->cells[0];
  a->wx=x;a->wy=y;a->m=(uint8_t)material;a->h=health;a->edge=1;
  recompute(ch);
  return ch;
}

static void wall(int x,int y){
  w.grid[IDX(x,y)]=MX_STEEL;w.hp[IDX(x,y)]=RI(150);
}

static int events(int type){
  int n=0;for(int i=0;i<w.nev;i++)n+=w.ev[i].type==type;return n;
}

static int hash_entries(void){
  int n=0;
  for(unsigned b=0;b<sizeof w.bhead/sizeof *w.bhead;b++)
    for(int id=w.bhead[b];id>=0;id=w.bnext[id]){
      CHECK(!w.chunks[id/MX_CHUNK_CELLS].cells[id%MX_CHUNK_CELLS].dead,
            "a pre-shattered fragment entered the contact hash");
      n++;
    }
  return n;
}

static unsigned live_trace(void){
  scene();
  for(int x=15;x<28;x++)wall(x,24);
  for(int i=0;i<8;i++){
    MxChunk *ch=body(i,R(18.2)+rmul(RI(i%4),R(.85)),
                     R(22.1)+rmul(RI(i/4),R(.85)),
                     i&1?MX_WOOD:MX_STEEL,RI(10000),i==7);
    ch->vx=R(.4)*(i%3-1);ch->vy=R(.1)*(i%2);
  }
  for(int frame=0;frame<240;frame++){
    step_chunks(&w,R(1.0/240.0));
    for(int i=0;i<8;i++)CHECK(!w.chunks[i].cells[0].dead,
                             "nonfatal trace unexpectedly shattered a fragment");
  }
  unsigned hash=2166136261u;
  const unsigned char *bytes=(const unsigned char *)&w;
  for(size_t i=0;i<sizeof w;i++){hash^=bytes[i];hash*=16777619u;}
  return hash;
}

int main(void){
  /* This trace can also be run against the prior core to require exact
   * whole-world parity when every participating fragment remains alive. */
  if(getenv("MX_CONTACT_LIVE_TRACE")){
    printf("CHUNK_LIVE_TRACE %08x\n",live_trace());return failures!=0;
  }
  scene();
  MxChunk *a=body(0,R(20.8),R(20.8),MX_GLASS,R(.1),1);
  a->cells[0].dead=1;
  body(1,RI(40),RI(20),MX_STEEL,RI(150),0);
  step_chunks(&w,0);
  CHECK(hash_entries()==1,"hash must contain only the surviving fragment");

  /* The right wall is visited before the floor. A weak diagonal glass
   * cell shatters on that first wall and must not damage the floor too. */
  scene();wall(21,20);wall(20,21);
  a=body(0,R(20.8),R(20.8),MX_GLASS,R(.1),0);
  a->vx=a->vy=RI(50);
  step_chunks(&w,0);
  CHECK(a->cells[0].dead,"first static impact must shatter the fixture");
  CHECK(a->cn==1,"a shattered cell acquired another static contact");
  CHECK(w.hp[IDX(21,20)]<RI(150),"the fatal static contact must still damage its wall");
  CHECK(w.hp[IDX(20,21)]==RI(150),"shattered glass damaged a second static surface");
  CHECK(events(EV_CELL)==1,"one shattered fragment emitted repeated destruction events");
  step_chunks(&w,0);
  CHECK(hash_entries()==0&&a->cn==0,"shattered cell survived into the next substep");
  clean_chunks(&w,R(1.0/60.0));
  CHECK(!a->active&&w.score==5,"cleanup must retire and score the lost fragment once");
  clean_chunks(&w,R(1.0/60.0));
  CHECK(w.score==5,"repeated cleanup scored a fragment twice");

  /* Shattering on a wall must also end this cell's later body-pair scan. */
  scene();wall(21,20);
  a=body(0,R(20.8),R(20.8),MX_GLASS,R(.1),0);a->vx=RI(50);
  body(1,R(20.2),R(20.8),MX_STEEL,RI(150),1);
  step_chunks(&w,0);
  CHECK(a->cells[0].dead&&a->cn==1,"dead static fragment continued into body contacts");
  CHECK(a->vx<0,"dead fragment bounced again off an unrelated sleeping body");

  /* A first pair kills the source. Its still-live partner and the third
   * body are farther than one cell apart, so no legitimate chain contact
   * can move the third body during this substep. */
  scene();
  a=body(0,R(20.8),R(20.8),MX_GLASS,R(.1),0);a->vx=-RI(100);
  MxChunk *first=body(1,R(20.1),R(20.8),MX_STEEL,RI(150),0);
  MxChunk *third=body(2,R(21.5),R(20.8),MX_STEEL,RI(150),0);
  step_chunks(&w,0);
  CHECK(a->cells[0].dead&&a->cn==1,"pair-shattered source acquired another body contact");
  CHECK(first->vx<0&&first->cn==1,"the fatal pair contact lost its valid impulse/correction");
  CHECK(third->cn==0&&third->x==R(21.5)&&third->vx==0,
        "a dead source displaced an unrelated third body");

  /* Counterparts that die while already in a bucket must be skipped when
   * their own outer scan would start later in the same substep. */
  scene();
  a=body(0,R(20.1),R(20.8),MX_STEEL,RI(150),0);a->vx=RI(100);
  MxChunk *victim=body(1,R(20.8),R(20.8),MX_GLASS,R(.1),0);
  third=body(2,R(21.5),R(20.8),MX_STEEL,RI(150),0);
  step_chunks(&w,0);
  CHECK(victim->cells[0].dead,"the counterpart must shatter before its source scan");
  CHECK(third->cn==0&&third->x==R(21.5),"a killed counterpart started its own source scan");

  /* A dead cell does not end its whole body's scan. The surviving cell
   * still contacts its wall, and mass changes only at normal compaction. */
  scene();wall(21,20);wall(17,20);
  a=body(0,R(20.8),R(20.8),MX_GLASS,R(.1),0);a->vx=RI(50);
  a->count=2;
  a->cells[1].lx=R(-2.6);a->cells[1].m=MX_STEEL;
  a->cells[1].h=RI(150);a->cells[1].edge=1;
  recompute(a);
  real mass=a->m,inertia=a->I;
  step_chunks(&w,0);
  CHECK(a->cells[0].dead&&!a->cells[1].dead&&a->cn==2,
        "shattering one cell suppressed its surviving neighbor's contact");
  CHECK(a->count==2&&a->m==mass&&a->I==inertia,
        "contact retirement changed mass before end-of-frame compaction");
  clean_chunks(&w,R(1.0/60.0));
  CHECK(a->active&&a->count==1&&a->cells[0].m==MX_STEEL&&a->m==MATS[MX_STEEL].d,
        "compaction failed to retain and reweigh the surviving fragment");
  CHECK(w.score==5,"mixed live/dead body must score only its shattered cell");

  unsigned hash=live_trace();
  if(failures){fprintf(stderr,"CHUNK_CONTACT_LIFETIME_FAIL failures=%d\n",failures);return 1;}
  printf("CHUNK_CONTACT_LIFETIME_PASS dead_hash=1 static_stop=1 pair_stop=1 cleanup_once=1 live_trace=%08x\n",hash);
  return 0;
}
