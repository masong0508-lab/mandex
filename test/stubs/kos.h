/* Minimal stand-in for KallistiOS, ONLY to syntax-check dreamcast/main.c on a host compiler. */
#ifndef STUB_KOS_H
#define STUB_KOS_H
#include <stdint.h>
typedef uint32_t uint32;
#define KOS_INIT_FLAGS(x)
#define INIT_DEFAULT 0
typedef struct { int dummy; } pvr_poly_hdr_t;
typedef struct { struct { int culling; } gen; } pvr_poly_cxt_t;
typedef struct { uint32 flags; float x, y, z, u, v; uint32 argb, oargb; } pvr_vertex_t;
enum { PVR_CMD_VERTEX = 0xe0000000, PVR_CMD_VERTEX_EOL = 0xf0000000, PVR_LIST_TR_POLY = 2, PVR_CULLING_NONE = 0 };
void pvr_init_defaults(void); void pvr_poly_cxt_col(pvr_poly_cxt_t*,int); void pvr_poly_compile(pvr_poly_hdr_t*,pvr_poly_cxt_t*);
int pvr_prim(void*,int); void pvr_wait_ready(void); void pvr_scene_begin(void); void pvr_list_begin(int); void pvr_list_finish(void); void pvr_scene_finish(void);
typedef struct { int d; } maple_device_t; typedef struct { uint32 buttons; int joyx, joyy; } cont_state_t;
enum { MAPLE_FUNC_CONTROLLER = 1, CONT_A = 4, CONT_START = 8, CONT_Y = 512 };
maple_device_t *maple_enum_type(int,int); void *maple_dev_status(maple_device_t*);
#endif
