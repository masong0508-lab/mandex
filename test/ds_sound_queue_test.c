/* Host regression for the REAL bare-metal ARM9 sound queue. On Linux, map
   the DS IPC address to anonymous RAM; no ARM7 hardware or emulator required.
   Verify backpressure never half-starts a voice or drops an essential kill. */
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/mman.h>
#include "../ds/bare/hw9.c"

#define CHECK(x,msg) do{ if(!(x)){fprintf(stderr,"DS sound queue: %s (line %d)\n",msg,__LINE__);return 1;} }while(0)
int main(void){
  void *mem = mmap((void *)MX_IPC_ADDR, 4096, PROT_READ|PROT_WRITE,
                   MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE, -1, 0);
  if(mem == MAP_FAILED){ perror("mmap DS IPC"); return 1; }
  soundEnable();

  /* Exactly two slots cannot admit a 3-command PCM start. No phantom voice. */
  MX_IPC->head = 61; MX_IPC->tail = 0;
  int h = soundPlayWave((const signed char *)0x02001000u, 440, 55, 64);
  CHECK(h == -1 && MX_IPC->head == 61 && !busy[0], "partial PCM voice admitted");
  MX_IPC->tail = 61;
  h = soundPlayWave((const signed char *)0x02001000u, 440, 55, 64);
  CHECK(h == 0 && MX_IPC->head == 0, "PCM start failed to wrap correctly");
  CHECK((MX_IPC->ring[61] >> 28) == C_WAVE_SRC &&
        (MX_IPC->ring[62] >> 28) == C_FREQ &&
        (MX_IPC->ring[63] >> 28) == C_PLAY_WAVE,
        "PCM source/frequency/start sequence interrupted");

  /* Fill to capacity and kill the PCM note. The kill must be retried before
     recycling channel zero; the ARM7 must never hear a start without a stop. */
  MX_IPC->tail = (MX_IPC->head + 1) & 63u;
  soundKill(h);
  CHECK(pending_kill == 1 && busy[0] && MX_IPC->head == 0, "kill lost at full queue");
  CHECK(soundPlayWave((const signed char *)0x02001000u, 330, 55, 64) < 0,
        "busy channel recycled before its kill");
  MX_IPC->tail = MX_IPC->head; /* ARM7 drains old commands */
  soundPump();
  CHECK(pending_kill == 0 && !busy[0] && (MX_IPC->ring[0] >> 28) == C_KILL,
        "pending kill not flushed first");
  h = soundPlayWave((const signed char *)0x02001000u, 330, 55, 64);
  CHECK(h == 0 && MX_IPC->head == 4 &&
        (MX_IPC->ring[3] >> 28) == C_PLAY_WAVE,
        "PCM channel not safely recyclable after kill");

  /* Gain quantization often repeats the previous frame's PCM volume.
     Coalescing those duplicates must not use ring slots. */
  MX_IPC->tail = MX_IPC->head;
  unsigned gain_start = MX_IPC->head;
  soundSetVol(0, 55); /* the freshly-started PCM voice already has gain 55 */
  CHECK(MX_IPC->head == gain_start, "initial PCM gain redundantly queued");
  soundSetVol(0, 22);
  CHECK(MX_IPC->head == ((gain_start + 1) & 63u) &&
        (MX_IPC->ring[gain_start] >> 28) == C_VOL,
        "first changed PCM gain not queued");
  gain_start = MX_IPC->head;
  soundSetVol(0, 22);
  CHECK(MX_IPC->head == gain_start, "identical PCM envelope gain queued twice");

  /* A dropped gain is not marked as queued; replay the latest gain after
     backpressure clears, or long notes could remain at stale loudness. */
  MX_IPC->tail = (MX_IPC->head + 4) & 63u;
  soundSetVol(0, 31);
  CHECK(MX_IPC->head == gain_start && pcm_gain[0] == 22,
        "dropped PCM gain incorrectly cached");
  MX_IPC->tail = MX_IPC->head;
  soundSetVol(0, 31);
  CHECK(MX_IPC->head == ((gain_start + 1) & 63u) && pcm_gain[0] == 31,
        "dropped PCM gain did not retry");
  soundSetVol(0, 31);
  CHECK(MX_IPC->head == ((gain_start + 1) & 63u),
        "retried PCM gain was queued twice");

  /* Pitch glides also spend many frames at the same integer frequency.
     Deduplicate already-queued pitches, but retry a pitch dropped for space. */
  unsigned freq_start = MX_IPC->head;
  soundSetFreq(0, 330); /* second wave's initial frequency */
  CHECK(MX_IPC->head == freq_start, "initial PCM pitch redundantly queued");
  soundSetFreq(0, 331);
  CHECK(MX_IPC->head == ((freq_start + 1) & 63u) &&
        (MX_IPC->ring[freq_start] >> 28) == C_FREQ,
        "changed PCM pitch not queued");
  freq_start = MX_IPC->head;
  soundSetFreq(0, 331);
  CHECK(MX_IPC->head == freq_start, "identical PCM pitch queued twice");
  MX_IPC->tail = (MX_IPC->head + 4) & 63u;
  soundSetFreq(0, 550);
  CHECK(MX_IPC->head == freq_start && queued_freq[0] == 331,
        "dropped PCM pitch incorrectly cached");
  MX_IPC->tail = MX_IPC->head;
  soundSetFreq(0, 550);
  CHECK(MX_IPC->head == ((freq_start + 1) & 63u) && queued_freq[0] == 550,
        "dropped PCM pitch not retried");

  /* A nearly-full queue drops cosmetic fades/glides to leave room for a
     necessary stop or a complete voice start. */
  MX_IPC->tail = (MX_IPC->head + 4) & 63u;  /* 3 slots free */
  unsigned before = MX_IPC->head;
  soundSetVol(0, 22); soundSetFreq(0, 220);
  CHECK(MX_IPC->head == before, "cosmetic commands consumed reserved room");
  soundKill(0);
  CHECK(MX_IPC->head == ((before + 1) & 63u) && !busy[0],
        "essential stop blocked by optional updates");

  /* PSG and noise starts are two-command transactions, not single writes. */
  soundEnable();
  MX_IPC->head = 62; MX_IPC->tail = 0;      /* one slot free */
  CHECK(soundPlayPSG(DutyCycle_50, 440, 50, 64) == -1 && !busy[8] &&
        soundPlayNoise(220, 50, 64) == -1 && !busy[14] &&
        MX_IPC->head == 62, "PSG/noise admitted half a start");
  MX_IPC->tail = 62;
  CHECK(soundPlayPSG(DutyCycle_50, 440, 50, 64) == 8 &&
        soundPlayNoise(220, 50, 64) == 14 &&
        (MX_IPC->ring[63] >> 28) == C_PLAY_PSG &&
        (MX_IPC->ring[1] >> 28) == C_PLAY_NOISE,
        "PSG/noise start commands not correctly ordered");

  munmap(mem,4096);
  puts("DS sound queue: atomic starts, backpressure, essential kill and recovery passed");
  return 0;
}
