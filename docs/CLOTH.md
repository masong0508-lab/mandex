# Towel physics (shared by the browser, DS and Dreamcast)

The towel he waves in the cape-taunt (and on the back-view wiggle) is a small real cloth, not a drawing.
The browser (`web/index.html`, JS: `class Cloth`) and the consoles (`core/cloth.c`) run the same recipe, so they behave the same.

## Space
- Design units (the units `drawSpin` uses). x right, y down, z toward the viewer. Origin = body centre.
- Drawing is orthographic: screen = (x, y), depth = z (bigger z = nearer).
- One fixed step = 1/60 s.

## Grid
- `NX = 7` columns along the length (i = 0 is the held edge), `NY = 3` rows across (j = 0..2). Particle k = i*NY + j.
- Rest lengths: along `RL = 7`, across `RW = 9`, shear (diagonals) `hypot(RL,RW)`. There are NO bend pairs (an i-to-i+2 pair at 2*RL just makes a stiff plank).
- The whole held edge (column 0, the three particles k = 0,1,2) is held on the fist as a short bar, every step:
  - mode 1 (the spin, he flies it like a flag on a pole): a vertical bar, `x = H + (0, (j-1)*RW, 0)`
  - mode 2 (the back view, he waves it like a towel): a horizontal bar, `x = H + ((j-1)*RW, 0, 0)`
  (With a single pinned particle the towel spun up into a thin, edge-on stick.)
- Start shape, z nudged by +0.4 if k is odd, else -0.4:
  - mode 1: `p(i,j) = H + (dir*i*RL*0.9, (j-1)*RW + i*RL*0.45, z)`, `dir = -1 if H.x < 0 else +1` (droops away from the fist)
  - mode 2: `p(i,j) = H + ((j-1)*RW, i*RL, z)` (hangs straight down)

## One step (in this order)
1. Gust, as an acceleration in units/s^2, with T = seconds since the towel started. It runs a little LATE at every column along the cloth (lag = 0.8 rad per column i),
   which makes ripples travel down it: `ax = G*sin(W1*T - 0.8*i)`, `ay = 0.25*G*sin(W2*T + 1.3 - 0.8*i)`, `az = G*cos(W1*T + 0.6 - 0.8*i)`;
   `G = 380`, `W1 = 2*pi*2.1`, `W2 = 2*pi*3.1`. Gravity `GRAV = 420` is added to ay.
2. Normal drag (this is what makes it flutter like cloth, not like a rope). For each of the 12 quads, with corners
   a=(i,j) b=(i+1,j) c=(i,j+1):  `n = (b-a) x (c-a)`;  `u` = mean of (x - px) over the four corners;
   `corr = KN * (n.u) / (n.n + 1e-3) * n`  (KN = 0.12);  every corner of the quad gets `corr * 0.25` taken off its own x - px.
   (Collect these first, then apply them together.)
3. Verlet for every free (not held) particle: `u = (x - px) * DAMP - corr_k` (DAMP = 0.992);  `px = x;  x += u + (gust + gravity) * dt^2`.  `dt = 1/60`.
4. The held particles: `px = x; x = H + bar offset`.
5. `ITER = 6` rounds of distance constraints. In each round, in this order: shear pairs (both diagonals of each quad), across pairs (j neighbours), along pairs (i neighbours; the length goes last so it wins).
   Each pair (a,b) with rest length L (no square root, Jakobsen): `d = b - a; f = L*L/(d.d + L*L) - 0.5; a -= d*f; b += d*f`.
   If both are held, skip the pair; if one is held it does not move and the other moves by `2*d*f`.

## The hand anchor H
- `H = (30*cos(th) + oU + shx, -69 + bob, -30*sin(th))` for the SPIN frames, with `oU = -hip*0.35` (th in radians, same as `drawSpin`), and (shx, bob) the sideways
  lean and crouch of the step-turn (`spinSway`). The back-view frames have `H = (38 - hip*0.35, -58, 0)`. The HTML exports all of them as `TOWEL_HAND`.
- Waving: the raised hand moves on a small circle `(5*sin(p), 2.5*cos(p), 3*cos(p+1.3))`, `p = 2*pi*2.3*T`. The browser moves the real arm (the elbow gets half);
  the consoles' sprites have a baked arm, so they add it to H inside the fist. The console anchor also follows the baked hand of the frame on screen: it eases to the
  new frame's hand, half the distance each tick, so a frame change is not a hard kick.
- The console tells mode 2 from mode 1 by the baked hand: y above -62.5 (the back view's -58) is mode 2.

## Drawing
- 12 quads, each depth sorted on its mean z; the front face (the one that faces +z when it hangs) is the towel pink, the back face a darker pink,
  both shaded by how squarely they face the light (upper left, front). Quads on columns i = 1 and i = 4 carry the darker cross stripe.
- Dark outline on the outer edge of the cloth only.
- The quads on the held edge (i = 0) never come in front of the fist.
- Consoles: quads with mean z < 0 (the i = 0 quads always) are drawn before the man sprite (behind him), the rest after. The browser draws the rest of the cloth in front of
  the arm and after the body.
- Colours: front (255,126,176), back (234,102,156), stripe front (255,84,146), stripe back (208,60,122); the consoles use three shade steps (x0.70, 0.86, 1.0).
