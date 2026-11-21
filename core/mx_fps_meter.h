/* Portable observed-FPS accumulator.  "Rendered" means completed display
 * submissions; "simulated" means fixed 1/60 s physics steps, not game seconds.
 * The caller supplies real elapsed video ticks (DS VBlank) or milliseconds.
 * In particular, a port that renders at 30 Hz but updates once per frame
 * correctly reports BOTH 30 rendered FPS and 30 simulation steps/s. */
#ifndef MX_FPS_METER_H
#define MX_FPS_METER_H
#include <stdint.h>
#include <limits.h>
typedef struct {
  unsigned clock_ticks, renders, simulations, dropped_ticks;
  unsigned render_x10, sim_x10, last_dropped, max_gap_ticks;
} MxFpsMeter;
static inline void mx_fps_meter_add(MxFpsMeter *m, unsigned elapsed,
                                    unsigned ticks_per_second,
                                    unsigned draws, unsigned sim_steps){
  if(!ticks_per_second)return;
  /* Saturate long stalls without overflowing when the clock uses a high
   * tick rate.  Keep arithmetic accurate even for microsecond clocks. */
  const unsigned cap = ticks_per_second > UINT_MAX/5u
                       ? UINT_MAX : ticks_per_second*5u;
  if(elapsed>cap)elapsed=cap;
  m->clock_ticks+=elapsed;
  m->renders+=draws;
  m->simulations+=sim_steps;
  if(elapsed>m->max_gap_ticks)m->max_gap_ticks=elapsed;
  if(elapsed>1u && ticks_per_second==60u)m->dropped_ticks+=elapsed-1;
  if(m->clock_ticks>=ticks_per_second*2u){
    /* Multiplication in unsigned is unsafe for high resolution clocks.
     * This is executed only once per reporting window, never per pixel. */
    m->render_x10=(unsigned)(((uint64_t)m->renders*ticks_per_second*10u)/m->clock_ticks);
    m->sim_x10=(unsigned)(((uint64_t)m->simulations*ticks_per_second*10u)/m->clock_ticks);
    m->last_dropped=m->dropped_ticks;
    m->clock_ticks=0;m->renders=0;m->simulations=0;m->dropped_ticks=0;
    m->max_gap_ticks=0;
  }
}
/* When the ARM9 misses a VBlank, the DS may submit 30 visible frames while
 * only simulating 30 fixed 60-Hz steps. Run a bounded number of actual fixed
 * steps only if the last physics update was cheap enough. One long stall should
 * never trigger a huge backlog (which would itself stall rendering). */
static inline unsigned mx_fps_catchup_steps(unsigned vblank_gap,
                                            unsigned prior_phys_ticks){
  if(vblank_gap<2u || vblank_gap>4u || prior_phys_ticks>=200000u)return 1u;
  if(vblank_gap>=3u && prior_phys_ticks<95000u)return 3u;
  return 2u;
}
#endif
