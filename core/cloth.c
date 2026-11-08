/* MANDEX towel cloth: see cloth.h and docs/CLOTH.md.  Jakobsen-style Verlet with sqrt-free distance constraints. */
#include "cloth.h"

#define K(i, j) ((i) * CL_NY + (j))
#define HELD(k) ((k) < CL_NY)              /* the whole held edge (column 0) rides on the fist as a short bar */
#define ITER 6

/* the constants of docs/CLOTH.md, already multiplied by dt (= 1/60) or dt^2 where the step wants them */
#define RL      R(7.0)
#define RW      R(9.0)
#define GRAV2   R(0.116667)               /* 420 * dt^2 */
#define GUST2   R(0.105556)               /* 380 * dt^2 */
#define DAMP    R(0.992)
#define KN      R(0.12)
#define PH1     0.219911                  /* 2 pi 2.1 / 60 */
#define PH2     0.324631                  /* 2 pi 3.1 / 60 */
#define LAG     R(0.8)                    /* the gust runs 0.8 rad later at every column along the cloth: ripples travel down it */

void cloth_init(Cloth *c, real hx, real hy, real hz, int mode){
  c->mode = mode;
  real dir = hx < 0 ? -1 : 1;
  for(int i = 0; i < CL_NX; i++) for(int j = 0; j < CL_NY; j++){
    int k = K(i, j);
    real z = hz + ((k & 1) ? R(0.4) : R(-0.4));
    if(mode == 1){                                  /* a flag: droops away from the fist */
      c->x[k][0] = hx + (real)(dir * i * RL * 9 / 10);
      c->x[k][1] = hy + (j - 1) * RW + i * RL * 45 / 100;
    } else {                                        /* a towel: hangs straight down */
      c->x[k][0] = hx + (j - 1) * RW;
      c->x[k][1] = hy + i * RL;
    }
    c->x[k][2] = z;
    for(int a = 0; a < 3; a++) c->p[k][a] = c->x[k][a];
  }
  c->t = 0;
}

static void rel(Cloth *c, int a, int b, real L2){
  int pa = HELD(a), pb = HELD(b);
  if(pa && pb) return;
  real d[3], dd = 0;
  for(int k = 0; k < 3; k++){ d[k] = c->x[b][k] - c->x[a][k]; dd += rmul(d[k], d[k]); }
  real f = rdiv(L2, dd + L2) - R(0.5);
  if(pa){ for(int k = 0; k < 3; k++) c->x[b][k] += 2 * rmul(d[k], f); }
  else if(pb){ for(int k = 0; k < 3; k++) c->x[a][k] -= 2 * rmul(d[k], f); }
  else for(int k = 0; k < 3; k++){ real s = rmul(d[k], f); c->x[a][k] -= s; c->x[b][k] += s; }
}

void cloth_step(Cloth *c, real hx, real hy, real hz){
  /* 1. gust (an oscillating push that makes him look like he is waving it), one value per column because of the lag */
  real ph1 = (real)(c->t * R(PH1)), ph2 = (real)(c->t * R(PH2));
  real g[CL_NX][3];
  for(int i = 0; i < CL_NX; i++){
    real lag = LAG * i;
    g[i][0] = rmul(GUST2, rsin(ph1 - lag));
    g[i][1] = rmul(GUST2 / 4, rsin(ph2 + R(1.3) - lag)) + GRAV2;
    g[i][2] = rmul(GUST2, rcos(ph1 + R(0.6) - lag));
  }

  /* 2. normal drag per quad: only the velocity across the cloth is slowed, which is what makes it flutter instead of hang like a rope */
  real corr[CL_N][3];
  for(int k = 0; k < CL_N; k++) corr[k][0] = corr[k][1] = corr[k][2] = 0;
  for(int i = 0; i < CL_NX - 1; i++) for(int j = 0; j < CL_NY - 1; j++){
    int a = K(i, j), b = K(i + 1, j), cc = K(i, j + 1), d = K(i + 1, j + 1), q[4] = { a, b, cc, d };
    real u[3], e1[3], e2[3], n[3];
    for(int k = 0; k < 3; k++){
      e1[k] = c->x[b][k] - c->x[a][k]; e2[k] = c->x[cc][k] - c->x[a][k];
      u[k] = 0; for(int m = 0; m < 4; m++) u[k] += c->x[q[m]][k] - c->p[q[m]][k];
      u[k] /= 4;
    }
    n[0] = rmul(e1[1], e2[2]) - rmul(e1[2], e2[1]);
    n[1] = rmul(e1[2], e2[0]) - rmul(e1[0], e2[2]);
    n[2] = rmul(e1[0], e2[1]) - rmul(e1[1], e2[0]);
    real nn = rmul(n[0], n[0]) + rmul(n[1], n[1]) + rmul(n[2], n[2]), nu = rmul(n[0], u[0]) + rmul(n[1], u[1]) + rmul(n[2], u[2]);
    if(nn < R(0.01)) continue;
    real s = rmul(KN, rdiv(nu, nn + R(0.001))) / 4;
    for(int m = 0; m < 4; m++) for(int k = 0; k < 3; k++) corr[q[m]][k] += rmul(s, n[k]);
  }

  /* 3. Verlet (the held edge is not integrated) */
  for(int k = 0; k < CL_N; k++){
    if(HELD(k)) continue;
    const real *gk = g[k / CL_NY];
    for(int a = 0; a < 3; a++){
      real x = c->x[k][a], v = rmul(x - c->p[k][a], DAMP) - corr[k][a];
      c->p[k][a] = x;
      c->x[k][a] = x + v + gk[a];
    }
  }
  /* 4. the hand holds the edge: mode 1 = a short vertical bar (flag on a pole), mode 2 = a short horizontal bar (towel) */
  for(int k = 0; k < CL_NY; k++){
    for(int a = 0; a < 3; a++) c->p[k][a] = c->x[k][a];
    c->x[k][0] = hx + (c->mode == 2 ? (k - 1) * RW : 0);
    c->x[k][1] = hy + (c->mode == 1 ? (k - 1) * RW : 0);
    c->x[k][2] = hz;
  }

  /* 5. distance constraints, in this order each round: shear, across, along (length last, so it wins).  No bend pairs: they only make a plank. */
  static const real L2[3] = { R(49.0), R(81.0), R(130.0) };   /* RL^2, RW^2, RL^2+RW^2 */
  for(int it = 0; it < ITER; it++){
    for(int i = 0; i < CL_NX - 1; i++) for(int j = 0; j < CL_NY - 1; j++){
      rel(c, K(i, j), K(i + 1, j + 1), L2[2]); rel(c, K(i, j + 1), K(i + 1, j), L2[2]);
    }
    for(int i = 0; i < CL_NX; i++) for(int j = 0; j < CL_NY - 1; j++) rel(c, K(i, j), K(i, j + 1), L2[1]);
    for(int i = 0; i < CL_NX - 1; i++) for(int j = 0; j < CL_NY; j++) rel(c, K(i, j), K(i + 1, j), L2[0]);
  }
  c->t++;
}

int cloth_quads(const Cloth *c, ClothQuad *out){
  int n = 0;
  for(int i = 0; i < CL_NX - 1; i++) for(int j = 0; j < CL_NY - 1; j++){
    int q[4] = { K(i, j), K(i + 1, j), K(i, j + 1), K(i + 1, j + 1) };
    ClothQuad *o = &out[n++];
    real z = 0;
    for(int m = 0; m < 4; m++){ o->x[m] = c->x[q[m]][0]; o->y[m] = c->x[q[m]][1]; z += c->x[q[m]][2]; }
    o->z = z / 4;
    if(i == 0 && o->z > R(-0.5)) o->z = R(-0.5);      /* the quads on the held edge stay behind the fist (drawn before the man) */
    real e1[3], e2[3], nv[3];
    for(int k = 0; k < 3; k++){ e1[k] = c->x[q[1]][k] - c->x[q[0]][k]; e2[k] = c->x[q[2]][k] - c->x[q[0]][k]; }
    nv[0] = rmul(e1[1], e2[2]) - rmul(e1[2], e2[1]);
    nv[1] = rmul(e1[2], e2[0]) - rmul(e1[0], e2[2]);
    nv[2] = rmul(e1[0], e2[1]) - rmul(e1[1], e2[0]);
    real nn = rmul(nv[0], nv[0]) + rmul(nv[1], nv[1]) + rmul(nv[2], nv[2]);
    o->face = nv[2] < 0;
    /* shade: (n.L)^2 / (n.n L.L) with L = (-0.35, -0.5, 0.8) - light from the upper left, toward us - bucketed so no square root is needed */
    real nl = rmul(nv[0], R(-0.35)) + rmul(nv[1], R(-0.5)) + rmul(nv[2], R(0.8));
    real b2 = nn > R(0.01) ? rdiv(rmul(nl, nl), rmul(nn, R(1.0125))) : 0;
    o->shade = b2 > R(0.45) ? 2 : b2 > R(0.15) ? 1 : 0;
    o->stripe = (i == 1 || i == 4);
    o->edge = (unsigned char)((i == 0 ? 1 : 0) | (i == CL_NX - 2 ? 2 : 0) | (j == 0 ? 4 : 0) | (j == CL_NY - 2 ? 8 : 0));
  }
  for(int a = 1; a < n; a++){                                     /* insertion sort, far (small z) first */
    ClothQuad t = out[a]; int b = a - 1;
    while(b >= 0 && out[b].z > t.z){ out[b + 1] = out[b]; b--; }
    out[b + 1] = t;
  }
  return n;
}

void cloth_actor_tick(ClothActor *a, const short hand[3], int face){
  if(hand[0] == CL_NONE){ a->live = 0; return; }
  real tx = (real)(RI(face >= 0 ? hand[0] : -hand[0]) / 4), ty = RI(hand[1]) / 4, tz = RI(hand[2]) / 4;
  if(!a->live){ a->live = 1; a->hx = tx; a->hy = ty; a->hz = tz; cloth_init(&a->c, tx, ty, tz, hand[1] > -250 ? 2 : 1); }   /* y above -62.5: the back view's fist (-58), a towel; the spin's fist (-66..-69) flies it as a flag */
  a->hx += (tx - a->hx) / 2; a->hy += (ty - a->hy) / 2; a->hz += (tz - a->hz) / 2;
  /* the sprite's arm is baked and does not move, so the towel is waved inside the fist, with the same small circle the browser moves its arm by:
     (5 sin p, 2.5 cos p, 3 cos(p + 1.3)), p = 2 pi 2.3 T */
  real w = (real)(a->c.t * R(0.240858));
  cloth_step(&a->c, a->hx + 5 * rsin(w), a->hy + rmul(R(2.5), rcos(w)), a->hz + rmul(R(3.0), rcos(w + R(1.3))));
}

void cloth_actor_frame(ClothActor *a, int frame, const short hand[3], int face){
  if(frame == a->last) return;
  if(frame - a->last > 4 || frame < a->last) a->live = 0;           /* stale: it was not drawn for a while */
  a->last = frame;
  cloth_actor_tick(a, hand, face);
}
