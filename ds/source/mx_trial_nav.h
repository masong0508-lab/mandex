/* Shared native title navigation. No new save fields or physics. */
#ifndef MX_TRIAL_NAV_H
#define MX_TRIAL_NAV_H
#include "mx_meta.h"
#include <string.h>
static inline int mx_menu_regular_end(void){return mx_ch4_available()?MX_CH4_FIRST:MX_NUM_LEVELS;}
static inline int mx_menu_regular_groups(void){return (mx_menu_regular_end()-MX_TRIAL_START+MX_TRIAL_DISTRICT_SIZE-1)/MX_TRIAL_DISTRICT_SIZE;}
static inline int mx_menu_group_count(const MxStats *s){
    return 1+mx_menu_regular_groups()+(mx_ch4_unlocked(s)?5:mx_ch4_available()&&mx_level_visible(s,MX_CH4_GATE)?1:0);
}
static inline int mx_menu_group_index(const MxStats *s,int level){
    if(!mx_level_visible(s,level)||level<MX_TRIAL_START)return 0;
    if(level<mx_menu_regular_end())return 1+(level-MX_TRIAL_START)/MX_TRIAL_DISTRICT_SIZE;
    return 1+mx_menu_regular_groups()+(mx_ch4_unlocked(s)?(level-MX_CH4_FIRST)/MX_TRIAL_DISTRICT_SIZE:0);
}
static inline int mx_menu_group_first(const MxStats *s,int group){
    if(group<=0)return 0;
    if(group<=mx_menu_regular_groups())return MX_TRIAL_START+(group-1)*MX_TRIAL_DISTRICT_SIZE;
    return mx_ch4_unlocked(s)?MX_CH4_FIRST+(group-1-mx_menu_regular_groups())*MX_TRIAL_DISTRICT_SIZE:MX_CH4_GATE;
}
static inline int mx_visible_district_step(const MxStats *s,int level,int dir){
    const int n=mx_menu_group_count(s),g=mx_menu_group_index(s,level);
    return mx_menu_group_first(s,(g+(dir>0?1:n-1))%n);
}
static inline int mx_visible_level_step(const MxStats *s,int level,int dir){
    /* The visible catalogue is contiguous except for the one qualifier.
       Jump directly across the secret interval, even on a held D-pad. */
    if(level<0||level>=MX_NUM_LEVELS)level=0;
    const int l=(level+(dir>0?1:MX_NUM_LEVELS-1))%MX_NUM_LEVELS;
    if(!mx_ch4_available()||mx_ch4_unlocked(s)||l<MX_CH4_FIRST)return l;
    if(mx_level_visible(s,MX_CH4_GATE)){
        if(dir>0&&l<=MX_CH4_GATE)return MX_CH4_GATE;
        if(dir<=0&&l>=MX_CH4_GATE)return MX_CH4_GATE;
    }
    return dir>0?0:MX_CH4_FIRST-1;
}
static inline int mx_menu_secret_part(const MxStats *s,int level){
    return mx_ch4_unlocked(s)&&level>=MX_CH4_FIRST&&level<MX_NUM_LEVELS?
        1+(level-MX_CH4_FIRST)/MX_TRIAL_DISTRICT_SIZE:0;
}
static inline int mx_menu_part_number(const MxStats *s,int level){
    const int secret=mx_menu_secret_part(s,level);
    return secret?secret:mx_menu_group_index(s,level);
}
static inline int mx_menu_part_count(const MxStats *s,int level){
    return mx_menu_secret_part(s,level)?
        (MX_NUM_LEVELS-MX_CH4_FIRST+MX_TRIAL_DISTRICT_SIZE-1)/MX_TRIAL_DISTRICT_SIZE:
        mx_menu_regular_groups()+(mx_ch4_available()&&!mx_ch4_unlocked(s)&&mx_level_visible(s,MX_CH4_GATE)?1:0);
}
static inline int mx_menu_trial_summary(const MxStats *s,int level,int *stars,int *size){
    const int g=mx_menu_group_index(s,level);
    int clear=0,total=0,n=0;
    if(g){
        const int first=mx_menu_group_first(s,g);
        const int end=first<mx_menu_regular_end()?mx_menu_regular_end():mx_ch4_unlocked(s)?MX_NUM_LEVELS:first+1;
        n=end-first;if(n>MX_TRIAL_DISTRICT_SIZE)n=MX_TRIAL_DISTRICT_SIZE;
        for(int i=first;i<first+n;i++){int v=s->stars[i];if(v>3)v=3;total+=v;clear+=v>0;}
    }
    if(stars)*stars=total;if(size)*size=n;return clear;
}
/* Search only real visible ranges. Standard memchr may use word-sized
   comparisons, and never reads private sectors or mutates the save. */
static inline int mx_find_uncleared(const MxStats *s,int first,int end){
    if(first>=end)return -1;
    const uint8_t *p=(const uint8_t*)memchr(s->stars+first,0,(size_t)(end-first));
    return p?(int)(p-s->stars):-1;
}
static inline int mx_trial_next_unfinished(const MxStats *s,int level){
    int first=mx_menu_group_first(s,mx_menu_group_index(s,level));
    if(first<MX_TRIAL_START)first=MX_TRIAL_START;
    if(!mx_ch4_available()||mx_ch4_unlocked(s)){
        const int next=mx_find_uncleared(s,first,MX_NUM_LEVELS);
        return next>=0?next:mx_find_uncleared(s,MX_TRIAL_START,first);
    }
    const int gate=mx_level_visible(s,MX_CH4_GATE)&&!s->stars[MX_CH4_GATE];
    if(first==MX_CH4_GATE&&gate)return MX_CH4_GATE;
    const int next=mx_find_uncleared(s,first,MX_CH4_FIRST);
    if(next>=0)return next;
    if(gate)return MX_CH4_GATE;
    return mx_find_uncleared(s,MX_TRIAL_START,first<MX_CH4_FIRST?first:MX_CH4_FIRST);
}
#endif
