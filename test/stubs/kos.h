/* Minimal stand-in for KallistiOS, to build and run dreamcast/main.c on a host compiler (a software rasteriser in dc_render.c stands in for the PVR). */
#ifndef STUB_KOS_H
#define STUB_KOS_H
#include <stdint.h>
#include <stdlib.h>
typedef uint32_t uint32;
typedef void *pvr_ptr_t;
#define KOS_INIT_FLAGS(x)
#define INIT_DEFAULT 0
typedef struct { int tex, fmt, w, h; void *ptr; int pad[3]; } pvr_poly_hdr_t;       /* (not the same size as a vertex, unlike the real thing: the stub tells them apart by size) */
typedef struct { struct { int culling; } gen; struct { int uv_clamp, fmt, w, h; void *ptr; int tex; } txr; } pvr_poly_cxt_t;
typedef struct { uint32 flags; float x, y, z, u, v; uint32 argb, oargb; } pvr_vertex_t;
enum { PVR_CMD_VERTEX = 0xe0000000, PVR_CMD_VERTEX_EOL = 0xf0000000, PVR_LIST_TR_POLY = 2, PVR_CULLING_NONE = 0 };
enum { PVR_TXRFMT_ARGB1555 = 0x00000000, PVR_TXRFMT_RGB565 = 0x08000000, PVR_TXRFMT_ARGB4444 = 0x10000000, PVR_TXRFMT_NONTWIDDLED = 0x04000000, PVR_FILTER_BILINEAR = 2, PVR_UVCLAMP_UV = 3 };
void pvr_init_defaults(void); void pvr_poly_cxt_col(pvr_poly_cxt_t*,int); void pvr_poly_compile(pvr_poly_hdr_t*,pvr_poly_cxt_t*);
void pvr_poly_cxt_txr(pvr_poly_cxt_t*, int list, int fmt, int w, int h, pvr_ptr_t ptr, int filter);
pvr_ptr_t pvr_mem_malloc(size_t n); void pvr_mem_free(pvr_ptr_t p); void pvr_txr_load(const void *src, pvr_ptr_t dst, uint32 n);
int pvr_prim(void*,int); void pvr_wait_ready(void); void pvr_scene_begin(void); void pvr_list_begin(int); void pvr_list_finish(void); void pvr_scene_finish(void);
typedef struct { int d; } maple_device_t; typedef struct { uint32 buttons; int joyx, joyy, ltrig, rtrig; } cont_state_t;
enum { MAPLE_FUNC_CONTROLLER = 1, CONT_A = 4, CONT_B = 2, CONT_START = 8, CONT_Y = 512, CONT_X = 1024, CONT_DPAD_LEFT = 128, CONT_DPAD_RIGHT = 64, CONT_DPAD_DOWN = 32 };
maple_device_t *maple_enum_type(int,int); void *maple_dev_status(maple_device_t*);
#endif
