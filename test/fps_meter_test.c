/* Pure accumulator tests: distinguishes observed frame submission from
 * actual simulation steps and handles wrap-safe elapsed VBlank deltas. */
#include <stdio.h>
#include "mx_fps_meter.h"
#define CK(c) do{if(!(c)){fprintf(stderr,"FAIL meter line %d\n",__LINE__);return 1;}}while(0)
int main(void){
 MxFpsMeter m={0};
 CK(mx_fps_catchup_steps(1,50000)==1);
 CK(mx_fps_catchup_steps(2,50000)==2);
 CK(mx_fps_catchup_steps(3,50000)==3);
 CK(mx_fps_catchup_steps(3,150000)==2);
 CK(mx_fps_catchup_steps(2,200000)==1);
 CK(mx_fps_catchup_steps(5,10000)==1);
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
 /* Microsecond clock: old 32-bit intermediate products wrap and report
  * nonsensical values even with ordinary 60 Hz rendering. */
 MxFpsMeter hi={0};
 for(int i=0;i<120;i++)mx_fps_meter_add(&hi,16667,1000000,1,1);
 CK(hi.render_x10>=599&&hi.render_x10<=600);
 CK(hi.sim_x10>=599&&hi.sim_x10<=600);
 /* A long stall is clipped to five seconds; reporting must remain finite. */
 MxFpsMeter stall={0};
 mx_fps_meter_add(&stall,UINT_MAX,1000000,1,1);
 CK(stall.render_x10==2&&stall.sim_x10==2);
 /* Saturation arithmetic must not wrap at a very high tick frequency. */
 MxFpsMeter huge={0};
 mx_fps_meter_add(&huge,UINT_MAX,1000000000u,1,1);
 CK(huge.render_x10==2&&huge.sim_x10==2);
 /* A fast timer's two-second window must not wrap at 2^32 ticks. */
 MxFpsMeter fast_clock={0};
 for(int i=0;i<3;i++)mx_fps_meter_add(&fast_clock,1000000000u,1000000000u,1,1);
 CK(fast_clock.render_x10==10&&fast_clock.sim_x10==10);
 CK(fast_clock.clock_ticks==1000000000u);
 /* Even the largest supported unsigned clock must close its window. */
 MxFpsMeter extreme_clock={0};
 mx_fps_meter_add(&extreme_clock,UINT_MAX,UINT_MAX,1,1);
 mx_fps_meter_add(&extreme_clock,UINT_MAX,UINT_MAX,1,1);
 CK(extreme_clock.render_x10==10&&extreme_clock.sim_x10==10);
 CK(extreme_clock.clock_ticks==0);
 unsigned short prev=65535,now=1;CK((unsigned short)(now-prev)==2);
 puts("ok: wall-clock FPS and simulation SPS distinguished at 60/30/30+catchup; 16-bit VBlank wrap");
 return 0;
}
