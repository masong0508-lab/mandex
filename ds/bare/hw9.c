/* ARM9 side of the bare-metal runtime; see nds.h */
#include "nds.h"

#define R16(a) (*(volatile u16 *)(a))
#define R32(a) (*(volatile u32 *)(a))

static u32 dispcnt_main, dispcnt_sub;

void videoSetMode(u32 m){ dispcnt_main = (dispcnt_main & 0x0800) | m; R32(0x04000000) = dispcnt_main; }
void videoSetModeSub(u32 m){ dispcnt_sub = (dispcnt_sub & 0x0800) | m; R32(0x04001000) = dispcnt_sub; }
void vramSetBankA(int v){ *(volatile u8 *)0x04000240 = (u8)v; }
void vramSetBankC(int v){ *(volatile u8 *)0x04000242 = (u8)v; }

static int bg_init(u32 base, u32 *dc, BgType t, BgSize s){
  R16(base + 0x0E) = (u16)(t | s);                 /* BG3CNT: 8-bit bitmap, 256x256, bitmap base 0 */
  R16(base + 0x30) = 256; R16(base + 0x32) = 0;    /* BG3 affine matrix = identity */
  R16(base + 0x34) = 0;   R16(base + 0x36) = 256;
  R32(base + 0x38) = 0;   R32(base + 0x3C) = 0;
  *dc |= 0x0800;                                   /* show BG3 */
  R32(base) = *dc;
  return 3;
}
int bgInit(int l, BgType t, BgSize s, int m, int ti){ (void)l; (void)m; (void)ti; return bg_init(0x04000000, &dispcnt_main, t, s); }
int bgInitSub(int l, BgType t, BgSize s, int m, int ti){ (void)l; (void)m; (void)ti; bg_init(0x04001000, &dispcnt_sub, t, s); return 7; }

void lcdMainOnBottom(void){ R16(0x04000304) = 0x0203; }   /* LCDs + both 2D engines on, bit 15 clear: engine A on the bottom screen */

void swiWaitForVBlank(void){
  while(R16(0x04000006) >= 192) ;                  /* VCOUNT: leave any vblank we are in ... */
  while(R16(0x04000006) < 192) ;                   /* ... and wait for the next one */
}

void dmaCopy(const void *src, void *dst, u32 bytes){
  const u16 *s = src; u16 *d = dst;
  for(u32 i = 0; i < bytes / 2; i++) d[i] = s[i];  /* VRAM wants 16-bit writes */
}

/* ------------------------------------------------------------- input */
static u32 cur, prev;
void scanKeys(void){
  prev = cur;
  u32 k = (~R16(0x04000130)) & 0x3FF;
  u32 b = MX_IPC->buttons;
  k |= (b & 3) << 10;                              /* X, Y */
  if(b & 4) k |= KEY_TOUCH;
  cur = k;
}
u32 keysHeld(void){ return cur; }
u32 keysDown(void){ return cur & ~prev; }
u32 keysUp(void){ return prev & ~cur; }
void touchRead(touchPosition *t){ t->px = MX_IPC->touch_x; t->py = MX_IPC->touch_y; t->rawx = t->rawy = t->z1 = t->z2 = 0; }

/* ------------------------------------------------------------- math (hardware divider / square-root unit) */
s32 divf32(s32 a, s32 b){
  *(volatile s64 *)0x04000290 = (s64)a << 12;       /* DIV_NUMER */
  *(volatile s64 *)0x04000298 = (s64)b;             /* DIV_DENOM */
  R16(0x04000280) = 1;                              /* DIVCNT: 64 / 32 -> 64 */
  while(R16(0x04000280) & 0x8000) ;
  return (s32)*(volatile s64 *)0x040002A0;
}
s32 sqrtf32(s32 a){
  if(a <= 0) return 0;
  *(volatile u64 *)0x040002B8 = (u64)(u32)a << 12;  /* SQRT_PARAM */
  R16(0x040002B0) = 1;                              /* SQRTCNT: 64-bit input */
  while(R16(0x040002B0) & 0x8000) ;
  return (s32)R32(0x040002B4);
}

/* two cascaded hardware timers = a 32-bit tick counter at 33.5 MHz */
void cpuStartTiming(int t){
  (void)t;
  R16(0x04000102) = 0; R16(0x04000106) = 0;
  R16(0x04000100) = 0; R16(0x04000104) = 0;
  R16(0x04000106) = 0x84;                           /* timer 1: enabled, count-up (cascade) */
  R16(0x04000102) = 0x80;                           /* timer 0: enabled, F/1 */
}
u32 cpuEndTiming(void){
  R16(0x04000102) = 0;
  return ((u32)R16(0x04000104) << 16) | R16(0x04000100);
}

/* ------------------------------------------------------------- sound (ARM7 does the register writes) */
enum { C_PLAY_PSG = 1, C_PLAY_NOISE = 2, C_FREQ = 3, C_KILL = 4 };
static u8 busy[16];
static void push(u32 cmd){
  MxIpc *p = MX_IPC;
  u32 h = p->head;
  if(((h + 1) & 63) == p->tail) return;             /* ring full: drop */
  p->ring[h & 63] = cmd;
  p->head = (h + 1) & 63;
}
static int grab(int lo, int hi){ for(int c = lo; c <= hi; c++) if(!busy[c]){ busy[c] = 1; return c; } return -1; }
void soundEnable(void){ MX_IPC->head = MX_IPC->tail = 0; }
int soundPlayPSG(DutyCycle d, u16 f, u8 v, u8 pan){
  int c = grab(8, 13); if(c < 0) return -1;
  push((C_FREQ << 28) | (c << 24) | f);               /* timer first, then start the channel */
  push((C_PLAY_PSG << 28) | (c << 24) | ((u32)(d & 7) << 21) | ((u32)(v & 127) << 14) | ((u32)(pan & 127) << 7));
  return c;
}
int soundPlayNoise(u16 f, u8 v, u8 pan){
  int c = grab(14, 15); if(c < 0) return -1;
  push((C_FREQ << 28) | (c << 24) | f);
  push((C_PLAY_NOISE << 28) | (c << 24) | ((u32)(v & 127) << 14) | ((u32)(pan & 127) << 7));
  return c;
}
void soundSetFreq(int h, u16 f){ if(h >= 8 && h < 16) push((C_FREQ << 28) | (h << 24) | f); }
void soundKill(int h){ if(h >= 8 && h < 16){ push((C_KILL << 28) | (h << 24)); busy[h] = 0; } }
