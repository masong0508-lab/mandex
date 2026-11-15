/* MANDEX DS - opening cutscene (the origin story: Gerald Fitzgerald -> MANDEX).  Plays once per power-on before level 1.
   Top screen = the picture, bottom screen = the typed narration + SKIP.  Pure drawing into the two 8-bit buffers, no hardware access. */
#ifndef MX_INTRO_H
#define MX_INTRO_H
#include <nds.h>

#define INTRO_SCENES 8
#define WOLF_SCENES 8
#define END_SCENES 4
void intro_start(void);
void intro_start_end(int chapter);   /* 1 / 2: the closing cutscene after the last level of chapter 1 / 2 */
void intro_start_wolf(void);          /* chapter 3: the wolfman cutscene */
int  intro_mode(void);                /* 0 opening, 1 wolfman, 2 / 3 the closers of chapter 1 / 2 */
int  intro_count(void);               /* scenes in the cutscene now playing */
/* call once per frame while the intro is up.  `down` = keysDown(), (tx,ty) = the stylus.  Returns 1 when the intro is over (finished or skipped). */
int  intro_tick(int down, int tx, int ty);
void intro_draw(u8 *top, u8 *bot);
int  intro_scene(void);                 /* for the host test */
int  intro_text_rows(int scene);        /* wrapped rows of narration on the bottom screen (must fit) */

#endif
