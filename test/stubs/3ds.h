/* Host stand-in for libctru's <3ds.h>: ONLY what 3ds/main.c calls, so the real main loop can run on a PC under test/n3ds_render.c.
   It models no GPU, no timing and no real hid behaviour (see PORTING.md). */
#ifndef STUB_3DS_H
#define STUB_3DS_H
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
typedef struct { u16 px, py; } touchPosition;
enum { KEY_A = 1 << 0, KEY_B = 1 << 1, KEY_SELECT = 1 << 2, KEY_START = 1 << 3, KEY_DRIGHT = 1 << 4, KEY_DLEFT = 1 << 5, KEY_DUP = 1 << 6, KEY_DDOWN = 1 << 7,
       KEY_R = 1 << 8, KEY_L = 1 << 9, KEY_X = 1 << 10, KEY_Y = 1 << 11, KEY_TOUCH = 1 << 20,
       KEY_CPAD_UP = 1 << 24, KEY_CPAD_DOWN = 1 << 25, KEY_CPAD_LEFT = 1 << 26, KEY_CPAD_RIGHT = 1 << 27,
       KEY_UP = KEY_DUP | KEY_CPAD_UP, KEY_DOWN = KEY_DDOWN | KEY_CPAD_DOWN, KEY_LEFT = KEY_DLEFT | KEY_CPAD_LEFT, KEY_RIGHT = KEY_DRIGHT | KEY_CPAD_RIGHT };
typedef enum { GFX_TOP, GFX_BOTTOM } gfxScreen_t;
typedef enum { GFX_LEFT, GFX_RIGHT } gfx3dSide_t;
void gfxInitDefault(void); void gfxSetDoubleBuffering(gfxScreen_t, bool); u8 *gfxGetFramebuffer(gfxScreen_t, gfx3dSide_t, u16 *, u16 *);
void gfxSet3D(bool); void gfxFlushBuffers(void); void gfxSwapBuffers(void); void gspWaitForVBlank(void); void gfxExit(void);
bool aptMainLoop(void); void hidScanInput(void); u32 hidKeysDown(void); u32 hidKeysHeld(void); u32 hidKeysUp(void);
void hidTouchRead(touchPosition *); float osGet3DSliderState(void);
#endif
