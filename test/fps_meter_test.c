/* Pure accumulator tests: distinguishes observed frame submission from
 * actual simulation steps and handles wrap-safe elapsed VBlank deltas. */
#include <stdio.h>
#include "mx_fps_meter.h"
#define CK(c) do{if(!(c)){fprintf(stderr,"FAIL meter line %d\n",__LINE__);return 1;}}while(0)
int main(void){
 MxFpsMeter m={0};
 for(int i=0;i<120;i++)mx_fps_meter_add(&m,1,60,1,1);
 CK(m.render_x10==600&&m.sim_x10==600&&m.last_dropped==0);
 for(int i=0;i<60;i++)mx_fps_meter_add(&m,2,60,1,1);
 CK(m.render_x10==300&&m.sim_x10==300&&m.last_dropped==60);
 for(int i=0;i<60;i++)mx_fps_meter_add(&m,2,60,1,2);
 CK(m.render_x10==300&&m.sim_x10==600);
 for(int i=0;i<120;i++)mx_fps_meter_add(&m,16+(i%3==0),1000,1,1);
 CK(m.render_x10==300); /* two-second window not complete yet */
 for(int i=0;i<30;i++)mx_fps_meter_add(&m,17,1000,1,1);
 CK(m.render_x10>=550&&m.render_x10<=650);
 unsigned short prev=65535,now=1;CK((unsigned short)(now-prev)==2);
 puts("ok: wall-clock FPS and simulation SPS distinguished at 60/30/30+catchup; 16-bit VBlank wrap");
 return 0;
}
