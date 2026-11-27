#include <stdio.h>
#include <string.h>
#include "mx_trial_nav.h"
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while (0)
int main(void){
    MxStats s,before;memset(&s,0,sizeof s);
    CHECK(MX_NUM_LEVELS==1110&&!mx_ch4_unlocked(&s));
    s.unlock_all=1;CHECK(!mx_ch4_unlocked(&s));
    for(int i=MX_CH4_FIRST;i<MX_NUM_LEVELS;i++)CHECK(!mx_level_visible(&s,i));
    CHECK(mx_visible_level_step(&s,MX_CH4_PREREQ,1)==0);
    CHECK(mx_visible_level_step(&s,0,-1)==MX_CH4_PREREQ);
    CHECK(mx_menu_group_count(&s)==49);
    CHECK(mx_visible_district_step(&s,0,-1)==1000);
    int count=0,stars=0;CHECK(mx_menu_trial_summary(&s,1000,&stars,&count)==0&&count==10);
    s.stars[MX_CH4_PREREQ]=1;
    CHECK(mx_level_visible(&s,MX_CH4_GATE)&&!mx_level_visible(&s,MX_CH4_FIRST));
    CHECK(mx_visible_level_step(&s,MX_CH4_PREREQ,1)==MX_CH4_GATE);
    CHECK(mx_visible_district_step(&s,1000,1)==MX_CH4_GATE);
    CHECK(mx_menu_trial_summary(&s,MX_CH4_GATE,&stars,&count)==0&&count==1);
    CHECK(mx_campaign_next(&s,MX_CH4_PREREQ)==MX_CH4_GATE);
    for(int i=MX_TRIAL_START;i<MX_CH4_FIRST;i++)s.stars[i]=1;
    CHECK(mx_trial_next_unfinished(&s,0)==MX_CH4_GATE);
    s.stars[MX_CH4_GATE]=1;CHECK(mx_ch4_unlocked(&s)&&mx_campaign_next(&s,MX_CH4_GATE)==MX_CH4_FIRST);
    CHECK(mx_menu_group_count(&s)==54);
    CHECK(mx_visible_district_step(&s,1000,1)==MX_CH4_FIRST);
    CHECK(mx_visible_district_step(&s,0,-1)==1090);
    for(int i=MX_CH4_FIRST;i<MX_NUM_LEVELS;i++)CHECK(mx_level_visible(&s,i));
    int cases=0;
    for(int i=MX_TRIAL_START;i<MX_NUM_LEVELS;i++){
        memset(s.stars,3,sizeof s.stars);s.stars[i]=0;before=s;
        for(int selected=0;selected<MX_NUM_LEVELS;selected++){
            CHECK(mx_trial_next_unfinished(&s,selected)==i);cases++;
        }
        CHECK(!memcmp(&s,&before,sizeof s));
    }
    memset(s.stars,0,sizeof s.stars);s.stars[MX_CH4_GATE]=1;
    for(int g=1;g<mx_menu_group_count(&s);g++){
        const int first=mx_menu_group_first(&s,g);
        mx_menu_trial_summary(&s,first,&stars,&count);
        CHECK(count==((first==1000)?10:20));
        CHECK(mx_trial_next_unfinished(&s,first+count-1)==first);
        for(int k=0;k<count;k++)s.stars[first+k]=1;
    }
    CHECK(mx_trial_next_unfinished(&s,-1)==-1&&mx_trial_next_unfinished(&s,MX_NUM_LEVELS)==-1&&mx_trial_next_unfinished(&s,0)==-1);
    CHECK(mx_campaign_next(&s,MX_CH4_GATE)==MX_CH4_GATE+1);
    s.stars[MX_TRIAL_START+2]=0;s.stars[MX_NUM_LEVELS-1]=0;
    CHECK(mx_trial_next_unfinished(&s,MX_NUM_LEVELS-3)==MX_NUM_LEVELS-1);
    CHECK(mx_trial_next_unfinished(&s,MX_NUM_LEVELS)==MX_TRIAL_START+2);
    CHECK(mx_trial_next_unfinished(&s,0)==MX_TRIAL_START+2);
    printf("TRIAL_NAV_PASS cases=%d all_clear=1 wrap=1 read_only=1 secret_chapter=1\n",cases);
    return 0;
}
