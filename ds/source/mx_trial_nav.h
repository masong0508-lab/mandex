/* Shared native title navigation. Read-only; no new save fields or physics. */
#ifndef MX_TRIAL_NAV_H
#define MX_TRIAL_NAV_H
#include "mx_meta.h"
/* Begin at the first trial of the selected district, then wrap through
   later districts once. A story selection starts at the first trial.
   Return -1 only when every trial is cleared. */
static inline int mx_trial_next_unfinished(const MxStats *s,int level){
    int first=MX_TRIAL_START;
    if(level>=MX_TRIAL_START&&level<MX_NUM_LEVELS)
        first+=((level-MX_TRIAL_START)/MX_TRIAL_DISTRICT_SIZE)*MX_TRIAL_DISTRICT_SIZE;
    for(int i=first;i<MX_NUM_LEVELS;i++)if(!s->stars[i])return i;
    for(int i=MX_TRIAL_START;i<first;i++)if(!s->stars[i])return i;
    return -1;
}
#endif
