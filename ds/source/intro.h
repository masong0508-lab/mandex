/* MANDEX DS - opening cutscene (the origin story: Gerald Fitzgerald -> MANDEX).  Plays once per power-on before level 1.
   Top screen = the picture, bottom screen = the typed narration + SKIP.  Pure drawing into the two 8-bit buffers, no hardware access. */
#ifndef MX_INTRO_H
#define MX_INTRO_H
#include <nds.h>

#define INTRO_SCENES 8
void intro_start(void);
/* call once per frame while the intro is up.  `down` = keysDown(), (tx,ty) = the stylus.  Returns 1 when the intro is over (finished or skipped). */
int  intro_tick(int down, int tx, int ty);
void intro_draw(u8 *top, u8 *bot);
int  intro_scene(void);                 /* for the host test */
int  intro_text_rows(int scene);        /* wrapped rows of narration on the bottom screen (must fit) */

#endif
