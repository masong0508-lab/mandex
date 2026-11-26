#include <stdio.h>
#include <string.h>
#include "mx_options_menu.h"
#include "mx_save_journal.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"options line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 MxOptMenu m;MxOptions o,q;MxStats a,b;uint8_t data[MX_SAVE_MAX];int outfit=0;
 mx_options_default(&o);mx_meta_init(&a);mx_meta_init(&b);mx_opt_open(&m);
 for(int page=0;page<4;page++){mx_opt_page(&m,page);int n=mx_opt_rows(&m);for(int i=0;i<n;i++){CHECK(mx_opt_id(&m,i)>=0);CHECK(mx_opt_value(&o,&m,mx_opt_id(&m,i),1)!=NULL);mx_opt_move(&m,1);}CHECK(m.row==0);}
 mx_opt_page(&m,1);for(int i=0;i<4;i++){CHECK(o.profile==i);CHECK(mx_opt_activate(&o,&m,1,1)==MX_O_CHANGED);}CHECK(o.profile==0);
 CHECK(mx_opt_activate(&o,&m,-1,1)==MX_O_CHANGED&&o.profile==3&&mx_opt_fx_stride(&o)==4);
 mx_opt_page(&m,3);mx_opt_pick(&m,2);CHECK(mx_opt_activate(&o,&m,0,0)==MX_O_NONE&&m.confirm==0);CHECK(mx_opt_activate(&o,&m,1,1)==MX_O_NONE);
 CHECK(mx_opt_activate(&o,&m,0,1)==MX_O_NONE&&m.confirm);mx_opt_move(&m,-1);CHECK(!m.confirm);mx_opt_pick(&m,2);CHECK(mx_opt_activate(&o,&m,0,1)==MX_O_NONE);CHECK(mx_opt_activate(&o,&m,0,1)==MX_O_ERASE);
 for(int p=0;p<4;p++)for(int sh=0;sh<3;sh++)for(int b0=0;b0<32;b0++){o.profile=p;o.shake=sh;o.eco=b0&1;o.no_guide=(b0>>1)&1;o.music_off=(b0>>2)&1;o.sfx_off=(b0>>3)&1;o.monitor=(b0>>4)&1;o.collapse=1;o.tool=MX_TOOL_QUAKE;o.nofight=1;
 int n=mx_meta_pack_opt(&a,0,&o,data,sizeof data);CHECK(n==MX_SAVE_MAX);CHECK(mx_meta_unpack_opt(&b,&outfit,&q,data,n));CHECK(!memcmp(&o,&q,sizeof o));}
 mx_opt_page(&m,3);mx_opt_pick(&m,1);CHECK(mx_opt_activate(&o,&m,0,1)==MX_O_NONE);CHECK(mx_opt_activate(&o,&m,0,1)==MX_O_CHANGED);CHECK(!o.profile&&!o.eco&&!o.shake&&!o.no_guide&&!o.music_off&&!o.sfx_off&&!o.monitor);
 puts("NATIVE_OPTIONS_PASS 384 preference combinations, page wrapping, confirmation and reset");return 0;
}
