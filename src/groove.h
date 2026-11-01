/* FOMAJAM: what the screen (main.c) and the sound (engine.c, drums.c, synths.c) share. */
#ifndef GROOVE_H
#define GROOVE_H
#include "fm1.h"

#define TRACKS 9 /* each plays one machine: a drum machine, or a synth engine */
#define STEPS 16
#define PATTERNS 40 /* as many as fit the settings */
#define SONG_VERSION 10

/* a step: what plays (a drum machine's voice, or a synth's MIDI note) and how */
#define STEP_ON 1
#define STEP_ACC 2
#define STEP_SLIDE 4 /* a synth: glide into the next step */
#define STEP_RAT 0x38 /* a ratchet: the step played RATCHET[(flag & STEP_RAT) >> 3] times across it */
#define RAT_SHIFT 3
static const uint8_t RATCHET[8] = {1, 2, 3, 4, 6, 8, 8, 8};
#define RATCHETS 6
#define GATE_TIE 101 /* GATE: a note held until the track's next one */

/* a track. Every knob value is 0..127 (64 = as the machine has it) unless said otherwise */
typedef struct {
  uint8_t machine, voice; /* voice: what new steps get (a drum voice, or a synth's note) */
  uint8_t level, send; /* send: to the delay */
  uint8_t tune, decay, tone, extra;     /* a drum machine's (a synth: TUNE) */
  uint8_t slide;                        /* a synth's glide */
  uint8_t rsend, csend, duck;           /* sends to the reverb and the chorus; how much track 1 ducks it */
  int8_t e[8];                          /* a synth engine's own parameters (synth_param) */
  uint8_t atk, dec, sus, rel;           /* its envelope */
  int8_t fenv;                          /* its envelope's sweep of the filter, -64..63 */
  uint8_t ftype, preset, mono;          /* ANALOG's filter type; the preset it came from; 1: one voice, legato */
  uint8_t mch, prog, gate, velo;        /* MIDI: the channel out (0..15), the program (0: none, else 1 + it), velocity;
                                         * GATE (a synth's and MIDI's): % of the step a note is held, GATE_TIE: to the next */
  uint8_t cc[4], ccn[4];                /* MIDI: four controllers' values and numbers */
} track_t;

typedef struct {
  uint8_t val[TRACKS][STEPS];  /* the voice or the note */
  uint8_t flag[TRACKS][STEPS]; /* STEP_* */
} pattern_t;

/* the whole song: it is the saved settings too */
typedef struct {
  uint8_t version, bpm, swing, length, theme, dtime, dfdbk, drive;
  uint16_t mute; /* bit t: track t is silent */
  uint8_t pattern, clock; /* clock: 1 sends MIDI clock (and start / stop) */
  track_t track[TRACKS];
  /* (added in version 7) */
  uint8_t rsize, rdamp;   /* the reverb */
  uint8_t crate, cdepth;  /* the chorus */
  uint8_t pump;           /* how fast a ducked track comes back */
  uint8_t filter, reso;   /* the master filter: 0..63 low-pass, 64 off, 65..127 high-pass */
  uint8_t dtype;          /* the distortion's kind (D_*); DRIVE is song.drive */
  uint8_t ccout;          /* 1: parameters changed here are sent out as MIDI CC (CC.md) */
  uint8_t lights;         /* the keys' backlight: 0 off, 1 low, 2 full (was padding: a saved song has 0) */
  pattern_t pat[PATTERNS]; /* here since version 8 (8 of them before, between track[] and rsize) */
  /* (added in version 9: a version 8 song loads without them) */
  uint8_t midiin;          /* MIDI in from: 0 all, MIDI_USB, MIDI_TRS, 3 Bluetooth (the fm1bt build) */
  uint8_t sync;            /* 1: follow MIDI clock in (and start / continue / stop) */
  uint8_t selch;           /* the channel that plays the selected track: 0 none, else 1 + it */
  uint8_t rxch[TRACKS];    /* the channel each track hears: 0 none, else 1 + it */
  /* (added in version 10: a version 9 song loads without them) */
  uint8_t btdev[6];        /* the Bluetooth MIDI device picked on the BLUETOOTH page (its address, least significant
                            * octet first; all 0: none): the FM-1 connects to it whenever it's around */
  uint8_t btrand;          /* its address is random */
  char btname[17];         /* its name, to find it again when its address changes (a Mac's, an iPhone's) */
} song_t;

_Static_assert(sizeof(song_t) <= SETTINGS_MAX, "the song must fit in the settings");
_Static_assert(sizeof(song_t) == 11938, "a version 10 song is 11938 bytes: a new field needs SONG_VERSION + 1");
#define SONG9_SIZE __builtin_offsetof(song_t, btdev)
#define SONG8_SIZE __builtin_offsetof(song_t, midiin)
/* the songs before: version 7 had 8 patterns after track[] and the effects after them; version 6 no effects */
#define SONG_HEAD __builtin_offsetof(song_t, rsize)               /* up to the patterns, in every version */
#define SONG_FX (__builtin_offsetof(song_t, pat) - SONG_HEAD)     /* rsize .. lights */
#define SONG6_SIZE (SONG_HEAD + 8 * sizeof(pattern_t))
#define SONG7_SIZE (SONG6_SIZE + SONG_FX)
_Static_assert(SONG7_SIZE == 2686, "a version 7 song was 2686 bytes");

extern song_t song; /* written by loop() only; audio() reads it */

/* ---- the engine (engine.c). Written by audio(): loop() starts and stops it with clock_start() / clock_stop() */
extern volatile uint8_t playing;
extern volatile uint8_t cur_pattern;   /* the pattern playing (song.pattern takes over when it ends) */
extern volatile int8_t play_step;      /* -1: stopped */
extern volatile uint16_t hits[TRACKS]; /* counts each sound started, for the screen */
extern volatile uint8_t cpu_load, cpu_peak; /* how much of each block's time audio() used, in % (FM-1 only) */
#define SCOPE_LEN 1024
extern int16_t scope[SCOPE_LEN];   /* the last of the output, for the screen's oscilloscope */
extern volatile uint32_t scope_at; /* where the next frame goes (it only counts up) */

void engine_tempo(void);             /* after the tempo or the delay time change */
void live_on(int t, int v, int vel); /* play track t now: a drum voice, or a synth note (held) */
void live_off(int t, int v);         /* let a synth note go */
void midi_out(int status, int d1, int d2); /* a message out over USB MIDI, sent by audio() (in order with the notes) */

/* ---- the machines: the modeled drums, the ROM drums, SMP (samples/) if there are any, the synth engines */
#define USER_KITS 4 /* the store's kits (fm1buddy's or the page's): machines after MIDI, named as they're made */
#define USER_MAX 27 /* samples in one, at most: one a key */
int machine_count(void);
int machine_kit(int m);   /* 1: a drum kit (the 22 voices, BD SD ...); else one voice a key (SMP, a kit so made) or notes */
int machine_empty(int m); /* 1: a store kit's slot with nothing in it (the knob passes it by) */
const char *machine_name(int m);
int machine_find(const char *name, int otherwise);
int machine_synth(int m);      /* 1: a synth engine (its voices are notes) */
int machine_engine(int m);     /* the synth engine (synths.c) it is, or -1 */
int machine_midi(int m);       /* 1: MIDI out (its voices are notes, sent, not heard) */
const char *machine_label(int m); /* 3 characters, for the grid */
int machine_voices(int m);     /* how many voice numbers it has (22, the samples, or 128 notes) */
int machine_has(int m, int v); /* 1: its own voice; 2: played by a neighbor; 0: silent */
const char *voice_name(int m, int v);
void drum_envelope(int t, int v, uint8_t *h, int n); /* the loudness over 1 s in n columns, 0..255 */

/* ---- the synth engines (synths.c, Felucca's): each has 8 parameters and presets */
typedef struct {
  const char *name;
  const char *const *names; /* the values' names, or 0 */
  int16_t lo, hi, def;
  uint8_t fmt; /* SF_* */
} synth_param_t;
enum { SF_NUM,
       SF_PCT,
       SF_SEMI,
       SF_ENUM };
int synth_count(void);
const char *synth_name(int e);
const char *synth_label(int e);
const char *synth_page(int e, int page); /* the titles of the engine's two pages of parameters */
void synth_param(int t, int k, synth_param_t *out); /* parameter k of track t's engine, as it has it set */
int synth_presets(int e);
const char *synth_preset_name(int e, int p);
void synth_load(int t, int p); /* preset p of track t's engine into song.track[t] */

/* between engine.c and drums.c / synths.c */
void synths_note(int t, int note, int vel, int slide, int32_t gate); /* gate: frames, -1 held */
void synths_off(int t, int note);
void synths_bend(int t, int bend, int depth); /* the wheels: bend in 1/16 semitones, vibrato depth 0..127 */
void synths_render(int t, int32_t *out, uint32_t n); /* track t, at its level; n: CTL (32) frames */
void synths_stop(void);
void synths_release(int t); /* track t's notes go (muted) */
void drums_store(void);   /* the store's bank (fm1.h) is the kits after MIDI: at power-on, and when it was rewritten */
void drums_unstore(void); /* it's about to be rewritten: they go quiet */
void drums_hit(int t, int v, int vel);
void drums_render(int t, int32_t *out, uint32_t n); /* track t's drums, at its level */

/* ---- the effects (fx.c), run by audio() */
enum { D_SOFT,
       D_HARD,
       D_TUBE,
       D_DIODE,
       D_FUZZ,
       D_FOLD,
       D_WAVE,
       D_RECT,
       D_BITS,
       D_RATE,
       D_CRUSH,
       DISTS };
extern const char *const DIST_NAME[DISTS];
void fx_reverb(const int32_t *in, int32_t *out, uint32_t n); /* adds what in sends it to out */
void fx_chorus(const int32_t *in, int32_t *out, uint32_t n);
void fx_master(int32_t *x, uint32_t n); /* the filter and the distortion, in place */
int32_t fx_curve(int32_t x);            /* the distortion as it is set, for the screen */

#endif
