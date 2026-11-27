#include <stdio.h>
#include <string.h>
#include "mx_trial_nav.h"
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while (0)
int main(void){
    MxStats s,before;
    memset(&s,0,sizeof s);
    int cases=0;
    for(int d=0;d<MX_TRIAL_COUNT/MX_TRIAL_DISTRICT_SIZE;d++){
        const int first=MX_TRIAL_START+d*MX_TRIAL_DISTRICT_SIZE;
        for(int k=0;k<MX_TRIAL_DISTRICT_SIZE;k++){
            memset(s.stars,3,sizeof s.stars);
            s.stars[first+k]=0;
            before=s;
            for(int selected=0;selected<MX_NUM_LEVELS;selected++){
                CHECK(mx_trial_next_unfinished(&s,selected)==first+k);
                cases++;
            }
            CHECK(!memcmp(&s,&before,sizeof s));
        }
    }
    memset(s.stars,0,sizeof s.stars);
    for(int d=0;d<MX_TRIAL_COUNT/MX_TRIAL_DISTRICT_SIZE;d++){
        const int first=MX_TRIAL_START+d*MX_TRIAL_DISTRICT_SIZE;
        CHECK(mx_trial_next_unfinished(&s,first+19)==first);
        for(int k=0;k<MX_TRIAL_DISTRICT_SIZE;k++)s.stars[first+k]=1;
        CHECK(mx_trial_next_unfinished(&s,first)==(first+MX_TRIAL_DISTRICT_SIZE<MX_NUM_LEVELS?first+MX_TRIAL_DISTRICT_SIZE:-1));
    }
    CHECK(mx_trial_next_unfinished(&s,-1)==-1);
    CHECK(mx_trial_next_unfinished(&s,MX_NUM_LEVELS)==-1);
    CHECK(mx_trial_next_unfinished(&s,0)==-1);
    s.stars[MX_TRIAL_START+2]=0;s.stars[MX_NUM_LEVELS-1]=0;
    CHECK(mx_trial_next_unfinished(&s,MX_NUM_LEVELS-3)==MX_NUM_LEVELS-1);
    CHECK(mx_trial_next_unfinished(&s,MX_NUM_LEVELS)==MX_TRIAL_START+2);
    CHECK(mx_trial_next_unfinished(&s,0)==MX_TRIAL_START+2);
    printf("TRIAL_NAV_PASS cases=%d all_clear=1 wrap=1 read_only=1\n",cases);
    return 0;
}
