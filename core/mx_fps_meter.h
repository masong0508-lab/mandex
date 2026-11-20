/* Portable observed-FPS accumulator.  "Rendered" means completed display
 * submissions; "simulated" means fixed 1/60 s physics steps, not game seconds.
 * The caller supplies real elapsed video ticks (DS VBlank) or milliseconds.
 * In particular, a port that renders at 30 Hz but updates once per frame
 * correctly reports BOTH 30 rendered FPS and 30 simulation steps/s. */
#ifndef MX_FPS_METER_H
#define MX_FPS_METER_H
typedef struct {
  unsigned clock_ticks, renders, simulations, dropped_ticks;
  unsigned render_x10, sim_x10, last_dropped, max_gap_ticks;
} MxFpsMeter;
static inline void mx_fps_meter_add(MxFpsMeter *m, unsigned elapsed,
                                    unsigned ticks_per_second,
                                    unsigned draws, unsigned sim_steps){
  if(!ticks_per_second)return;
  if(elapsed>ticks_per_second*5u)elapsed=ticks_per_second*5u;
  m->clock_ticks+=elapsed;
  m->renders+=draws;
  m->simulations+=sim_steps;
  if(elapsed>m->max_gap_ticks)m->max_gap_ticks=elapsed;
  if(elapsed>1u && ticks_per_second==60u)m->dropped_ticks+=elapsed-1;
  if(m->clock_ticks>=ticks_per_second*2u){
    m->render_x10=(m->renders*ticks_per_second*10u)/m->clock_ticks;
    m->sim_x10=(m->simulations*ticks_per_second*10u)/m->clock_ticks;
    m->last_dropped=m->dropped_ticks;
    m->clock_ticks=0;m->renders=0;m->simulations=0;m->dropped_ticks=0;
    m->max_gap_ticks=0;
  }
}
#endif
