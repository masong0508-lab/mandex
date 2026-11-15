/* gcc -std=c99 -O2 -I../core -I../ds/source snd_test.c ../core/mx_snd.c -o snd_test && ./snd_test [dir]   (dir: also write each effect as a WAV) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mx_snd.h"
#include "sfx.h"
static short buf[MX_SND_MAX];
static void wav(const char *fn, int n){
  FILE *f = fopen(fn, "wb"); if(!f) return; unsigned d = n * 2, r = MX_SND_RATE, br = r * 2; unsigned short one = 1, two = 2, bits = 16; unsigned sz = 36 + d, fmt = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&sz, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f); fwrite(&one, 2, 1, f); fwrite(&one, 2, 1, f);
  fwrite(&r, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&two, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&d, 4, 1, f); fwrite(buf, 2, n, f); fclose(f);
}
int main(int argc, char **argv){
  int fails = 0;
  for(int id = 0; id < SFX_COUNT; id++){
    memset(buf, 0x55, sizeof buf);
    int n = mx_snd_render(id, buf, MX_SND_MAX);
    int peak = 0; double e = 0; int zc = 0;
    for(int i = 0; i < n; i++){ int a = abs(buf[i]); if(a > peak) peak = a; e += (double)buf[i] * buf[i]; if(i && (buf[i] < 0) != (buf[i-1] < 0)) zc++; }
    double rms = n ? sqrt(e / n) : 0;
    int tail = n > 4 ? abs(buf[n - 1]) : 0, head = n ? abs(buf[0]) : 0;
    int ok = n > 400 && n <= MX_SND_MAX && peak > 1500 && peak < 32000 && rms > 300 && tail < 400 && head < 400 && zc > 4 && buf[n] == 0x5555;
    printf("%s sfx %2d: %5d samples (%3d ms) peak %5d rms %5.0f crossings %4d\n", ok ? "ok:" : "FAIL:", id, n, n * 1000 / MX_SND_RATE, peak, rms, zc);
    if(!ok) fails++;
    if(argc > 1){ char fn[256]; snprintf(fn, sizeof fn, "%s/sfx%02d.wav", argv[1], id); wav(fn, n); }
  }
  printf("%d out of range -> %d\n", mx_snd_render(-1, buf, 10) + mx_snd_render(SFX_COUNT, buf, 10), 0);
  if(mx_snd_render(SFX_BOOM, buf, 100) != 100){ printf("FAIL: render must stop at max\n"); fails++; }
  { MxEvent e; memset(&e, 0, sizeof e); int o[2], n = 0, seen[SFX_COUNT] = { 0 };
    for(int t = 0; t < 40; t++){ e.type = t; e.a = R(1); e.b = R(1); n = mx_event_sfx(&e, o); if(n < 0 || n > 2){ printf("FAIL: event %d -> %d sounds\n", t, n); fails++; } for(int i = 0; i < n; i++){ if(o[i] < 0 || o[i] >= SFX_COUNT){ printf("FAIL: event %d -> bad id\n", t); fails++; } else seen[o[i]]++; } }
    int used = 0; for(int i = 0; i < SFX_COUNT; i++) used += seen[i] > 0;
    printf("%s events reach %d of %d effects\n", used >= 16 ? "ok:" : "FAIL:", used, SFX_COUNT); if(used < 16) fails++; }
  puts(fails ? "snd_test: FAILED" : "snd_test: all passed"); return fails != 0;
}
