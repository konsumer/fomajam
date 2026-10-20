/* FOMAJAM's sound: the step sequencer, MIDI out, the mix (sends and ducking), the delay and the output. The
 * drum machines are in drums.c, the synth engines in synths.c, and the other effects in fx.c. Everything runs in
 * 32-frame blocks (as Felucca's engines do), and steps start on a block boundary. This code runs in audio():
 * integers only, no screen, no flash.
 *
 * The steps follow the platform's MIDI clock (fm1.h clock_*): a step every 6 ticks, its own tempo or one coming in,
 * with start and stop. MIDI tracks' notes and whatever loop() queues with midi_out() go out from here, in order. */
#include "groove.h"

song_t song;
volatile uint8_t playing;
volatile uint8_t cur_pattern;
volatile int8_t play_step = -1;
volatile uint16_t hits[TRACKS];
volatile uint8_t cpu_load, cpu_peak;
int16_t scope[SCOPE_LEN];
volatile uint32_t scope_at;

static volatile uint32_t dlen = 11025; /* the delay, in frames, from engine_tempo() */

static int clamp(int v, int lo, int hi) {
  return v < lo ? lo : v > hi ? hi
                              : v;
}

static int is_engine(int t) { return machine_engine(song.track[t].machine) >= 0; }
static int is_midi(int t) { return machine_midi(song.track[t].machine); }

/* ---- requests from loop(): loop() queues them, audio() carries them out */
enum { EV_ON,
       EV_OFF,
       EV_MIDI };
#define QLEN 64
static struct {
  uint8_t kind, a, b, c;
} queue[QLEN];
static volatile uint8_t q_head, q_tail; /* loop() writes the head, audio() the tail */

static void push(int kind, int a, int b, int c) {
  uint8_t h = q_head, next = (h + 1) & (QLEN - 1);
  if (next == q_tail) {
    return; /* full: drop it */
  }
  queue[h].kind = (uint8_t)kind;
  queue[h].a = (uint8_t)a;
  queue[h].b = (uint8_t)b;
  queue[h].c = (uint8_t)c;
  q_head = next;
}

void live_on(int t, int v, int vel) { push(EV_ON, t, v, vel); }
void live_off(int t, int v) { push(EV_OFF, t, v, 0); }
void midi_out(int status, int d1, int d2) { push(EV_MIDI, status, d1, d2); }

/* ---- MIDI tracks: the note each has sounding, and the frames until its note-off */
static uint8_t m_note[TRACKS], m_on[TRACKS];
static int32_t m_left[TRACKS]; /* -1: held (played live) */

static void m_off(int t) {
  if (m_on[t]) {
    midi_send((uint8_t)(0x80 | (song.track[t].mch & 15)), m_note[t], 0);
    m_on[t] = 0;
  }
}

static void m_note_on(int t, int note, int vel, int legato, int32_t gate) {
  int ch = song.track[t].mch & 15, old = m_on[t], was = m_note[t];
  if (old && !legato) {
    m_off(t);
  }
  midi_send((uint8_t)(0x90 | ch), (uint8_t)note, (uint8_t)clamp(vel, 1, 127));
  if (old && legato && was != note) { /* a slide: the new note first, then the old one goes */
    midi_send((uint8_t)(0x80 | ch), (uint8_t)was, 0);
  }
  m_note[t] = (uint8_t)note;
  m_on[t] = 1;
  m_left[t] = gate;
  hits[t]++;
}

static void m_tick(uint32_t n) { /* the note-offs due in this block */
  for (int t = 0; t < TRACKS; t++) {
    if (m_on[t] && m_left[t] > 0 && (m_left[t] -= (int32_t)n) <= 0) {
      m_off(t);
    }
  }
}

/* ---- the clock: a 16th's frames at the tempo (the song's, or the clock's coming in) */
static uint32_t step_base(void) {
  uint32_t bpm100 = clock_bpm();
  return bpm100 >= 2000 ? 66150000u / bpm100 : 661500u / (song.bpm < 40 ? 40 : song.bpm); /* 44100 * 60 / 4 */
}

void engine_tempo(void) {
  uint32_t d = song.dtime * step_base();
  dlen = d >= 44100 ? 44099 : d;
}

/* ---- ratchets: a step's repeats, spread evenly across the step */
static struct {
  uint8_t left, val, vel;
  int32_t every, next;
} rat[TRACKS];

static void hit(int t, int val, int vel, int slide_in, int32_t gate) { /* play a step on whatever machine the track has */
  if (is_midi(t)) {
    m_note_on(t, val, vel, slide_in, gate);
  } else if (machine_synth(song.track[t].machine)) {
    synths_note(t, val, vel, slide_in, gate);
  } else {
    drums_hit(t, val, vel);
  }
}

static void rat_tick(uint32_t n) {
  for (int t = 0; t < TRACKS; t++) {
    if (!rat[t].left) {
      continue;
    }
    rat[t].next -= (int32_t)n;
    while (rat[t].left && rat[t].next <= 0) {
      hit(t, rat[t].val, rat[t].vel, 0, rat[t].every / 2);
      rat[t].left--;
      rat[t].next += rat[t].every;
    }
  }
}

static uint8_t tick6;    /* the clock's tick in the step: a step on every 6th */
static int32_t swing_in; /* a swung step waiting: the frames until it plays (-1: none) */

static void next_step(void) {
  int len = song.length < 1 ? 1 : song.length > STEPS ? STEPS
                                                      : song.length;
  int s = play_step + 1 >= len ? 0 : play_step + 1, prev = s ? s - 1 : len - 1;
  if (!s) {
    cur_pattern = song.pattern;
  }
  play_step = (int8_t)s;
  const pattern_t *p = &song.pat[cur_pattern];
  uint32_t base = step_base(), even = 2 * base * (200 + song.swing) / 400; /* swing 0..100: 50% .. 75% of the pair */
  uint32_t step_len = (s & 1) ? 2 * base - even : even;
  for (int t = 0; t < TRACKS; t++) {
    uint8_t f = p->flag[t][s];
    rat[t].left = 0;
    if (!(f & STEP_ON) || (song.mute >> t & 1)) {
      continue;
    }
    const track_t *d = &song.track[t];
    int r = RATCHET[(f & STEP_RAT) >> RAT_SHIFT], vel = f & STEP_ACC ? 127 : is_midi(t) ? d->velo
                                                                                        : 100;
    if (r > 1) { /* a ratchet: r hits across the step, the first now (no slide in a roll) */
      int32_t every = (int32_t)step_len / r;
      rat[t].left = (uint8_t)(r - 1);
      rat[t].val = p->val[t][s];
      rat[t].vel = (uint8_t)vel;
      rat[t].every = rat[t].next = every;
      hit(t, p->val[t][s], vel, 0, every / 2);
      continue;
    }
    uint8_t pf = p->flag[t][prev];
    int slide_in = (pf & STEP_ON) && (pf & STEP_SLIDE);
    /* GATE: how long a note is held; TIE: until the track's next (a synth: -2, its next note lets it go) */
    int32_t gate = (f & STEP_SLIDE)      ? (int32_t)step_len + 300
                   : d->gate >= GATE_TIE ? (is_midi(t) ? -1 : -2)
                                         : (int32_t)(step_len * (d->gate ? d->gate : 1) / 100u);
    hit(t, p->val[t][s], vel, slide_in, gate);
  }
}

/* a tick while playing: every 6th starts a step, an odd one late by the swing */
static void tick(void) {
  if (!tick6) {
    int len = song.length < 1 ? 1 : song.length > STEPS ? STEPS
                                                        : song.length;
    int s = play_step + 1 >= len ? 0 : play_step + 1;
    swing_in = (s & 1) ? (int32_t)(step_base() * song.swing / 200) : 0; /* (the pair's share past half) */
  }
  tick6 = (uint8_t)((tick6 + 1) % 6);
}

/* start (from the top, or continue where it stopped) and stop: the notes, MIDI programs */
static void transport(int on, int resume) {
  static int8_t stop_step;
  static uint8_t stop_tick;
  if (!on) {
    stop_step = play_step, stop_tick = tick6;
    play_step = -1;
    swing_in = -1;
    synths_stop();
    for (int t = 0; t < TRACKS; t++) {
      rat[t].left = 0;
      m_off(t);
    }
    return;
  }
  play_step = resume ? stop_step : -1;
  tick6 = resume ? stop_tick : 0;
  swing_in = -1;
  if (!resume) {
    cur_pattern = song.pattern;
    for (int t = 0; t < TRACKS; t++) { /* each MIDI track's program, as the song starts */
      if (is_midi(t) && song.track[t].prog) {
        midi_send((uint8_t)(0xC0 | (song.track[t].mch & 15)), (uint8_t)(song.track[t].prog - 1), 0);
      }
    }
  }
}

/* ---- the mix: each track at its level, into the dry mix and the sends. Track 1 ducks the others */
#define DLINE 44100
static int16_t dline[DLINE] POOL;
static uint32_t dpos;
static int32_t mix[256], snd[256], rsnd[256], csnd[256];
static int32_t duck_env; /* how loud track 1 is, Q15 */

static void render(uint32_t at, uint32_t n) {
  int32_t x[32], duck[32];
  int32_t rel = 12896 / (16 + song.pump * 12 / 5); /* Q20 a frame: back in ~ 30 .. 600 ms */
  for (int t = 0; t < TRACKS; t++) {
    const track_t *d = &song.track[t];
    for (uint32_t i = 0; i < n; i++) {
      x[i] = 0;
    }
    drums_render(t, x, n);
    synths_render(t, x, n); /* (it notices a track that stopped being one) */
    if (!t) {
      for (uint32_t i = 0; i < n; i++) {
        int32_t a = x[i] < 0 ? -x[i] : x[i];
        a = a > 16384 ? 32767 : a * 2;
        duck_env += a > duck_env ? (a - duck_env) >> 4 : -(int32_t)(((int64_t)duck_env * rel) >> 20) - 1;
        duck_env = duck_env < 0 ? 0 : duck_env;
        duck[i] = duck_env;
      }
    } else if (d->duck) {
      for (uint32_t i = 0; i < n; i++) {
        x[i] = (int32_t)(((int64_t)x[i] * (32768 - duck[i] * d->duck / 127)) >> 15);
      }
    }
    int32_t ds = d->send * 258 >> 4, rs = d->rsend * 258 >> 4, cs = d->csend * 258 >> 4;
    for (uint32_t i = 0; i < n; i++) {
      mix[at + i] += x[i];
      snd[at + i] += x[i] * ds >> 11;
      rsnd[at + i] += x[i] * rs >> 11;
      csnd[at + i] += x[i] * cs >> 11;
    }
  }
}

void audio(int32_t *out, uint32_t frames) {
  static uint8_t was;
  static uint32_t avg, peak; /* Q8 % */
  uint32_t t0 = micros();
  if (frames > 256) {
    frames = 256;
  }
  for (uint32_t i = 0; i < frames; i++) {
    mix[i] = snd[i] = rsnd[i] = csnd[i] = 0;
  }
  while (q_tail != q_head) {
    uint8_t q = q_tail;
    int t = queue[q].a, kind = queue[q].kind;
    if (kind == EV_MIDI) {
      midi_send(queue[q].a, queue[q].b, queue[q].c);
    } else if (is_midi(t)) {
      if (kind == EV_ON) {
        m_note_on(t, queue[q].b, queue[q].c, 0, -1);
      } else if (m_on[t] && m_note[t] == queue[q].b) {
        m_off(t);
      }
    } else if (is_engine(t)) {
      if (kind == EV_ON) {
        synths_note(t, queue[q].b, queue[q].c, 0, -1);
      } else {
        synths_off(t, queue[q].b);
      }
    } else if (kind == EV_ON) {
      drums_hit(t, queue[q].b, queue[q].c);
    }
    q_tail = (q + 1) & (QLEN - 1);
  }
  if (playing != was) { /* (set directly: the tests) */
    was = playing;
    transport(playing, 0);
  }
  const clock_event_t *ev;
  int nev = clock_events(&ev), e = 0;
  for (uint32_t i = 0; i < frames; i += 32) {
    uint32_t n = frames - i < 32 ? frames - i : 32;
    for (; e < nev && ev[e].at < i + n; e++) { /* the clock's ticks, start, continue, stop in this block */
      int what = ev[e].what;
      if (what == CLOCK_TICK) {
        if (playing) {
          tick();
        }
      } else if (what == CLOCK_STOP) {
        if (playing) {
          playing = was = 0;
          transport(0, 0);
        }
      } else if (what == CLOCK_START || !playing) { /* (continue only from stopped) */
        playing = was = 1;
        transport(1, what == CLOCK_CONTINUE);
      }
    }
    if (playing) {
      if (swing_in >= 0 && swing_in < (int32_t)n) {
        swing_in = -1;
        next_step();
      } else if (swing_in >= 0) {
        swing_in -= (int32_t)n;
      }
      rat_tick(n);
    } else {
      cur_pattern = song.pattern;
    }
    m_tick(n);
    render(i, n);
  }
  int32_t fb = song.dfdbk * 230;
  uint32_t dl = dlen;
  for (uint32_t i = 0; i < frames; i++) { /* the delay */
    uint32_t r = dpos >= dl ? dpos - dl : dpos + DLINE - dl;
    int32_t wet = dline[r];
    dline[dpos] = (int16_t)clamp(snd[i] + (wet * fb >> 15), -32767, 32767);
    if (++dpos == DLINE) {
      dpos = 0;
    }
    mix[i] += wet * 3 >> 2;
  }
  fx_reverb(rsnd, mix, frames);
  fx_chorus(csnd, mix, frames);
  fx_master(mix, frames); /* the filter and the distortion */
  for (uint32_t i = 0; i < frames; i++) {
    int32_t v = clamp(mix[i], -49151, 49151);
    int32_t y = v - (int32_t)((((((int64_t)v * v) >> 15) * v >> 15) * 4855) >> 15); /* soft clip */
    out[2 * i] += y;
    out[2 * i + 1] += y;
    scope[(scope_at + i) & (SCOPE_LEN - 1)] = (int16_t)clamp(y, -32767, 32767);
  }
#if FOMA_USB_AUDIO
  const int32_t *in = audio_in(); /* a computer playing to the FM-1: added to the output as it comes (stereo) */
  for (uint32_t i = 0; in && i < 2 * frames; i++) {
    out[i] += in[i];
  }
#endif
  scope_at += frames;
  /* how long that took, of the time the block lasts (22.676 us a frame): smoothed, and the peak held a while */
  uint32_t load = (micros() - t0) * 100u * 256u / (frames * 22676u / 1000u + 1u);
  avg += ((int32_t)load - (int32_t)avg) >> 4;
  peak = load > peak ? load : peak - (peak >> 9);
  cpu_load = (uint8_t)(avg >> 8 > 255 ? 255 : avg >> 8);
  cpu_peak = (uint8_t)(peak >> 8 > 255 ? 255 : peak >> 8);
}
