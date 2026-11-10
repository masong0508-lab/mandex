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
enum { C_PLAY_PSG = 1, C_PLAY_NOISE = 2, C_FREQ = 3, C_KILL = 4, C_WAVE_SRC = 5, C_PLAY_WAVE = 6, C_VOL = 7 };
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
int soundPlayWave(const signed char *cyc, u16 f, u8 v, u8 pan){   /* PCM channels 0..7 loop a 32-byte cycle: no square-wave buzz.  The ARM9 has no data cache here, so the ARM7 reads the table straight from RAM. */
  int c = grab(0, 7); if(c < 0) return -1;
  push((C_WAVE_SRC << 28) | ((u32)cyc & 0xFFFFFC));   /* main RAM is 0x02xxxxxx: only the low 24 bits travel */
  push((C_FREQ << 28) | (c << 24) | f);               /* timer first, then start the channel */
  push((C_PLAY_WAVE << 28) | (c << 24) | ((u32)(v & 127) << 14) | ((u32)(pan & 127) << 7));
  return c;
}
void soundSetVol(int h, u8 v){ if(h >= 0 && h < 8) push((C_VOL << 28) | (h << 24) | (v & 127)); }
void soundSetFreq(int h, u16 f){ if(h >= 0 && h < 16) push((C_FREQ << 28) | (h << 24) | f); }
void soundKill(int h){ if(h >= 0 && h < 16){ push((C_KILL << 28) | (h << 24)); busy[h] = 0; } }

/* ------------------------------------------------------------- cartridge backup chip (8 KB / 64 kbit EEPROM, 2-byte address, 32-byte pages)
   Register values are from memory of GBATEK ("DS Cartridge Backup") and libnds' cardEeprom code: check them on the first emulator run.
   AUXSPICNT 0x040001A0: bit 0-1 baud, bit 6 chip-select hold, bit 7 busy, bit 13 SPI mode, bit 15 enable.   AUXSPIDATA 0x040001A2.
   EXMEMCNT 0x04000204 bit 11: card slot owner (0 = ARM9).  The ARM7's touch/firmware SPI is a different bus and is not touched. */
#define CARD_CNT  R16(0x040001A0)
#define CARD_DATA R16(0x040001A2)
static u8 card_xfer(u8 d, int hold){
  CARD_CNT = 0xA000 | (hold ? 0x40 : 0);           /* enable + SPI mode, 4 MHz */
  CARD_DATA = d;
  while(CARD_CNT & 0x80) ;
  return (u8)CARD_DATA;
}
static void card_begin(void){ R16(0x04000204) &= (u16)~0x0800; }     /* the card slot belongs to the ARM9 */
static void card_end(void){ CARD_CNT = 0; }
static u8 card_status(void){ card_xfer(0x05, 1); u8 s = card_xfer(0, 0); return s; }
static int card_wait_idle(void){                    /* 1 once the write-in-progress bit clears; 0 on timeout or a missing chip */
  for(int i = 0; i < 400000; i++){ u8 s = card_status(); if(s == 0xFF) return 0; if(!(s & 1)) return 1; }
  return 0;
}
int card_save_read(u8 *dst, int len){
  if(len <= 0 || len > 8192) return 0;
  card_begin();
  u8 st = card_status();
  if(st == 0xFF){ card_end(); return 0; }           /* nothing answering on the bus */
  card_xfer(0x03, 1); card_xfer(0, 1); card_xfer(0, 1);     /* READ, address 0 */
  for(int i = 0; i < len; i++) dst[i] = card_xfer(0, i < len - 1);
  card_end();
  return 1;
}
int card_save_write(const u8 *src, int len){
  if(len <= 0 || len > 8192) return 0;
  card_begin();
  if(!card_wait_idle()){ card_end(); return 0; }
  for(int a = 0; a < len; ){
    int n = 32 - (a & 31); if(n > len - a) n = len - a;      /* never cross a 32-byte page */
    card_xfer(0x06, 0);                                       /* write enable */
    if(!(card_status() & 2)){ card_end(); return 0; }         /* WEL did not latch: no writable EEPROM (FLASH, 512 B chip, or none) */
    card_xfer(0x02, 1); card_xfer((u8)(a >> 8), 1); card_xfer((u8)a, 1);
    for(int i = 0; i < n; i++) card_xfer(src[a + i], i < n - 1);
    if(!card_wait_idle()){ card_end(); return 0; }
    a += n;
  }
  card_end();
  return 1;
}
