/* The DS cutscene player (ds/source/intro.c) and the DS drawing library (ds/source/gfx.c) are shared with the 3DS build.  They only need the basic integer types
   from <nds.h>, so on the 3DS this stand-in supplies them (and nothing else). */
#ifndef MX_3DS_NDS_SHIM_H
#define MX_3DS_NDS_SHIM_H
#include <stdint.h>
#ifndef u8
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
#endif
#ifndef RGB15
#define RGB15(r, g, b) ((r) | ((g) << 5) | ((b) << 10))
#endif
#endif
