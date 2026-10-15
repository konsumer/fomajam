/* FOMAJAM: a groovebox. 9 tracks, each its own machine: a drum machine (808, 909, 707, DMX ...) with
 * a voice per step, or a synth engine (ANALOG, LOFI, ... Felucca's) with a note per step; 8 patterns of 16 steps.
 *
 * Every page has the same three parts: the header (page, track, pattern, tempo), the body, and the
 * knob bar: KNOB1..4 always sit under the screen in the same four colours, named for what they do here.
 *
 *   SEQ        PATTERN  white keys = steps, black keys = tracks
 *   EDIT       SOUND    the track's sound (SEL: the second page of knobs); keys play
 *   HOME       MIX      levels; white keys = mutes, black keys = tracks
 *   FX         FX       the delay and the drive; keys play
 *   GLO        GLOBAL   tempo, swing, length, theme; keys play
 *   SELECT     the track          PRESETS    the pattern (changes at the end of the bar)
 *   ALGORITHM  the tempo          PLAY / REC / SAVE / OCT- OCT+
 *   SEL        the next page of knobs; on PATTERN with a synth: SLIDE on the cursor's step (lit when it slides)
 *   ENV        clear the pattern? OCT+ yes, OCT- no (OCT- / OCT+ answer every question)
 *   ARP        on PATTERN: the cursor's step's ratchet, x1 x2 x3 x4 x6 x8 (lit when it has one)
 *
 * The sound is in engine.c, the colours in themes.h. */
#include "groove.h"
#include "themes.h"

/* ---- the layout, the same on every page */
#define HEAD_H 24
#define BODY_Y 26
#define BODY_H 156
#define KNOB_Y 184

enum { PG_PATTERN,
       PG_SOUND,
       PG_MIX,
       PG_FX,
       PG_GLOBAL,
       PAGES };
static const char *const PAGE_NAME[PAGES] = {"PATTERN", "SOUND", "MIX", "FX", "GLOBAL"};
static const uint8_t PAGE_BTN[PAGES] = {BTN_SEQ, BTN_EDIT, BTN_HOME, BTN_FX, BTN_GLO};

static int page, sub, track, cursor, octave = -1, rec;
static const theme_t *th;

/* ---- the knobs: every value is a parameter with a name, a range and a way to show it */
enum { F_NUM,
       F_SEMI,
       F_MACHINE,
       F_VOICE,
       F_ONOFF,
       F_STEP,
       F_THEME,
       F_16TH,
       F_PCT,
       F_ENGINE,
       F_BIPOLAR,
       F_PRESET,
       F_CHAN,
       F_GATE,
       F_PROG,
       F_NONE };
enum { P_TUNE,
       P_DECAY,
       P_TONE,
       P_LEVEL,
       P_MACHINE,
       P_VOICE,
       P_EXTRA,
       P_SEND,
       P_SLIDE,
       P_STEP,
       P_STACC,
       P_STSLIDE,
       P_STVOICE,
       P_MUTE,
       P_DRIVE,
       P_BPM,
       P_SWING,
       P_LENGTH,
       P_THEME,
       P_DTIME,
       P_DFDBK,
       P_SE0, /* a synth engine's eight */
       P_SE1,
       P_SE2,
       P_SE3,
       P_SE4,
       P_SE5,
       P_SE6,
       P_SE7,
       P_ATK,
       P_DEC,
       P_SUS,
       P_REL,
       P_FENV,
       P_PRESET,
       P_MONO,
       P_MCH, /* a MIDI track's */
       P_PROG,
       P_GATE,
       P_VELO,
       P_CC0,
       P_CC1,
       P_CC2,
       P_CC3,
       P_CCN0,
       P_CCN1,
       P_CCN2,
       P_CCN3,
       P_CLOCK,
       P_NONE, /* an empty knob */
       PARAMS };
static const struct {
  const char *name;
  uint8_t lo, hi, fmt;
} PARAM[PARAMS] = {
    [P_TUNE] = {"TUNE", 0, 127, F_SEMI},
    [P_DECAY] = {"DECAY", 0, 127, F_NUM},
    [P_TONE] = {"TONE", 0, 127, F_NUM},
    [P_LEVEL] = {"LEVEL", 0, 127, F_NUM},
    [P_MACHINE] = {"MACHINE", 0, 0, F_MACHINE}, /* up to: phi() */
    [P_VOICE] = {"VOICE", 0, 0, F_VOICE},
    [P_EXTRA] = {"EXTRA", 0, 127, F_NUM},
    [P_SEND] = {"SEND", 0, 127, F_NUM},
    [P_SLIDE] = {"SLIDE", 0, 127, F_NUM},
    [P_STEP] = {"STEP", 0, STEPS - 1, F_STEP},
    [P_STACC] = {"ACCENT", 0, 1, F_ONOFF},
    [P_STSLIDE] = {"SLIDE", 0, 1, F_ONOFF},
    [P_STVOICE] = {"VOICE", 0, 0, F_VOICE},
    [P_MUTE] = {"MUTE", 0, 1, F_ONOFF},
    [P_DRIVE] = {"DRIVE", 0, 127, F_NUM},
    [P_BPM] = {"BPM", 40, 250, F_NUM},
    [P_SWING] = {"SWING", 0, 100, F_PCT},
    [P_LENGTH] = {"LENGTH", 1, STEPS, F_NUM},
    [P_THEME] = {"THEME", 0, THEME_COUNT - 1, F_THEME},
    [P_DTIME] = {"TIME", 1, 16, F_16TH},
    [P_DFDBK] = {"FDBK", 0, 127, F_NUM},
    [P_SE0] = {"", 0, 0, F_ENGINE}, /* names, ranges: the engine's (synth_param) */
    [P_SE1] = {"", 0, 0, F_ENGINE},
    [P_SE2] = {"", 0, 0, F_ENGINE},
    [P_SE3] = {"", 0, 0, F_ENGINE},
    [P_SE4] = {"", 0, 0, F_ENGINE},
    [P_SE5] = {"", 0, 0, F_ENGINE},
    [P_SE6] = {"", 0, 0, F_ENGINE},
    [P_SE7] = {"", 0, 0, F_ENGINE},
    [P_ATK] = {"ATTACK", 0, 127, F_NUM},
    [P_DEC] = {"DECAY", 0, 127, F_NUM},
    [P_SUS] = {"SUSTN", 0, 127, F_NUM},
    [P_REL] = {"RELEASE", 0, 127, F_NUM},
    [P_FENV] = {"ENV>FLT", 0, 127, F_BIPOLAR}, /* -64..63: plo() */
    [P_PRESET] = {"PRESET", 0, 0, F_PRESET},
    [P_MONO] = {"MONO", 0, 1, F_ONOFF},
    [P_MCH] = {"CHAN", 0, 15, F_CHAN},
    [P_PROG] = {"PROG", 0, 128, F_PROG},
    [P_GATE] = {"GATE", 1, GATE_TIE, F_GATE},
    [P_VELO] = {"VELO", 1, 127, F_NUM},
    [P_CC0] = {"CC", 0, 127, F_NUM}, /* named by their number: pname() */
    [P_CC1] = {"CC", 0, 127, F_NUM},
    [P_CC2] = {"CC", 0, 127, F_NUM},
    [P_CC3] = {"CC", 0, 127, F_NUM},
    [P_CCN0] = {"CC1 #", 0, 127, F_NUM},
    [P_CCN1] = {"CC2 #", 0, 127, F_NUM},
    [P_CCN2] = {"CC3 #", 0, 127, F_NUM},
    [P_CCN3] = {"CC4 #", 0, 127, F_NUM},
    [P_CLOCK] = {"CLOCK", 0, 1, F_ONOFF},
    [P_NONE] = {"", 0, 0, F_NONE},
};

static int knobs[4];

static int machine_of(int t) { return song.track[t].machine; }
static int is_synth(int t) { return machine_synth(machine_of(t)); } /* a synth engine: notes */
static int engine_of(int t) { return machine_engine(machine_of(t)); }

static int is_se(int id) { return id >= P_SE0 && id <= P_SE7; }

static int phi(int id) { /* a parameter's highest value */
  synth_param_t sp;
  if (is_se(id)) {
    synth_param(track, id - P_SE0, &sp);
    return sp.hi;
  }
  return id == P_MACHINE                    ? machine_count() - 1
         : id == P_VOICE || id == P_STVOICE ? machine_voices(machine_of(track)) - 1
         : id == P_PRESET                   ? (engine_of(track) >= 0 ? synth_presets(engine_of(track)) - 1 : 0)
         : id == P_FENV                     ? 63
                                            : PARAM[id].hi;
}

static int plo(int id) { /* and its lowest */
  synth_param_t sp;
  if (is_se(id)) {
    synth_param(track, id - P_SE0, &sp);
    return sp.lo;
  }
  return id == P_FENV ? -64 : PARAM[id].lo;
}

static const char *pname(int id) {  /* EXTRA is what the voice's original called it; a synth has notes */
  if (id >= P_CC0 && id <= P_CC3) { /* a controller by its number: CC74 */
    static char s[4][8];
    char *o = s[id - P_CC0];
    o[0] = 'C', o[1] = 'C';
    format_int(o + 2, song.track[track].ccn[id - P_CC0]);
    return o;
  }
  if (id == P_STVOICE && is_synth(track)) {
    return "NOTE";
  }
  if (is_se(id)) {
    synth_param_t sp;
    synth_param(track, id - P_SE0, &sp);
    return sp.name;
  }
  if (id == P_SLIDE && engine_of(track) >= 0) {
    return "GLIDE";
  }
  if (id == P_EXTRA) {
    const char *v = voice_name(machine_of(track), song.track[track].voice);
    return v[0] == 'B' && v[1] == 'D' ? "ATTACK" : v[0] == 'S' && v[1] == 'D' ? "SNAPPY"
                                                                              : "EXTRA";
  }
  return PARAM[id].name;
}

static int is_midi(int t) { return machine_midi(machine_of(t)); }

static int subpages(void) {
  return page == PG_GLOBAL ? 2 : page != PG_SOUND    ? 1
                             : engine_of(track) >= 0 ? 5
                             : is_midi(track)        ? 3
                                                     : 2;
}

static const char *subpage_name(int i) { /* the tabs on the SOUND and GLOBAL pages */
  static const char *const DRUM[2] = {"PLAY", "MORE"}, *const SE[3] = {"ENV", "PATCH", "PLAY"}, *const MIDI[3] = {"NOTE", "CC", "CC#"},
                           *const GLOBAL[2] = {"TEMPO", "MIDI"};
  int e = engine_of(track);
  return page == PG_GLOBAL ? GLOBAL[i & 1] : e >= 0       ? (i < 2 ? synth_page(e, i) : SE[i - 2])
                                         : is_midi(track) ? MIDI[i % 3]
                                                          : DRUM[i & 1];
}

static void pick_knobs(int *k) {
  static const uint8_t DRUM_STEP[4] = {P_STEP, P_STVOICE, P_STACC, P_MACHINE};
  static const uint8_t DRUM_PLAY[4] = {P_TUNE, P_DECAY, P_TONE, P_LEVEL};
  static const uint8_t DRUM_MORE[4] = {P_MACHINE, P_VOICE, P_EXTRA, P_SEND};
  static const uint8_t SYN_STEP[4] = {P_STEP, P_STVOICE, P_STACC, P_MACHINE}; /* SLIDE: the SEL button */
  static const uint8_t SE[5][4] = {{P_SE0, P_SE1, P_SE2, P_SE3}, {P_SE4, P_SE5, P_SE6, P_SE7}, {P_ATK, P_DEC, P_SUS, P_REL}, {P_PRESET, P_FENV, P_LEVEL, P_SEND}, {P_GATE, P_MONO, P_SLIDE, P_TUNE}};
  static const uint8_t MIX[4] = {P_LEVEL, P_SEND, P_MUTE, P_DRIVE};
  static const uint8_t FX[4] = {P_SEND, P_DTIME, P_DFDBK, P_DRIVE};
  static const uint8_t GLOBAL[2][4] = {{P_BPM, P_SWING, P_LENGTH, P_THEME}, {P_CLOCK, P_NONE, P_NONE, P_NONE}};
  static const uint8_t MIDI[3][4] = {{P_MCH, P_PROG, P_GATE, P_VELO}, {P_CC0, P_CC1, P_CC2, P_CC3}, {P_CCN0, P_CCN1, P_CCN2, P_CCN3}};
  const uint8_t *src = page == PG_MIX                              ? MIX
                       : page == PG_FX                             ? FX
                       : page == PG_GLOBAL                         ? GLOBAL[sub & 1]
                       : is_midi(track) && page == PG_SOUND        ? MIDI[sub % 3]
                       : engine_of(track) >= 0 && page == PG_SOUND ? SE[sub % 5]
                       : is_synth(track)                           ? SYN_STEP
                       : page == PG_PATTERN                        ? DRUM_STEP
                                                                   : (sub ? DRUM_MORE : DRUM_PLAY);
  for (int i = 0; i < 4; i++) {
    k[i] = src[i];
  }
}

static pattern_t *pat(void) { return &song.pat[song.pattern]; }

static uint8_t *pbyte(int id) { /* the byte behind a parameter, if it is one */
  track_t *d = &song.track[track];
  switch (id) {
    case P_TUNE:
      return &d->tune;
    case P_DECAY:
      return &d->decay;
    case P_TONE:
      return &d->tone;
    case P_LEVEL:
      return &d->level;
    case P_MACHINE:
      return &d->machine;
    case P_VOICE:
      return &d->voice;
    case P_EXTRA:
      return &d->extra;
    case P_SEND:
      return &d->send;
    case P_SLIDE:
      return &d->slide;
    case P_ATK:
      return &d->atk;
    case P_DEC:
      return &d->dec;
    case P_SUS:
      return &d->sus;
    case P_REL:
      return &d->rel;
    case P_PRESET:
      return &d->preset;
    case P_MONO:
      return &d->mono;
    case P_MCH:
      return &d->mch;
    case P_PROG:
      return &d->prog;
    case P_GATE:
      return &d->gate;
    case P_VELO:
      return &d->velo;
    case P_CC0:
    case P_CC1:
    case P_CC2:
    case P_CC3:
      return &d->cc[id - P_CC0];
    case P_CCN0:
    case P_CCN1:
    case P_CCN2:
    case P_CCN3:
      return &d->ccn[id - P_CCN0];
    case P_CLOCK:
      return &song.clock;
    case P_DRIVE:
      return &song.drive;
    case P_BPM:
      return &song.bpm;
    case P_SWING:
      return &song.swing;
    case P_LENGTH:
      return &song.length;
    case P_THEME:
      return &song.theme;
    case P_DTIME:
      return &song.dtime;
    case P_DFDBK:
      return &song.dfdbk;
  }
  return 0;
}

static int pget(int id) {
  if (is_se(id)) {
    return song.track[track].e[id - P_SE0];
  }
  uint8_t *b = pbyte(id);
  if (b) {
    return *b;
  }
  switch (id) {
    case P_STEP:
      return cursor;
    case P_FENV:
      return song.track[track].fenv;
    case P_STACC:
      return !!(pat()->flag[track][cursor] & STEP_ACC);
    case P_STVOICE: /* -1: a rest */
      return pat()->flag[track][cursor] & STEP_ON ? pat()->val[track][cursor] : -1;
    case P_STSLIDE:
      return !!(pat()->flag[track][cursor] & STEP_SLIDE);
    case P_MUTE:
      return song.mute >> track & 1;
  }
  return 0;
}

static void flip(uint8_t *f, int bit, int on) {
  *f = (uint8_t)(on ? *f | bit : *f & ~bit);
}

static void flip16(uint16_t *bits, int b, int on) {
  *bits = (uint16_t)(on ? *bits | 1u << b : *bits & ~(1u << b));
}

static void pset(int id, int v) {
  uint8_t *b = pbyte(id);
  if (is_se(id)) {
    song.track[track].e[id - P_SE0] = (int8_t)v;
  } else if (id == P_FENV) {
    song.track[track].fenv = (int8_t)v;
  } else if (b) {
    *b = (uint8_t)v;
    if (id == P_PRESET) {
      synth_load(track, v);
    } else if (id == P_MACHINE && engine_of(track) >= 0) { /* a synth engine: its first preset */
      synth_load(track, 0);
    }
  } else if (id == P_STEP) {
    cursor = v;
  } else if (id == P_STACC || id == P_STSLIDE) { /* on a rest: the step comes on too */
    uint8_t *f = &pat()->flag[track][cursor];
    if (v && !(*f & STEP_ON)) {
      pat()->val[track][cursor] = song.track[track].voice;
      *f |= STEP_ON;
    }
    flip(f, id == P_STACC ? STEP_ACC : STEP_SLIDE, v);
  } else if (id == P_STVOICE) {
    uint8_t *f = &pat()->flag[track][cursor];
    if (v < 0) {
      *f = 0;
    } else {
      pat()->val[track][cursor] = (uint8_t)v;
      *f |= STEP_ON;
      song.track[track].voice = (uint8_t)v; /* new steps get it too */
    }
  } else if (id == P_MUTE) {
    flip16(&song.mute, track, v);
  }
}

static void pformat(int id, int v, char *out) {
  const char *s = 0;
  int n;
  switch (PARAM[id].fmt) {
    case F_SEMI:
      n = (v - 64) * 12 / 64;
      out[0] = n < 0 ? '-' : '+';
      format_int(out + 1, n < 0 ? -n : n);
      if (!n) {
        out[0] = '0', out[1] = 0;
      }
      return;
    case F_MACHINE:
      s = machine_name(v);
      break;
    case F_VOICE:
      s = v < 0 ? "--" : voice_name(machine_of(track), v);
      break;
    case F_ONOFF:
      s = v ? "ON" : "OFF";
      break;
    case F_THEME:
      s = THEMES[v].name;
      break;
    case F_STEP:
      format_int(out, v + 1);
      return;
    case F_16TH: {
      format_int(out, v);
      char *o = out;
      while (*o) {
        o++;
      }
      o[0] = '/', o[1] = '1', o[2] = '6', o[3] = 0;
      return;
    }
    case F_PCT:
      format_int(out, 50 + v / 4); /* the swung 16th's share of the pair */
      return;
    case F_BIPOLAR:
      out[0] = v < 0 ? '-' : '+';
      format_int(out + 1, v < 0 ? -v : v);
      return;
    case F_GATE:
      if (v >= GATE_TIE) {
        s = "TIE";
        break;
      }
      format_int(out, v);
      return;
    case F_CHAN:
      format_int(out, v + 1);
      return;
    case F_PROG:
      if (!v) {
        s = "--";
        break;
      }
      format_int(out, v - 1);
      return;
    case F_NONE:
      out[0] = 0;
      return;
    case F_PRESET:
      s = engine_of(track) >= 0 ? synth_preset_name(engine_of(track), v) : "";
      break;
    case F_ENGINE: {
      synth_param_t sp;
      synth_param(track, id - P_SE0, &sp);
      if (sp.fmt == SF_ENUM) {
        s = sp.names[v - sp.lo];
        break;
      }
      if (sp.fmt == SF_SEMI) {
        out[0] = v < 0 ? '-' : '+';
        format_int(out + 1, v < 0 ? -v : v);
        return;
      }
      format_int(out, v);
      return;
    }
    default:
      format_int(out, v);
      return;
  }
  int i = 0;
  for (; s[i] && i < 7; i++) { /* (7: what fits a knob) */
    out[i] = s[i];
  }
  out[i] = 0;
}

/* ---- drawing helpers */
static int len(const char *s) {
  int n = 0;
  while (s[n]) {
    n++;
  }
  return n;
}

static void text_center(int x, int w, int y, const char *s, uint16_t color, int font) {
  text(x + (w - text_width(s, font)) / 2, y, s, color, font);
}

static uint16_t tcolor(int t) { return (song.mute >> t & 1) ? th->dim : th->track[t]; }

static const char *track_short(int t) { return machine_label(machine_of(t)); }

static int append(char *out, int i, const char *a) {
  for (; *a && i < 14; a++) {
    out[i++] = *a;
  }
  out[i] = 0;
  return i;
}

static void track_title(int t, char *out, int label) { /* "3 909 CH", "9 ANALOG A1"; label: "9 ANA A1" */
  out[0] = (char)('1' + t);
  out[1] = ' ';
  int i = append(out, 2, label ? machine_label(machine_of(t)) : machine_name(machine_of(t)));
  out[i++] = ' ';
  append(out, i, voice_name(machine_of(t), song.track[t].voice));
}

/* ---- the header: page, track (or a message), pattern, tempo, transport */
static char toast_msg[16];
static uint32_t toast_until;

static void draw_transport(void) {
  char s[12];
  int queued = playing && cur_pattern != song.pattern;
  rect(166, 0, 74, HEAD_H, th->panel);
  s[0] = 'P';
  format_int(s + 1, song.pattern + 1);
  text(168, 4, s, queued ? th->accent : th->dim, FONT_SMALL);
  format_int(s, song.bpm);
  text(218 - text_width(s, FONT_SMALL), 4, s, th->text, FONT_SMALL);
  uint16_t c = rec ? RGB(255, 60, 60) : th->text;
  if (playing) {
    for (int i = 0; i < 6; i++) {
      rect(224 + i, 6 + i, 1, 13 - 2 * i, c);
      rect(224 + i, 6 + i, 1, 1, c);
    }
  } else {
    rect(224, 7, 10, 10, rec ? c : th->dim);
  }
}

static void draw_track_chip(void) {
  int x = text_width(PAGE_NAME[page], FONT_SMALL) + 16;
  rect(x, 0, 166 - x, HEAD_H, th->panel);
  if (toast_until) {
    text(x + 2, 4, toast_msg, th->accent, FONT_SMALL);
    return;
  }
  char s[16];
  track_title(track, s, 0);
  if (text_width(s, FONT_SMALL) + 10 > 164 - x) { /* too long for the room: the machine's label */
    track_title(track, s, 1);
  }
  int w = text_width(s, FONT_SMALL) + 10;
  rect(x, 3, w, 18, th->track[track]);
  text(x + 5, 4, s, th->bg, FONT_SMALL);
}

static void draw_header(void) {
  int w = text_width(PAGE_NAME[page], FONT_SMALL) + 12;
  rect(0, 0, w, HEAD_H, th->accent);
  text(6, 4, PAGE_NAME[page], th->bg, FONT_SMALL);
  rect(w, 0, 4, HEAD_H, th->panel);
  draw_track_chip();
  draw_transport();
}

static void toast(const char *s) {
  int i = 0;
  for (; s[i] && i < 15; i++) {
    toast_msg[i] = s[i];
  }
  toast_msg[i] = 0;
  toast_until = millis() + 1200;
  draw_track_chip();
}

/* ---- the knob bar */
static void draw_knob(int k) {
  int id = knobs[k], x = k * 60, v = pget(id);
  uint16_t c = th->knob[k];
  char s[16];
  rect(x, KNOB_Y, 59, 56, th->panel);
  if (id == P_NONE) {
    return;
  }
  rect(x, KNOB_Y, 59, 2, c);
  text_center(x, 59, KNOB_Y + 4, pname(id), c, FONT_SMALL);
  pformat(id, v, s);
  if (len(s) <= 3) {
    text_center(x, 59, KNOB_Y + 18, s, th->text, FONT_BIG);
  } else {
    text_center(x, 59, KNOB_Y + 27, s, th->text, FONT_SMALL);
  }
  int lo = plo(id), hi = phi(id), fill = v < lo ? 0 : hi > lo ? (v - lo) * 51 / (hi - lo)
                                                              : 51;
  rect(x + 4, KNOB_Y + 50, fill, 3, c);
  rect(x + 4 + fill, KNOB_Y + 50, 51 - fill, 3, th->off);
}

/* ---- PATTERN: 9 rows of 16 steps */
#define GRID_Y 28
#define ROW_H 16
#define CELL_X 30
#define CELL_W 13
static uint32_t flash_until[TRACKS];

static void draw_label(int t) {
  int y = page == PG_MIX ? 28 : GRID_Y + t * ROW_H, x = page == PG_MIX ? 3 + t * 26 : 0, w = page == PG_MIX ? 22 : 27;
  int lit = t == track || flash_until[t];
  rect(x, y, w, 15, lit ? tcolor(t) : th->bg);
  text_center(x, w, y, track_short(t), lit ? th->bg : tcolor(t), FONT_SMALL);
}

/* a drum step is coloured by its voice (kicks like the first track's colour, snares the second's ...);
 * a synth's by its track */
static uint16_t voice_color(int t, int v) {
  static const uint8_t FAMILY[22] = {0, 0, 1, 1, 2, 2, 5, 5, 6, 3, 4, 4, 4, 7, 7, 3, 5, 5, 6, 6, 3, 2};
  int n = machine_voices(machine_of(t));
  if (song.mute >> t & 1) {
    return th->dim;
  }
  return n == 22 ? th->track[FAMILY[v % 22]] : n == 128 ? th->track[t]
                                                        : th->track[v % 8];
}

static void draw_cell(int t, int s) {
  int x = CELL_X + s * CELL_W, y = GRID_Y + t * ROW_H;
  pattern_t *p = pat();
  if (s >= song.length) {
    rect(x, y, CELL_W, 15, th->bg);
    return;
  }
  int f = p->flag[t][s], on = f & STEP_ON, acc = f & STEP_ACC;
  uint16_t c = !on ? ((s & 3) ? th->off : th->beat) : voice_color(t, p->val[t][s]);
  rect(x, y, CELL_W - 2, 15, c);
  rect(x + CELL_W - 2, y, 2, 15, th->bg);
  if (on && acc) {
    rect(x, y, CELL_W - 2, 3, th->text);
  }
  if (on && (f & STEP_SLIDE)) {
    rect(x + CELL_W - 2, y + 6, 2, 3, tcolor(t));
  }
  int r = RATCHET[(f & STEP_RAT) >> RAT_SHIFT];
  if (on && r > 1) { /* a ratchet: the cell cut in its hits (up to 4), x6 and x8 underlined too */
    int segs = r > 4 ? 4 : r;
    for (int k = 1; k < segs; k++) {
      rect(x + k * (CELL_W - 2) / segs, y + 3, 1, 12, th->bg);
    }
    if (r > 4) {
      rect(x, y + 13, CELL_W - 2, 2, th->text);
    }
  }
  if (t == track && s == cursor) {
    frame(x, y, CELL_W - 2, 15, th->text);
  }
}

static int playhead_drawn = -1;

static void draw_playhead(int s) {
  if (playhead_drawn >= 0) {
    rect(CELL_X + playhead_drawn * CELL_W, 174, CELL_W - 2, 4, th->bg);
  }
  playhead_drawn = s;
  if (s >= 0) {
    rect(CELL_X + s * CELL_W, 174, CELL_W - 2, 4, th->accent);
  }
}

static void draw_row(int t) {
  draw_label(t);
  for (int s = 0; s < STEPS; s++) {
    draw_cell(t, s);
  }
}

/* ---- SOUND: the track's name, its decay drawn, and what both knob pages hold */
static void draw_tabs(void) { /* a page's pages of knobs, as tabs: SEL moves on */
  int n = subpages(), w = 232 / n;
  for (int i = 0; i < n; i++) {
    rect(4 + i * w, 160, w - 4, 20, i == sub ? th->accent : th->panel);
    text_center(4 + i * w, w - 4, 162, subpage_name(i), i == sub ? th->bg : th->dim, FONT_SMALL);
  }
}

static void draw_sound(void) {
  rect(0, BODY_Y, SCREEN_W, BODY_H, th->bg);
  if (is_midi(track)) { /* where it goes */
    const track_t *d = &song.track[track];
    char s[24] = "CHANNEL ";
    text(8, 30, "MIDI OUT", th->track[track], FONT_BIG);
    format_int(s + 8, d->mch + 1);
    text(8, 76, s, th->text, FONT_SMALL);
    pformat(P_PROG, d->prog, s);
    text(8, 96, "PROGRAM", th->dim, FONT_SMALL);
    text(80, 96, s, th->text, FONT_SMALL);
    text(8, 116, "GATE", th->dim, FONT_SMALL);
    rect(80, 120, 150, 8, th->off);
    rect(80, 120, d->gate * 150 / 100, 8, th->track[track]);
  } else if (engine_of(track) >= 0) { /* the engine, its preset, its envelope */
    const track_t *d = &song.track[track];
    uint16_t c = th->track[track];
    text(8, 30, machine_name(machine_of(track)), c, FONT_BIG);
    text(8, 62 - 12 + 4, synth_preset_name(engine_of(track), d->preset), th->text, FONT_SMALL);
    int x = 8, w = d->atk * 56 / 127 + 1, y0 = 140, top = 76, sus = y0 - d->sus * (y0 - top) / 127;
    for (int i = 0; i < w; i++, x++) { /* attack */
      int h = (y0 - top) * (i + 1) / w;
      rect(x, y0 - h, 1, h + 1, c);
    }
    w = d->dec * 56 / 127 + 1;
    for (int i = 0; i < w; i++, x++) { /* decay */
      int y = top + (sus - top) * (i + 1) / w;
      rect(x, y, 1, y0 - y + 1, c);
    }
    rect(x, sus, 48, y0 - sus + 1, c); /* sustain */
    x += 48;
    w = d->rel * 56 / 127 + 1;
    for (int i = 0; i < w; i++, x++) { /* release */
      int y = sus + (y0 - sus) * (i + 1) / w;
      rect(x, y, 1, y0 - y + 1, c);
    }
  } else { /* the machine, the voice, and how it rings */
    uint8_t h[224];
    int v = song.track[track].voice, x = text(8, 30, machine_name(machine_of(track)), th->track[track], FONT_BIG);
    text(x + 10, 30, voice_name(machine_of(track), v), voice_color(track, v), FONT_BIG);
    drum_envelope(track, v, h, 224);
    rect(8, 70, 224, 71, th->bg);
    for (int i = 0; i < 224; i++) {
      int hh = h[i] * 70 / 255;
      rect(8 + i, 140 - hh, 1, hh + 1, voice_color(track, v));
    }
  }
  draw_tabs();
}

/* ---- MIX: a strip per track */
static void draw_strip(int t) {
  int x = 3 + t * 26, h = song.track[t].level * 116 / 127;
  draw_label(t);
  rect(x, 48, 22, 116 - h, th->off);
  rect(x, 48 + 116 - h, 22, h, tcolor(t));
  rect(x, 166, 22, 15, th->bg);
  if (song.mute >> t & 1) {
    text_center(x, 22, 166, "M", th->text, FONT_SMALL);
  }
}

/* ---- FX: the echoes */
static void draw_fx(void) {
  rect(0, BODY_Y, SCREEN_W, BODY_H, th->bg);
  text(8, 30, "DELAY", th->dim, FONT_SMALL);
  for (int s = 0; s < STEPS; s++) {
    rect(8 + s * 14, 172, 12, 3, (s & 3) ? th->off : th->beat);
  }
  int h = 120, x = 8;
  rect(x, 170 - h, 10, h, th->text);
  h = h * 3 / 4;
  for (int s = song.dtime; s < STEPS && h > 1; s += song.dtime) {
    rect(8 + s * 14, 170 - h, 10, h, th->accent);
    h = h * song.dfdbk * 230 >> 15;
  }
}

/* ---- GLOBAL: the tempo, the theme, the load */
static void draw_cpu(void) { /* how much of the time the sound takes (on the FM-1: the emulator's clock is not real) */
  rect(120, 30, 116, 32, th->bg);
#ifdef __wasm__
  text(120, 30, "CPU: ON FM-1", th->dim, FONT_SMALL);
#else
  char s[24] = "CPU ";
  format_int(s + 4, cpu_load);
  int i = len(s);
  s[i++] = '%', s[i] = 0;
  text(120, 30, s, cpu_peak > 85 ? RGB(255, 60, 60) : th->text, FONT_SMALL);
  char p[24] = "PEAK ";
  format_int(p + 5, cpu_peak);
  i = len(p);
  p[i++] = '%', p[i] = 0;
  text(120, 46, p, cpu_peak > 85 ? RGB(255, 60, 60) : th->dim, FONT_SMALL);
#endif
}

static void draw_global(void) {
  char s[16];
  rect(0, BODY_Y, SCREEN_W, BODY_H, th->bg);
  format_int(s, song.bpm);
  text(8, 30, s, th->accent, FONT_BIG);
  text(8 + text_width(s, FONT_BIG) + 6, 46, "BPM", th->dim, FONT_SMALL);
  /* the theme: the screen is already in it (turning THEME recolours everything), so its name and colours */
  text(8, 84, "THEME", th->dim, FONT_SMALL);
  text(8, 100, th->name, th->accent, FONT_BIG);
  for (int k = 0; k < TRACKS; k++) {
    rect(8 + k * 25, 142, 22, 14, th->track[k]);
  }
  draw_tabs();
  draw_cpu();
}

static void draw_body(void) {
  playhead_drawn = -1;
  rect(0, HEAD_H, SCREEN_W, KNOB_Y - HEAD_H, th->bg);
  if (page == PG_PATTERN) {
    for (int t = 0; t < TRACKS; t++) {
      draw_row(t);
    }
  } else if (page == PG_SOUND) {
    draw_sound();
  } else if (page == PG_MIX) {
    for (int t = 0; t < TRACKS; t++) {
      draw_strip(t);
    }
  } else if (page == PG_FX) {
    draw_fx();
  } else {
    draw_global();
  }
}

static void draw_knobs(void) {
  pick_knobs(knobs);
  for (int k = 0; k < 4; k++) {
    draw_knob(k);
  }
}

static void draw_all(void) {
  th = &THEMES[song.theme];
  clear(th->bg);
  draw_header();
  draw_body();
  draw_knobs();
}

/* ---- changing things */
static int dirty, save_pending;
static uint32_t ask_until; /* a question is up (clear the pattern?) until then: OCT+ yes, OCT- no */

static void set_page(int p) {
  if (p != page) {
    page = p;
    sub = 0;
    draw_all();
  }
}

static void set_track(int t) {
  if (t == track || t < 0 || t >= TRACKS) {
    return;
  }
  int old = track;
  track = t;
  draw_track_chip();
  if (page == PG_PATTERN) { /* the cursor moves with it */
    draw_row(old);
    draw_row(t);
  } else if (page == PG_MIX) {
    draw_label(old);
    draw_label(t);
  } else if (page == PG_SOUND) {
    draw_sound();
  }
  draw_knobs();
}

static void changed(int id) { /* a parameter changed: show it where it shows (and a MIDI track: send it) */
  dirty = 1;
  const track_t *d = &song.track[track];
  if (id >= P_CC0 && id <= P_CC3) { /* a controller, as it turns */
    midi_out(0xB0 | (d->mch & 15), d->ccn[id - P_CC0], d->cc[id - P_CC0]);
  } else if (id == P_PROG && d->prog) {
    midi_out(0xC0 | (d->mch & 15), d->prog - 1, 0);
  } else if (id >= P_CCN0 && id <= P_CCN3) { /* its value knob is named by it */
    for (int k = 0; k < 4; k++) {
      if (knobs[k] == id - P_CCN0 + P_CC0) {
        draw_knob(k);
      }
    }
  }
  if (id == P_THEME) {
    draw_all();
    return;
  }
  if (id == P_BPM || id == P_SWING || id == P_DTIME) {
    engine_tempo();
  }
  if (id == P_BPM) {
    draw_transport();
  }
  if (id == P_MACHINE || id == P_VOICE || id == P_STVOICE) {
    draw_track_chip();
  }
  if (id == P_MACHINE && page == PG_SOUND) { /* another kind of machine: other pages of knobs */
    sub = sub < subpages() ? sub : 0;
  }
  if (page == PG_PATTERN) {
    if (id == P_LENGTH || id == P_MUTE) {
      for (int t = 0; t < TRACKS; t++) {
        draw_row(t);
      }
    } else {
      draw_row(track);
    }
  } else if (page == PG_MIX) {
    draw_strip(track);
  } else if (page == PG_SOUND || page == PG_FX || page == PG_GLOBAL) {
    if (id != P_LEVEL && id != P_SEND && id != P_DRIVE) {
      draw_body();
    }
  }
  if (id == P_STEP || id == P_MACHINE || id == P_VOICE) { /* the other knobs show this step, this machine */
    draw_knobs();
  } else {
    for (int k = 0; k < 4; k++) {
      if (knobs[k] == id) {
        draw_knob(k);
      }
    }
  }
}

/* detents turned this pass -> steps: a slow turn is 1 a detent, a fast spin up to 4 */
static int accel(int e, int32_t detents) {
  static uint32_t last[ENC_COUNT];
  uint32_t now = millis(), dt = now - last[e];
  if (!detents) {
    return 0;
  }
  last[e] = now;
  return detents * (dt < 25 ? 4 : dt < 60 ? 2
                                          : 1);
}

static void turn(int id, int e) {
  int d = encoder(e);
  if (!d || id == P_NONE) {
    return;
  }
  int lo = plo(id), hi = phi(id), v = pget(id);
  if (id == P_VOICE || id == P_STVOICE) {                                            /* one voice a detent, past the ones this machine has none of */
    int m = machine_of(track), n = v < 0 && d > 0 ? song.track[track].voice - 1 : v; /* from a rest: the track's voice */
    lo = id == P_STVOICE ? -1 : 0;
    do {
      n += d > 0 ? 1 : -1;
    } while (n >= 0 && n <= hi && !machine_has(m, n));
    if (n >= lo && n <= hi && n != v) {
      pset(id, n);
      changed(id);
      if (n >= 0 && !is_synth(track)) {
        live_on(track, n, 100);
      }
    }
    return;
  }
  d = hi - lo > 20 ? accel(e, d) : d;
  int n = v + d < lo ? lo : v + d > hi ? hi
                                       : v + d;
  if (n != v) {
    pset(id, n);
    changed(id);
  }
}

/* ---- the keys: on PATTERN and MIX, white keys are steps / mutes and black keys tracks; else they play */
static int8_t white_of[KEY_COUNT], black_of[KEY_COUNT];

/* with REC: into the step playing, or (stopped) into the cursor's, which then moves on */
static void record(int t, int v, int vel) {
  int s = playing ? play_step : cursor;
  if (!rec || s < 0) {
    return;
  }
  pat()->val[t][s] = (uint8_t)v;
  pat()->flag[t][s] = (uint8_t)(STEP_ON | (vel > 110 ? STEP_ACC : 0));
  if (!playing) {
    cursor = (cursor + 1) % song.length;
    draw_knobs();
  }
  if (page == PG_PATTERN) {
    draw_row(t);
  }
  dirty = 1;
}

/* a drum voice or a synth note, now; it is also what new steps get */
static void play(int t, int v, int vel) {
  live_on(t, v, vel);
  if (song.track[t].voice != v) {
    song.track[t].voice = (uint8_t)v;
    if (t == track) {
      draw_track_chip();
    }
  }
  record(t, v, vel);
}

/* the drum keys: up the white keys the usual kit, the black keys the rest; SMP: a sample a key */
static int key_voice(int t, int k) {
  static const int8_t ON_WHITE[16] = {0, 2, 9, 10, 5, 4, 6, 7, 8, 11, 12, 13, 15, 20, 21, 19};
  static const int8_t ON_BLACK[11] = {1, 3, 14, 16, 17, 18, -1, -1, -1, -1, -1};
  int m = machine_of(t), v;
  if (machine_voices(m) != 22) {
    return k < machine_voices(m) ? k : -1;
  }
  v = white_of[k] >= 0 ? ON_WHITE[white_of[k]] : ON_BLACK[black_of[k]];
  return v >= 0 && machine_has(m, v) ? v : -1;
}

/* the keys edit steps on PATTERN (until REC: then they play, as everywhere) and mutes on MIX */
static int keys_edit(void) { return page == PG_MIX || (page == PG_PATTERN && !rec); }

static void keys_pattern(void) {
  for (int k = 0; k < KEY_COUNT; k++) {
    if (!key_pressed(k)) {
      continue;
    }
    int w = white_of[k], b = black_of[k];
    if (b >= 0 && b < TRACKS) {
      set_track(b);
      if (!is_synth(b)) {
        live_on(b, song.track[b].voice, 100);
      }
    } else if (w >= 0 && w < song.length) { /* a rest, or the track's voice (note) */
      pattern_t *p = pat();
      if (p->flag[track][w] & STEP_ON) {
        p->flag[track][w] = 0;
      } else {
        p->val[track][w] = song.track[track].voice;
        p->flag[track][w] = STEP_ON;
      }
      int old = cursor;
      cursor = w;
      draw_cell(track, old);
      draw_cell(track, w);
      draw_knobs();
      dirty = 1;
    }
  }
}

static void keys_mix(void) {
  for (int k = 0; k < KEY_COUNT; k++) {
    if (!key_pressed(k)) {
      continue;
    }
    int w = white_of[k], b = black_of[k];
    if (b >= 0 && b < TRACKS) {
      set_track(b);
    } else if (w >= 0 && w < TRACKS) {
      song.mute ^= (uint16_t)(1u << w);
      dirty = 1;
      draw_strip(w);
      for (int i = 0; i < 4; i++) {
        if (knobs[i] == P_MUTE) {
          draw_knob(i);
        }
      }
    }
  }
}

static void keys_play(void) {
  static uint8_t held[KEY_COUNT], held_t[KEY_COUNT];
  int base = KEY_FIRST_NOTE + 12 * octave;
  for (int k = 0; k < KEY_COUNT; k++) {
    if (key_pressed(k)) {
      if (is_synth(track)) {
        held[k] = (uint8_t)(base + k); /* a key stops the note it started, whatever the octave or track is now */
        held_t[k] = (uint8_t)track;
        play(track, held[k], 100);
      } else if (key_voice(track, k) >= 0) {
        play(track, key_voice(track, k), 100);
      }
    }
    if (key_released(k) && held[k]) {
      live_off(held_t[k], held[k]);
      held[k] = 0;
    }
  }
}

/* MIDI channels 1..9 play tracks 1..9, channel 10 the track chosen; a drum machine takes General MIDI
 * drum keys, a synth notes */
static void midi_in(void) {
  static const int8_t GM[47] = {1, 0, 4, 2, 5, 3, 6, 9, 6, 9, 7, 10, 7, 8, 11, 8, 12, 11, 12, 15, 11, 13, 11, -1, 12,
                                18, 17, 18, 18, 16, 19, 19, 13, 14, 20, 20, -1, -1, -1, -1, 21, 21, 21, -1, -1, -1, -1};
  uint8_t st, d1, d2;
  while (midi_read(&st, &d1, &d2)) {
    int ch = st & 0x0F, t = ch < TRACKS ? ch : ch == 9 ? track
                                                       : -1;
    int on = (st & 0xF0) == 0x90 && d2, off = (st & 0xF0) == 0x80 || ((st & 0xF0) == 0x90 && !d2);
    if (t < 0) {
      continue;
    }
    if (is_synth(t)) {
      if (on) {
        play(t, d1, d2);
      } else if (off) {
        live_off(t, d1);
      }
    } else if (on) {
      int v = d1 >= 35 && d1 <= 81 ? GM[d1 - 35] : -1;
      if (machine_voices(machine_of(t)) != 22) {
        v = d1 - 36; /* SMP: a sample a key from C */
      }
      if (v >= 0 && machine_has(machine_of(t), v)) {
        play(t, v, d2);
      }
    }
  }
}

/* ---- the song to start with */
/* "b...s..." -> a drum track's steps: a letter a voice, capitals accented, . a rest */
static void drum_steps(pattern_t *p, int t, const char *s) {
  static const char CODE[] = "b?s?rplmhcoyiw?tg?jeak"; /* BD . SD . RS CP LT MT HT CH OH CY rIde coWbell . Tamb conGa . ... */
  for (int i = 0; i < STEPS && s[i]; i++) {
    char c = s[i] >= 'A' && s[i] <= 'Z' ? (char)(s[i] + 32) : s[i];
    for (int v = 0; CODE[v]; v++) {
      if (CODE[v] == c) {
        p->val[t][i] = (uint8_t)v;
        p->flag[t][i] = (uint8_t)(STEP_ON | (c != s[i] ? STEP_ACC : 0));
      }
    }
  }
}

/* a synth track's steps: the notes, then "x" on, "a" accented, "s" sliding into the next */
static void note_steps(pattern_t *p, int t, const uint8_t *notes, const char *s) {
  for (int i = 0; i < STEPS && s[i]; i++) {
    p->val[t][i] = notes[i];
    p->flag[t][i] = (uint8_t)(s[i] == '.' ? 0 : STEP_ON | (s[i] == 'a' ? STEP_ACC : 0) | (s[i] == 's' ? STEP_SLIDE : 0));
  }
}

static void default_song(void) {
  /* a machine per track (a ROM one if it is built in, else the modelled one after it) */
  static const char *const MACHINE[TRACKS][2] = {{"909", "909"}, {"808", "808"}, {"909", "909"}, {"707", "CR78"}, {"DMX", "808"}, {"SDS", "SDS"}, {"606", "CR78"}, {"ANALOG", "ANALOG"}, {"ANALOG", "ANALOG"}};
  static const uint8_t VOICE[TRACKS] = {0, 5, 9, 15, 2, 6, 9, 69, 33};
  /* one pattern, as plain as a beat gets: a kick on the beat, a clap on 2 and 4, hats between, a one-note bass */
  static const char *const BEAT[3] = {"b...b...b...b...", "....p.......p...", "..c...c...c...c."};
  static const uint8_t BASS[STEPS] = {33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33};
  for (uint32_t i = 0; i < sizeof song; i++) {
    ((uint8_t *)&song)[i] = 0;
  }
  song.version = SONG_VERSION;
  song.bpm = 120;
  song.swing = 0;
  song.length = 16;
  song.dtime = 3;
  song.dfdbk = 60;
  song.drive = 24;
  for (int t = 0; t < TRACKS; t++) {
    track_t *d = &song.track[t];
    d->machine = (uint8_t)machine_find(MACHINE[t][0], machine_find(MACHINE[t][1], 0));
    d->voice = VOICE[t];
    d->tune = d->decay = d->tone = d->extra = 64;
    d->level = 100;
    d->send = 0;
    d->slide = 40;
    d->mch = (uint8_t)t; /* MIDI out (when the track is that): its own channel, the usual controllers */
    d->gate = 50;
    d->velo = 100;
    for (int k = 0; k < 4; k++) {
      static const uint8_t CCN[4] = {74, 71, 1, 10}, CC[4] = {64, 0, 0, 64};
      d->ccn[k] = CCN[k];
      d->cc[k] = CC[k];
    }
  }
  synth_load(7, 0); /* track 8: ANALOG's SAW LEAD, ready (no steps) */
  song.track[7].level = 70;
  for (int k = 0; k < synth_presets(machine_engine(song.track[8].machine)); k++) { /* track 9: ANALOG's SUB BASS */
    const char *n = synth_preset_name(machine_engine(song.track[8].machine), k);
    if (n[0] == 'S' && n[1] == 'U' && n[2] == 'B') {
      synth_load(8, k);
    }
  }
  song.track[8].level = 90;
  for (int t = 0; t < 3; t++) {
    drum_steps(&song.pat[0], t, BEAT[t]);
  }
  note_steps(&song.pat[0], 8, BASS, "x..x..x...x..x..");
}

static void sanitize(void) {
  for (int id = 0; id < PARAMS; id++) { /* each saved value back into its range */
    for (int t = 0; t < TRACKS; t++) {
      int was = track;
      track = t;
      uint8_t *b = pbyte(id);
      if (b && id != P_PRESET) {
        *b = (uint8_t)(*b < PARAM[id].lo ? PARAM[id].lo : *b > phi(id) ? phi(id)
                                                                       : *b);
      }
      track = was;
    }
  }
  song.pattern &= PATTERNS - 1;
  for (int t = 0; t < TRACKS; t++) { /* a synth engine's own */
    if (engine_of(t) >= 0) {
      for (int k = 0; k < 8; k++) {
        synth_param_t sp;
        int8_t *e = &song.track[t].e[k];
        synth_param(t, k, &sp);
        *e = (int8_t)(*e < sp.lo ? sp.lo : *e > sp.hi ? sp.hi
                                                      : *e);
      }
    }
  }
  for (int p = 0; p < PATTERNS; p++) { /* a voice this machine has no number for: a rest */
    for (int t = 0; t < TRACKS; t++) {
      for (int s = 0; s < STEPS; s++) {
        if (song.pat[p].val[t][s] >= machine_voices(machine_of(t))) {
          song.pat[p].flag[t][s] = 0;
        }
      }
    }
  }
}

void setup(void) {
  for (int k = 0, w = 0, b = 0; k < KEY_COUNT; k++) {
    static const uint8_t IS_BLACK[12] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0}; /* from F */
    white_of[k] = IS_BLACK[k % 12] ? -1 : (int8_t)w++;
    black_of[k] = IS_BLACK[k % 12] ? (int8_t)b++ : -1;
  }
  if (settings_load(&song, sizeof song) || song.version != SONG_VERSION) {
    default_song();
  }
  sanitize();
  engine_tempo();
  draw_all();
}

/* ---- the LEDs */
static void leds(void) {
  pattern_t *p = pat();
  uint16_t steps = 0;
  for (int s = 0; s < STEPS; s++) {
    steps |= (uint16_t)((p->flag[track][s] & STEP_ON) << s);
  }
  uint32_t held = keys();
  for (int k = 0; k < KEY_COUNT; k++) {
    int w = white_of[k], b = black_of[k], on;
    if (keys_edit()) {
      on = b >= 0           ? b == track
           : page == PG_MIX ? (w < TRACKS && !(song.mute >> w & 1))
                            : w < song.length && ((steps >> w & 1) ^ (w == play_step));
    } else {
      on = held >> k & 1;
    }
    led_key(k, on);
  }
  for (int i = 0; i < PAGES; i++) {
    led_button(PAGE_BTN[i], i == page);
  }
  led_button(BTN_ENV, ask_until != 0);
  led_button(BTN_ARP, page == PG_PATTERN && (pat()->flag[track][cursor] & (STEP_ON | STEP_RAT)) > STEP_ON);
  led_button(BTN_SEL, page == PG_PATTERN ? is_synth(track) && pget(P_STSLIDE) : subpages() > 1);
  led_button(BTN_PLAY, playing && (play_step & 3) != 3);
  led_button(BTN_REC, rec);
  led_button(BTN_SAVE, dirty);
  led_button(BTN_OCT_DOWN, ask_until ? 1 : octave < 0); /* both lit: they answer */
  led_button(BTN_OCT_UP, ask_until ? 1 : octave > 0);
}

void loop(void) {
  static int8_t step_drawn = -1;
  static uint8_t pattern_drawn, playing_drawn;
  static uint16_t hits_seen[TRACKS];
  static uint32_t stopped_at;
  uint32_t now = millis();

  /* the buttons */
  if (button_pressed(BTN_SEQ)) {
    set_page(PG_PATTERN);
  }
  if (button_pressed(BTN_EDIT)) {
    set_page(PG_SOUND);
  }
  if (button_pressed(BTN_HOME)) {
    set_page(PG_MIX);
  }
  if (button_pressed(BTN_FX)) {
    set_page(PG_FX);
  }
  if (button_pressed(BTN_GLO)) {
    set_page(PG_GLOBAL);
  }
  int answered = 0;                              /* an OCT press that answered a question does not change the octave */
  if (ask_until && button_pressed(BTN_OCT_UP)) { /* yes: clear the pattern */
    pattern_t *p = pat();
    for (uint32_t i = 0; i < sizeof p->flag; i++) {
      (&p->flag[0][0])[i] = 0;
    }
    ask_until = 0;
    answered = dirty = 1;
    toast("CLEARED");
    if (page == PG_PATTERN) {
      draw_body();
    }
    draw_knobs();
  } else if (ask_until && (button_pressed(BTN_OCT_DOWN) || (int32_t)(now - ask_until) >= 0)) { /* no */
    answered = button_pressed(BTN_OCT_DOWN);
    ask_until = 0;
    toast("KEPT");
  }
  if (button_pressed(BTN_ENV)) { /* clear the pattern? */
    ask_until = now + 4000;
    toast("CLEAR? OCT+");
    toast_until = ask_until;
  }
  if (button_pressed(BTN_ARP) && page == PG_PATTERN) { /* the cursor's step: its ratchet, x1 .. x8, round */
    uint8_t *f = &pat()->flag[track][cursor];
    int r = ((*f & STEP_RAT) >> RAT_SHIFT) + 1;
    r = r >= RATCHETS ? 0 : r;
    if (!(*f & STEP_ON)) { /* on a rest: the step comes on with the track's voice */
      pat()->val[track][cursor] = song.track[track].voice;
      *f |= STEP_ON;
      r = 1;
    }
    *f = (uint8_t)((*f & ~STEP_RAT) | r << RAT_SHIFT);
    char m[16] = "RATCHET X";
    format_int(m + 9, RATCHET[r]);
    toast(m);
    draw_cell(track, cursor);
    dirty = 1;
  }
  if (button_pressed(BTN_SEL) && page == PG_PATTERN && is_synth(track)) { /* slide the cursor's step into the next */
    int v = !pget(P_STSLIDE);
    pset(P_STSLIDE, v);
    changed(P_STSLIDE);
    toast(v ? "SLIDE ON" : "SLIDE OFF");
  }
  if (button_pressed(BTN_SEL) && subpages() > 1) {
    sub = (sub + 1) % subpages();
    draw_knobs();
    if (page == PG_SOUND) {
      draw_sound();
    } else if (page == PG_GLOBAL) {
      draw_global();
    }
  }
  if (button_pressed(BTN_PLAY)) {
    playing = !playing;
    if (!playing) {
      stopped_at = now;
    }
  }
  if (button_pressed(BTN_REC)) {
    rec = !rec;
    toast(rec ? "REC ON" : "REC OFF");
    draw_transport();
  }
  if (button_pressed(BTN_SAVE)) {
    if (playing) {
      save_pending = 1;
      toast("SAVE ON STOP");
    } else {
      save_pending = 1;
      stopped_at = now - 1000;
    }
  }
  int oct = ask_until || answered ? octave : octave + button_pressed(BTN_OCT_UP) - button_pressed(BTN_OCT_DOWN);
  if (oct != octave && oct >= -3 && oct <= 2) {
    char s[16] = "OCTAVE ";
    octave = oct;
    format_int(s + 7, octave);
    toast(s);
  }

  /* the encoders: the same on every page */
  int t = track + encoder(ENC_SELECT);
  set_track(t < 0 ? 0 : t >= TRACKS ? TRACKS - 1
                                    : t);
  int pt = song.pattern + encoder(ENC_PRESETS);
  if (pt != song.pattern && pt >= 0 && pt < PATTERNS) {
    char s[16] = "PATTERN ";
    song.pattern = (uint8_t)pt;
    format_int(s + 8, pt + 1);
    toast(s);
    dirty = 1;
    if (page == PG_PATTERN) {
      draw_body();
    }
    draw_knobs();
  }
  turn(P_BPM, ENC_ALGORITHM);
  for (int k = 0; k < 4; k++) {
    turn(knobs[k], ENC_KNOB1 + k);
  }

  /* the keys and MIDI */
  if (page == PG_PATTERN && !rec) {
    keys_pattern();
  } else if (page == PG_MIX) {
    keys_mix();
  } else {
    keys_play();
  }
  midi_in();

  /* what the sound side did */
  for (int i = 0; i < TRACKS; i++) {
    if (hits[i] != hits_seen[i]) {
      hits_seen[i] = hits[i];
      if (!flash_until[i] && (page == PG_PATTERN || page == PG_MIX)) {
        flash_until[i] = now + 70;
        draw_label(i);
      }
    } else if (flash_until[i] && (int32_t)(now - flash_until[i]) >= 0) {
      flash_until[i] = 0;
      if (page == PG_PATTERN || page == PG_MIX) {
        draw_label(i);
      }
    }
  }
  int8_t ps = play_step;
  if (ps != step_drawn) {
    step_drawn = ps;
    if (page == PG_PATTERN) {
      draw_playhead(ps);
    }
  }
  if (cur_pattern != pattern_drawn || playing != playing_drawn) {
    pattern_drawn = cur_pattern;
    playing_drawn = playing;
    draw_transport();
  }
  static uint32_t cpu_at;
  if (page == PG_GLOBAL && now - cpu_at > 500) {
    cpu_at = now;
    draw_cpu();
  }
  if (toast_until && (int32_t)(now - toast_until) >= 0) {
    toast_until = 0;
    draw_track_chip();
  }

  /* saving stalls the sound: only when stopped and quiet */
  if (save_pending && !playing && now - stopped_at > 600) {
    save_pending = 0;
    toast(settings_save(&song, sizeof song) ? "SAVE FAILED" : "SAVED");
    dirty = 0;
  }
  leds();
}
