/* mx_stars_for: 3 stars with two or more men left, 2 with one, otherwise 1. Shared by DS, 3DS and Dreamcast. */
#include <stdio.h>
#include "mx.h"
#include "mx_meta.h"
static int fails;
#define CHECK(c, msg) do{ if(!(c)){ printf("FAIL: %s\n", msg); fails++; } else printf("ok: %s\n", msg); }while(0)
int main(void){
  static MxWorld W;
  mx_load(&W, 0);
  W.men = 3; CHECK(mx_stars_for(&W) == 3, "3 men left = 3 stars");
  W.men = 2; CHECK(mx_stars_for(&W) == 3, "2 men left = 3 stars");
  W.men = 1; CHECK(mx_stars_for(&W) == 2, "1 man left = 2 stars");
  W.men = 0; CHECK(mx_stars_for(&W) == 1, "0 men left = 1 star");
  { MxStats S; mx_meta_init(&S); W.level = 4;
    W.men = 1; mx_meta_win(&S, &W); CHECK(S.stars[4] == 2, "mx_meta_win records the helper's stars");
    W.men = 0; mx_meta_win(&S, &W); CHECK(S.stars[4] == 2, "a worse win never lowers best stars");
    W.men = 2; mx_meta_win(&S, &W); CHECK(S.stars[4] == 3, "a better win raises best stars"); }
  printf(fails ? "stars_test: %d FAILED\n" : "stars_test: all passed\n", fails);
  return fails != 0;
}
