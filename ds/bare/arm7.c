/* ARM7: key X/Y + pen, touch screen (with firmware calibration) and the sound channels.  No libnds, no interrupts: polled once per frame. */
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef int s32;
#define R8(a)  (*(volatile u8 *)(a))
#define R16(a) (*(volatile u16 *)(a))
#define R32(a) (*(volatile u32 *)(a))

typedef struct {
  volatile u16 buttons, touch_x, touch_y, alive;
  volatile u32 head, tail;
  volatile u32 ring[64];
} Ipc;
#define IPC ((Ipc *)0x027FF000)

#define SPICNT  0x040001C0
#define SPIDATA 0x040001C2
#define SPI_ON   0x8000
#define SPI_CONT 0x0800
#define SPI_FW   0x0100
#define SPI_TS   0x0200
static void spi_wait(void){ while(R16(SPICNT) & 0x80) ; }
static u8 spi_xfer(u8 v, u32 ctl){ R16(SPICNT) = SPI_ON | ctl; R16(SPIDATA) = v; spi_wait(); return (u8)R16(SPIDATA); }

static u16 ts_read(u8 cmd){
  spi_xfer(cmd, SPI_TS | SPI_CONT | 1);
  u32 hi = spi_xfer(0, SPI_TS | SPI_CONT | 1);
  u32 lo = spi_xfer(0, SPI_TS | 1);                 /* last byte: release chip select */
  return (u16)(((hi << 8) | lo) >> 3) & 0xFFF;
}

/* calibration from the firmware user settings */
static int adc_x1 = 0x0200, adc_y1 = 0x0200, scr_x1 = 32, scr_y1 = 32;
static int adc_x2 = 0x0E00, adc_y2 = 0x0B00, scr_x2 = 224, scr_y2 = 160;
static void fw_read(u32 addr, u8 *dst, int n){
  spi_xfer(0x03, SPI_FW | SPI_CONT);
  spi_xfer(addr >> 16, SPI_FW | SPI_CONT); spi_xfer(addr >> 8, SPI_FW | SPI_CONT); spi_xfer(addr, SPI_FW | SPI_CONT);
  for(int i = 0; i < n; i++) dst[i] = spi_xfer(0, SPI_FW | (i == n - 1 ? 0 : SPI_CONT));
}
static void load_calibration(void){
  u8 h[2]; fw_read(0x20, h, 2);
  u32 base = ((u32)h[1] << 8 | h[0]) * 8;
  u8 s[0x70];
  fw_read(base, s, 0x70);                           /* first copy; the other is at +0x100, either is fine for a calibration */
  int ax1 = s[0x58] | s[0x59] << 8, ay1 = s[0x5A] | s[0x5B] << 8, sx1 = s[0x5C], sy1 = s[0x5D];
  int ax2 = s[0x5E] | s[0x5F] << 8, ay2 = s[0x60] | s[0x61] << 8, sx2 = s[0x62], sy2 = s[0x63];
  if(ax2 > ax1 && ay2 > ay1 && sx2 > sx1 && sy2 > sy1 && ax2 < 0x1000 && ay2 < 0x1000){
    adc_x1 = ax1; adc_y1 = ay1; scr_x1 = sx1; scr_y1 = sy1; adc_x2 = ax2; adc_y2 = ay2; scr_x2 = sx2; scr_y2 = sy2;
  }
}
static int clampi(int v, int lo, int hi){ return v < lo ? lo : v > hi ? hi : v; }

/* sound */
static u32 wave_src = 0x02000000u;                 /* where the looped 32-byte cycle for PCM channels lives (set by op 5) */
static void snd_cmd(u32 c){
  u32 op = c >> 28, ch = (c >> 24) & 15;
  u32 base = 0x04000400 + ch * 16;
  if(op == 3){                                      /* frequency */
    u32 f = c & 0xFFFF; if(f < 20) f = 20;
    if(ch < 8) f *= 32;                             /* PCM wave channels: 32 samples per cycle */
    else if(ch < 14) f *= 8;                        /* square wave has 8 samples per cycle */
    R16(base + 8) = (u16)(0x10000 - 33513982u / 2 / f);   /* timer reload: ARM7 sound clock = 16.756 MHz */
  } else if(op == 1){                               /* PSG */
    R32(base) = 0x80000000u | (3u << 29) | (((c >> 21) & 7) << 24) | ((c >> 14) & 127) | ((((c >> 7) & 127)) << 16);
  } else if(op == 2){                               /* noise */
    R32(base) = 0x80000000u | (3u << 29) | ((c >> 14) & 127) | ((((c >> 7) & 127)) << 16);
  } else if(op == 4){ R32(base) = 0;
  } else if(op == 5){ wave_src = 0x02000000u | (c & 0xFFFFFC);          /* remember the wave table address */
  } else if(op == 6 && ch < 8){                     /* PCM8 loop: SAD, PNT 0, LEN 8 words (32 bytes), repeat = loop forever */
    R32(base + 4) = wave_src; R16(base + 10) = 0; R32(base + 12) = 8;
    R32(base) = 0x80000000u | (1u << 27) | ((c >> 14) & 127) | ((((c >> 7) & 127)) << 16);
  } else if(op == 7 && ch < 8){ R8(base) = (u8)(c & 127);   /* volume only: a byte write cannot restart the channel */
  }
}

void arm7_main(void){
  R16(0x04000304) = 1;                              /* POWCNT2: speakers on */
  R16(0x04000500) = 0x807F;                         /* SOUNDCNT: enable, master volume 127 */
  R16(0x04000508) = 0x0200;                         /* SOUNDBIAS */
  load_calibration();
  u32 lastv = 0;
  for(;;){
    while(R16(0x04000006) != 200) ;                 /* VCOUNT: once per frame */
    u32 ex = R16(0x04000136);                       /* EXTKEYIN: active low, bit0 X, bit1 Y, bit6 pen */
    u32 b = 0;
    if(!(ex & 1)) b |= 1;
    if(!(ex & 2)) b |= 2;
    if(!(ex & 0x40)){
      u16 rx = ts_read(0xD1), ry = ts_read(0x91);
      if(rx > 0 && ry > 0 && rx < 0xFF0 && ry < 0xFF0){
        int px = (rx - adc_x1) * (scr_x2 - scr_x1) / (adc_x2 - adc_x1) + scr_x1 - 0;
        int py = (ry - adc_y1) * (scr_y2 - scr_y1) / (adc_y2 - adc_y1) + scr_y1 - 0;
        IPC->touch_x = (u16)clampi(px, 0, 255); IPC->touch_y = (u16)clampi(py, 0, 191);
        b |= 4;
      }
    }
    IPC->buttons = (u16)b;
    while(IPC->tail != IPC->head){ snd_cmd(IPC->ring[IPC->tail & 63]); IPC->tail = (IPC->tail + 1) & 63; }
    IPC->alive = (u16)(++lastv);
    while(R16(0x04000006) == 200) ;
  }
}
