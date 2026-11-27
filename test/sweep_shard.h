#ifndef MX_SWEEP_SHARD_H
#define MX_SWEEP_SHARD_H
#include <stdlib.h>
typedef struct { int index, count; } SweepShard;
/* Assign level L to L % count. This retains the exact original seeds and
 * spreads neighboring (often similarly expensive) levels across runners. */
static inline int sweep_shard_parse(const char *index, const char *count, int levels, SweepShard *s) {
  char *ie, *ce;
  long i=strtol(index,&ie,10), c=strtol(count,&ce,10);
  if (ie==index || *ie || ce==count || *ce || c<1 || c>levels || i<0 || i>=c) return 0;
  s->index=(int)i; s->count=(int)c;
  return 1;
}
#endif
