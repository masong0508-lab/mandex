/*
 * MANDEX - the towel he waves in the cape taunt and on the back-view wiggle: a small real cloth.
 * Same recipe as the browser version (see docs/CLOTH.md).  Header + one C file, no platform calls, float or 20.12 fixed point.
 *
 * Units are the man's design units (the HTML's): x right, y down, z toward the viewer, origin = body centre.
 * The front-ends convert to pixels with  px = design * 0.36 * (ppc / 4)  (the sprites are drawn at 0.36 px per unit at 4 px per cell).
 */
#ifndef MX_CLOTH_H
#define MX_CLOTH_H
#include "mx.h"

#define CL_NX 7                       /* columns along the length, i = 0 is the held edge */
#define CL_NY 3                       /* rows across */
#define CL_N  (CL_NX * CL_NY)
#define CL_NQ ((CL_NX - 1) * (CL_NY - 1))

typedef struct {
  real x[CL_N][3], p[CL_N][3];        /* position and previous position (Verlet) */
  int  t;                             /* ticks since it started: drives the gust */
  int  mode;                          /* 1 = held edge is a vertical bar (flag on a pole, the spin), 2 = horizontal bar (towel, the back view) */
} Cloth;

/* one drawable quad of the cloth, already sorted back to front by cloth_quads() */
typedef struct {
  real x[4], y[4];                    /* corners in design units: a=(i,j) b=(i+1,j) c=(i,j+1) d=(i+1,j+1) */
  real z;                             /* mean depth, bigger = nearer; < 0 means "behind the body" */
  unsigned char face;                 /* 1 = the front (pink) side is toward us, 0 = the back side */
  unsigned char shade;                /* 0 dark, 1 mid, 2 bright: how squarely it faces the light */
  unsigned char stripe;               /* on a darker cross stripe */
  unsigned char edge;                 /* bit0 = a-c is on the cloth's outer edge, bit1 = b-d, bit2 = a-b, bit3 = c-d */
} ClothQuad;

/* start shape under (mode 2) or drooping away from (mode 1) the hand anchor H */
void cloth_init(Cloth *c, real hx, real hy, real hz, int mode);
/* one fixed 1/60 s step with the hand anchor at H */
void cloth_step(Cloth *c, real hx, real hy, real hz);
/* fills out[CL_NQ] sorted far to near; returns CL_NQ */
int  cloth_quads(const Cloth *c, ClothQuad *out);

/* A towel that follows a man: it exists while the frame on screen carries one (hand[0] != CL_NONE), eases its anchor toward the
   baked hand of that frame (so a frame change is not a hard kick) and starts hanging when the towel first appears. */
#define CL_NONE 32767
typedef struct { Cloth c; real hx, hy, hz; int live, last; } ClothActor;
/* hand = the frame's baked hand in quarter design units {x,y,z} (MS_HAND in art/manspr.h), mirrored by `face` (+1 / -1) */
void cloth_actor_tick(ClothActor *a, const short hand[3], int face);
/* same, but safe to call from drawing code: ticks at most once per value of `frame` (the front-end's frame counter, so drawing the
   same man on two screens does not run it twice) and starts over if it was not called for a few frames */
void cloth_actor_frame(ClothActor *a, int frame, const short hand[3], int face);

#endif
