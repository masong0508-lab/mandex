/* Dumps every level (grid after mx_load, men, landlord cells) as text, so tools/level_parity.js can diff it against the web game cell by cell. */
#include <stdio.h>
#include <string.h>
#include "mx.h"
static MxWorld W;
int main(void){
  for(int l = 0; l < MX_NUM_LEVELS; l++){
    memset(&W, 0, sizeof W); mx_load(&W, l);
    printf("L %d men %d targets %d\n", l, W.men, W.ntargets);
    for(int i = 0; i < W.ntargets; i++) printf("T %d %d\n", (int)(W.targets[i].x * 2) , (int)(W.targets[i].y * 8));
    for(int y = 0; y < MX_ROWS; y++){ for(int x = 0; x < MX_COLS; x++) putchar('0' + W.grid[y * MX_COLS + x]); putchar('\n'); }
  }
  return 0;
}
