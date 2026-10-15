/* FOMAJAM's sound: the step clock, MIDI out, the delay and the output. The drum machines are in
 * drums.c, the synth engines in synths.c. It all runs in blocks of 32 frames (Felucca's), steps
 * starting on a block: integers only, no screen, no flash.
 *
 * audio() is the one sender of MIDI (the USB queue has one writer): a MIDI track's notes and their
 * note-offs, the clock, and what loop() asks for (midi_out: controllers, programs, notes played). */
#include "groove.h"

song_t song;
volatile uint8_t playing;
volatile uint8_t cur_pattern;
volatile int8_t play_step = -1;
volatile uint16_t hits[TRACKS];
volatile uint8_t cpu_load, cpu_peak;

static volatile uint32_t len_even = 5512, len_odd = 5512, dlen = 11025; /* in frames, from engine_tempo() */

static int clamp(int v, int lo, int hi) {
  return v < lo ? lo : v > hi ? hi
                              : v;
}

static int is_engine(int t) { return machine_engine(song.track[t].machine) >= 0; }
static int is_midi(int t) { return machine_midi(song.track[t].machine); }

/* ---- what loop() asks for: loop() queues, audio() does it */
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

/* ---- the clock */
void engine_tempo(void) {
  uint32_t base = 661500u / (song.bpm < 40 ? 40 : song.bpm); /* frames per 16th */
  uint32_t even = 2 * base * (200 + song.swing) / 400;       /* swing 0..100: 50% .. 75% of the pair */
  len_even = even;
  len_odd = 2 * base - even;
  uint32_t d = song.dtime * base;
  dlen = d >= 44100 ? 44099 : d;
}

/* ---- ratchets: a step's repeats, the rest of its length apart */
static struct {
  uint8_t left, val, vel;
  int32_t every, next;
} rat[TRACKS];

static void hit(int t, int val, int vel, int slide_in, int32_t gate) { /* a step's sound, on whatever the track is */
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

static int32_t ctr;             /* frames to the next step */
static uint32_t step_frames;    /* the step playing's length */
static uint32_t clk_acc, clk_n; /* MIDI clock: 6 a 16th, the first on the step */

static void next_step(void) {
  int len = song.length < 1 ? 1 : song.length > STEPS ? STEPS
                                                      : song.length;
  int s = play_step + 1 >= len ? 0 : play_step + 1, prev = s ? s - 1 : len - 1;
  if (!s) {
    cur_pattern = song.pattern;
  }
  play_step = (int8_t)s;
  const pattern_t *p = &song.pat[cur_pattern];
  uint32_t step_len = (s & 1) ? len_odd : len_even;
  step_frames = step_len;
  if (song.clock) {
    midi_send(0xF8, 0, 0);
    clk_acc = 0;
    clk_n = 1;
  }
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
  ctr += (int32_t)step_len; /* (what the last step ran over its block is taken off this one) */
}

static void clock_tick(uint32_t n) { /* the step's other five clocks, where they fall */
  if (!song.clock || clk_n >= 6) {
    return;
  }
  clk_acc += n * 6;
  while (clk_n < 6 && clk_acc >= step_frames) {
    clk_acc -= step_frames;
    midi_send(0xF8, 0, 0);
    clk_n++;
  }
}

/* ---- the delay and the output */
#define DLINE 44100
static int16_t dline[DLINE] POOL;
static uint32_t dpos;
static int32_t mix[256], snd[256];

static void render(uint32_t at, uint32_t n) {
  drums_render(mix + at, snd + at, n);
  for (int t = 0; t < TRACKS; t++) {
    synths_render(t, mix + at, snd + at, n); /* (it notices a track that stopped being one) */
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
    mix[i] = snd[i] = 0;
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
  if (playing != was) {
    was = playing;
    play_step = -1;
    ctr = 0;
    if (!playing) {
      synths_stop();
      for (int t = 0; t < TRACKS; t++) {
        rat[t].left = 0;
      }
      for (int t = 0; t < TRACKS; t++) {
        m_off(t);
      }
      if (song.clock) {
        midi_send(0xFC, 0, 0); /* stop */
      }
    } else {
      cur_pattern = song.pattern;
      for (int t = 0; t < TRACKS; t++) { /* each MIDI track's program, as the song starts */
        if (is_midi(t) && song.track[t].prog) {
          midi_send((uint8_t)(0xC0 | (song.track[t].mch & 15)), (uint8_t)(song.track[t].prog - 1), 0);
        }
      }
      if (song.clock) {
        midi_send(0xFA, 0, 0); /* start */
      }
    }
  }
  if (!playing) {
    cur_pattern = song.pattern;
  }
  for (uint32_t i = 0; i < frames; i += 32) {
    uint32_t n = frames - i < 32 ? frames - i : 32;
    if (playing) {
      if (ctr <= 0) {
        next_step();
      }
      ctr -= (int32_t)n;
      clock_tick(n);
      rat_tick(n);
    }
    m_tick(n);
    render(i, n);
  }
  int32_t fb = song.dfdbk * 230, drive = 128 + song.drive * 6;
  uint32_t dl = dlen;
  for (uint32_t i = 0; i < frames; i++) {
    uint32_t r = dpos >= dl ? dpos - dl : dpos + DLINE - dl;
    int32_t wet = dline[r];
    dline[dpos] = (int16_t)clamp(snd[i] + (wet * fb >> 15), -32767, 32767);
    if (++dpos == DLINE) {
      dpos = 0;
    }
    int32_t v = clamp((mix[i] + (wet * 3 >> 2)) * drive >> 8, -49151, 49151);
    int32_t y = v - (int32_t)((((((int64_t)v * v) >> 15) * v >> 15) * 4855) >> 15); /* soft clip */
    out[2 * i] += y;
    out[2 * i + 1] += y;
  }
  /* how long that took, of the time the block lasts (22.676 us a frame): smoothed, and the peak held a while */
  uint32_t load = (micros() - t0) * 100u * 256u / (frames * 22676u / 1000u + 1u);
  avg += ((int32_t)load - (int32_t)avg) >> 4;
  peak = load > peak ? load : peak - (peak >> 9);
  cpu_load = (uint8_t)(avg >> 8 > 255 ? 255 : avg >> 8);
  cpu_peak = (uint8_t)(peak >> 8 > 255 ? 255 : peak >> 8);
}
