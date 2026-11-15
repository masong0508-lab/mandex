/* The theme shared by every port: the DS plays it on two PCM channels (ds/source/sfx.c), the 3DS and Dreamcast play a pre-rendered loop of it
   (core/mx_snd.c: mx_music_render), the web game plays the same notes with WebAudio. */
#ifndef MX_MUS_TAB_H
#define MX_MUS_TAB_H
#define MUS_STEP_FRAMES 12              /* 60ths of a second per step (an eighth note) */
static const unsigned short NF[] = { 220, 233, 247, 262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831, 880 };
#define MUS_BARS 8
static const signed char TUNE[MUS_BARS * 8] = {
  12, -1, 15, -1, 19, -1, 17, 15,   17, -1, 15, -1, 12, -1, -1, -1,
  15, -1, 19, -1, 22, -1, 19, 15,   17, -1, 14, -1, 10, -1, -1, -1,
  12, -1, 15, -1, 19, -1, 20, 19,   17, -1, 20, -1, 24, -1, 20, 17,
  22, -1, 19, -1, 17, -1, 14, -1,   12, -1, 15, -1, 12, -1, -1, -1 };
static const unsigned short BASS[MUS_BARS][2] = { {110,165}, {87,131}, {131,196}, {98,147}, {110,165}, {87,131}, {98,147}, {110,165} };

#endif
