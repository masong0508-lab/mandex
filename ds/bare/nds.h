/*
 * MANDEX bare-metal DS runtime: a small stand-in for <nds.h>, so the game can be built without libnds / calico
 * (their start-up code does not boot in DraStic and crashes melonDS here; classic homebrew with its own crt0 does).
 * Only what the game uses.  Hardware registers straight from GBATEK.  The ARM7 side is arm7.c.
 */
#ifndef MX_BARE_NDS_H
#define MX_BARE_NDS_H
#include <stdint.h>
#include <stddef.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t s8; typedef int16_t s16; typedef int32_t s32; typedef int64_t s64;
typedef volatile u16 vu16; typedef volatile u32 vu32;

#define RGB15(r, g, b) ((r) | ((g) << 5) | ((b) << 10))
#define BG_PALETTE      ((vu16 *)0x05000000)
#define BG_PALETTE_SUB  ((vu16 *)0x05000400)
#define BG_GFX          ((u16 *)0x06000000)
#define BG_GFX_SUB      ((u16 *)0x06200000)

/* keys (same bit layout libnds uses) */
#define KEY_A      (1 << 0)
#define KEY_B      (1 << 1)
#define KEY_SELECT (1 << 2)
#define KEY_START  (1 << 3)
#define KEY_RIGHT  (1 << 4)
#define KEY_LEFT   (1 << 5)
#define KEY_UP     (1 << 6)
#define KEY_DOWN   (1 << 7)
#define KEY_R      (1 << 8)
#define KEY_L      (1 << 9)
#define KEY_X      (1 << 10)
#define KEY_Y      (1 << 11)
#define KEY_TOUCH  (1 << 12)
typedef struct { u16 rawx, rawy, px, py, z1, z2; } touchPosition;

/* shared with the ARM7 (main RAM, mirrored at 0x027FF000 on both CPUs) */
#define MX_IPC_ADDR 0x027FF000u
typedef struct {
  volatile u16 buttons;        /* ARM7: bit0 X, bit1 Y, bit2 pen down (1 = pressed) */
  volatile u16 touch_x, touch_y;   /* ARM7: screen pixels */
  volatile u16 alive;          /* ARM7 increments every frame */
  volatile u32 head, tail;     /* sound command ring: ARM9 writes head, ARM7 writes tail */
  volatile u32 ring[64];
} MxIpc;
#define MX_IPC ((MxIpc *)MX_IPC_ADDR)

/* display */
#define MODE_5_2D 0x10005
#define VRAM_A_MAIN_BG 0x81
#define VRAM_C_SUB_BG  0x84
typedef enum { BgType_Bmp8 = 0x80 } BgType;
typedef enum { BgSize_B8_256x256 = 0x4000 } BgSize;
void videoSetMode(u32 mode);
void videoSetModeSub(u32 mode);
void vramSetBankA(int v);
void vramSetBankC(int v);
int bgInit(int layer, BgType t, BgSize s, int map, int tile);
int bgInitSub(int layer, BgType t, BgSize s, int map, int tile);
void lcdMainOnBottom(void);
void swiWaitForVBlank(void);
void dmaCopy(const void *src, void *dst, u32 bytes);

/* input */
void scanKeys(void);
u32 keysDown(void); u32 keysHeld(void); u32 keysUp(void);
void touchRead(touchPosition *t);

/* fixed-point math, 20.12 */
static inline s32 mulf32(s32 a, s32 b){ return (s32)(((s64)a * b) >> 12); }
s32 divf32(s32 a, s32 b);
s32 sqrtf32(s32 a);

/* sound: commands go to the ARM7 */
typedef enum { DutyCycle_50 = 3 } DutyCycle;
void soundEnable(void);
int soundPlayPSG(DutyCycle duty, u16 freq, u8 volume, u8 pan);
int soundPlayNoise(u16 freq, u8 volume, u8 pan);
void soundSetFreq(int handle, u16 freq);
void soundKill(int handle);
#endif
