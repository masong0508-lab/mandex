/*
 * Minimal stand-in for libnds, ONLY so the real DS source files can be compiled and run on a
 * host machine for testing.  It models just enough of the API surface we use (including the
 * hardware's 6 PSG + 2 noise channel limit) - it is not an emulator and says nothing about real
 * timing or about whether the true libnds signatures match; see PORTING.md.
 */
#ifndef STUB_NDS_H
#define STUB_NDS_H
#include <stdint.h>
#include <stdio.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
extern u16 BG_PALETTE[256], BG_PALETTE_SUB[256];
extern u8 BG_GFX[65536], BG_GFX_SUB[65536];
#define RGB15(r,g,b) ((r)|((g)<<5)|((b)<<10))
typedef struct { int px, py; } touchPosition;
enum { KEY_A = 1 << 0, KEY_B = 1 << 1, KEY_SELECT = 1 << 2, KEY_START = 1 << 3, KEY_RIGHT = 1 << 4, KEY_LEFT = 1 << 5,
       KEY_UP = 1 << 6, KEY_DOWN = 1 << 7, KEY_R = 1 << 8, KEY_L = 1 << 9, KEY_X = 1 << 10, KEY_Y = 1 << 11, KEY_TOUCH = 1 << 12 };
enum { MODE_5_2D = 5, VRAM_A_MAIN_BG = 1, VRAM_C_SUB_BG = 4, BgType_Bmp8 = 1, BgSize_B8_256x256 = 2, DutyCycle_50 = 4 };
void lcdMainOnBottom(void); void videoSetMode(int); void videoSetModeSub(int); void vramSetBankA(int); void vramSetBankC(int);
int bgInit(int, int, int, int, int); int bgInitSub(int, int, int, int, int);
void soundEnable(void); void scanKeys(void); int keysDown(void), keysHeld(void), keysUp(void);
void touchRead(touchPosition *);
int soundPlayPSG(int, u16, u8, u8); int soundPlayNoise(u16, u8, u8); void soundKill(int); void soundSetFreq(int, int);
void swiWaitForVBlank(void); void dmaCopy(const void *, void *, u32); void DC_FlushRange(const void *, u32);
void cpuStartTiming(int); u32 cpuEndTiming(void);
/* libnds 20.12 helpers as documented: mulf32 = (a*b)>>12, divf32 = (a<<12)/b, sqrtf32 = sqrt in 20.12.
   (sqrtf32 is renamed because glibc's <math.h> also declares a function of that name.) */
#define sqrtf32 stub_sqrtf32
static inline int32_t mulf32(int32_t a, int32_t b){ return (int32_t)(((int64_t)a * b) >> 12); }
static inline int32_t divf32(int32_t a, int32_t b){ return (int32_t)(((int64_t)a << 12) / b); }
static inline int32_t sqrtf32(int32_t a){
  uint64_t v = (uint64_t)(uint32_t)a << 12, r = 0, b = 1ULL << 62;
  while(b > v) b >>= 2;
  while(b){ if(v >= r + b){ v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
  return (int32_t)r;
}
#endif
