/* Render FOMAJAM's sound on your computer, to check it by ear and by numbers.
 *
 *   ./fm1 build    (once, to create build/gen/)
 *   cc -O2 -Isrc -Iplatform -Ibuild/gen -Ivendor/felucca-engines/src -Ivendor/felucca -D__wasm__ \
 *      -o build/render test/render.c src/{drums,engine,fx,main,synths}.c -lm
 *   build/render --check            every voice of every drum machine sounds, stays in range and dies away
 *   build/render --synths [DIR]     the same checks for every synth preset (and writes a .wav of each to DIR)
 *   build/render --calibrate        rewrites src/gains.h, bringing each modeled voice to its TARGET peak
 *   build/render --wav DIR          writes a .wav of every machine and voice
 *   build/render --song FILE [N]    8 seconds of the song the firmware starts with (pattern N, 1-40)
 *   build/render --share            the song and samples read and written as fm1buddy does it (platform/src/share.c)
 *   build/render --bank FILE        a sample bank (as fm1buddy and the page make them) played as USER
 *   build/render --midi             the MIDI messages a MIDI track and the clock send in one bar, with times
 *
 * The panel and screen are stubbed out here, and sine() and note_inc() use floating point (the FM-1 doesn't). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drums.h"
#include "groove.h"

/* ---- the platform, as stubs */
int32_t sine(uint32_t ph) { return (int32_t)(sin(ph / 4294967296.0 * 2 * M_PI) * 32767); }
uint32_t note_inc(int n) { return (uint32_t)(440.0 * pow(2, (n - 69) / 12.0) / 44100 * 4294967296.0); }
uint32_t millis(void) { return 0; }
uint32_t micros(void) { return 0; }
uint32_t random32(void) {
  static uint32_t x = 2463534242u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}
void clear(uint16_t c) {}
void pixel(int x, int y, uint16_t c) {}
void rect(int x, int y, int w, int h, uint16_t c) {}
void frame(int x, int y, int w, int h, uint16_t c) {}
int text(int x, int y, const char *s, uint16_t c, int f) { return x; }
int text_width(const char *s, int f) { return 0; }
int text_height(int f) { return 16; }
int settings_load(void *p, uint32_t n) { return 1; }
int settings_save(const void *p, uint32_t n) { return 0; }
void format_int(char *b, int32_t v) { sprintf(b, "%d", v); }
int32_t encoder(int e) { return 0; }
int key_pressed(int k) { return 0; }
int key_released(int k) { return 0; }
int button_pressed(int b) { return 0; }
uint32_t keys(void) { return 0; }
void led_key(int k, int o) {}
void led_button(int b, int o) {}
void led_backlight(int l) {}
void led_key_color(int k, uint16_t c) {}
int key_velocity(int k) { return 100; }
int pitch_wheel(void) { return 0; }
int mod_wheel(void) { return 0; }
int fader(int i) { return -1; }
/* the platform's MIDI clock at the song's tempo, a block before each audio() as on the FM-1 */
#define CLOCK_MUL ((uint64_t)44100u * 4294967296u / 1000000u)
#include "clock.c"
const int32_t *audio_in(void) { return 0; }
static void block(int32_t *b) {
  clock_tempo(song.bpm * 100u);
  clock_send(song.clock);
  clock_block(256, 0);
  audio(b, 256);
}
int midi_read(uint8_t *a, uint8_t *b, uint8_t *c) { return 0; }
static int midi_log;
static uint32_t frame_now;
void midi_send(uint8_t a, uint8_t b, uint8_t c) {
  if (midi_log) {
    printf("%6.1f ms  %02X %3d %3d\n", frame_now / 44.1, a, b, c);
  }
}

static void wav(const char *path, const int16_t *x, uint32_t n) {
  FILE *f = fopen(path, "wb");
  uint32_t h[11] = {0x46464952, 36 + n * 2, 0x45564157, 0x20746d66, 16, 0x00010001, 44100, 88200, 0x00100002, 0x61746164, n * 2};
  fwrite(h, 4, 11, f);
  fwrite(x, 2, n, f);
  fclose(f);
}

/* ---- sharing (platform/src/share.c, through ota.c's frames): a helper's frames in, the replies out */
#define APP_NAME "FOMAJAM"
#define APP_VER "0.1"
#define FELUCCA_ID "FM-1_961"
static void share_frame(uint32_t cmd, const uint8_t *b, uint32_t n);
#define OTA_MORE share_frame
static const uint8_t *sh_in;
static uint32_t sh_in_n;
static uint8_t sh_out[1024];
static uint32_t sh_out_n;
static int ota_wire_send(const uint8_t *p, uint32_t n) {
  memcpy(sh_out, p, n);
  sh_out_n = n;
  return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n) {
  *p = sh_in, *n = sh_in_n;
  return sh_in_n > 0;
}
static void ota_frame_done(void) { sh_in_n = 0; }
static uint32_t ota_now_ms(void) { return 0; }
static void ota_idle(void) {}
static int ota_erase(uint32_t off) { return -1; }
static int ota_prog(uint32_t off, const void *p, uint32_t n) { return -1; }
static int ota_fread(uint32_t off, void *p, uint32_t n) { return -1; }
static void ota_show(uint32_t step, int32_t code) {}
static void ota_commit(const uint8_t *parm) {}
#include "src/ota.c"
/* the store: flash as memory, as the emulator's */
static uint8_t flash[STORE_MAX + 4096];
static const uint8_t *store_mem(void) { return flash; }
static int store_erase(uint32_t off) {
  memset(flash + off, 0xFF, 4096);
  return 0;
}
static int store_prog(uint32_t off, const uint8_t *p, uint32_t n) {
  for (uint32_t i = 0; i < n; i++) {
    flash[off + i] &= p[i];
  }
  return 0;
}
#include "src/share.c"

/* cmd for block k with body b (n bytes) to the FM-1, as the helper frames it; the reply's body into r, its length
 * (-1: none) */
static int share_ask(uint32_t cmd, int k, const uint8_t *b, uint32_t n, uint8_t *r) {
  static uint8_t m[1024], w[1200], d[1024];
  uint32_t s = k;
  n++;
  m[0] = 0, m[1] = 0x59, m[2] = (uint8_t)cmd, m[3] = (uint8_t)n, m[4] = (uint8_t)(n >> 8), m[5] = (uint8_t)(n >> 16);
  m[6] = (uint8_t)k;
  for (uint32_t i = 1; i < n; i++) {
    s += m[6 + i] = b[i - 1];
  }
  m[6 + n] = (uint8_t)~s;
  sh_in = w, sh_in_n = ota_pack7(m, 7 + n, w);
  sh_out_n = 0;
  ota_service();
  if (sh_out_n < 2 || sh_out[0] != 0xF0 || sh_out[sh_out_n - 1] != 0xF7) {
    return -1;
  }
  uint32_t got = ota_unpack7(sh_out + 1, sh_out_n - 2, d, sizeof d);
  if (got < 7 || d[2] != cmd || ota_rd24(d + 3) + 7 != got) {
    return -1;
  }
  memcpy(r, d + 6, got - 7);
  return (int)(got - 7);
}

/* block k written to the FM-1 in 512-byte pieces (each one twice: a reply lost on the way); DONE's status */
static int share_write(int k, const uint8_t *p, uint32_t n, int crc_off) {
  uint8_t b[600], r[600];
  for (uint32_t off = 0; off < n; off += 512) {
    uint32_t len = n - off < 512 ? n - off : 512;
    ota_wr32(b, off), ota_wr32(b + 4, n);
    memcpy(b + 8, p + off, len);
    for (int again = 0; again < 2; again++) {
      if (share_ask(0x52, k, b, 8 + len, r) != 5 || ota_rd32(r) != off) {
        return -1;
      }
      if (r[4] != SHARE_OK) {
        return r[4];
      }
    }
  }
  ota_wr32(b, n), ota_wr16(b + 4, ota_crc16(p, n, 0) + crc_off);
  return share_ask(0x53, k, b, 6, r) == 1 ? r[0] : -1;
}

/* block k read from the FM-1 into out; its size, or -1 */
static int share_read(int k, uint8_t *out) {
  uint8_t r[600], q[6];
  if (share_ask(0x50, k, 0, 0, r) != 36) {
    return -1;
  }
  uint32_t size = ota_rd32(r + 1);
  for (uint32_t off = 0; off < size; off += 512) {
    uint32_t want = size - off < 512 ? size - off : 512;
    ota_wr32(q, off), ota_wr16(q + 4, 512);
    if (share_ask(0x51, k, q, 6, r) != (int)(6 + want) || ota_rd32(r) != off || ota_rd16(r + 4) != want) {
      return -1;
    }
    memcpy(out + off, r + 6, want);
  }
  return (int)size;
}

/* a bank as the helpers make them: MINE (KICK, HAT: one a key) and DRUMS (a drum kit: BD, CH) */
static uint32_t make_bank(uint8_t *b) {
  uint32_t n = 8 + 2 * 12 + 4 * 20, lens[4] = {4000, 2000, 3000, 1000};
  static const char *const NAME[4] = {"KICK", "HAT", "", ""};
  memset(b, 0, n);
  memcpy(b, "FJB1", 4);
  b[4] = 2, b[5] = 4;
  memcpy(b + 8, "MINE", 4), b[16] = 0, b[17] = 2, b[18] = 0;
  memcpy(b + 20, "DRUMS", 5), b[28] = 2, b[29] = 2, b[30] = 1;
  for (int i = 0; i < 4; i++) {
    uint8_t *e = b + 32 + 20 * i;
    memcpy(e, NAME[i], strlen(NAME[i]));
    ota_wr32(e + 8, n), ota_wr32(e + 12, lens[i]);
    ota_wr16(e + 16, 22050);
    e[18] = 40;
    e[19] = i == 2 ? V_BD : i == 3 ? V_CH : 0;
    for (uint32_t k = 0; k < lens[i] / 2; k++) {
      b[n + k] = (uint8_t)(i & 1 ? random32() : (k & 8 ? 0x77 : 0xFF)); /* (codes up and down: a loud wave) */
    }
    n += lens[i] / 2;
  }
  return n;
}

/* the peak of voice v on machine m over 2048 frames */
static int32_t peak_of(int m, int v) {
  static int32_t o[2048];
  song.track[0].machine = (uint8_t)m, song.track[0].level = 100;
  song.track[0].tune = song.track[0].decay = song.track[0].tone = 64;
  memset(o, 0, sizeof o);
  drums_hit(0, v, 127);
  for (int i = 0; i < 2048; i += 256) {
    drums_render(0, o + i, 256);
  }
  int32_t peak = 0;
  for (int i = 0; i < 2048; i++) {
    peak = abs(o[i]) > peak ? abs(o[i]) : peak;
  }
  return peak;
}

static int share_test(void) {
  extern void setup(void);
  static uint8_t kept[sizeof(song_t)], r[600], bank[8192], back[8192];
  int bad = 0, k1 = machine_find("KIT1", -1);
#define CHECK(c, what) (c ? printf("ok   %s\n", what) : (printf("FAIL %s\n", what), bad = 1))
  setup();
  int k = share_ask(0x50, 0, 0, 0, r);
  CHECK(k == 36 && r[0] == 1 && ota_rd32(r + 1) == sizeof song && !strcmp((char *)r + 7, "FOMAJAM") && r[31] == 0,
        "INFO: FOMAJAM, a song's size");
  CHECK(share_read(0, kept) == sizeof song && !memcmp(kept, &song, sizeof song), "READ: the song, in pieces");

  song_t s;
  memcpy(&s, kept, sizeof s);
  s.bpm = 97, s.pat[39].flag[8][15] = STEP_ON;
  CHECK(share_write(0, (uint8_t *)&s, sizeof s, 0) == SHARE_OK && song.bpm == 97 && song.pat[39].flag[8][15] == STEP_ON,
        "WRITE: a changed song loads");
  CHECK(share_write(0, kept, sizeof song, 1) == SHARE_INCOMPLETE && song.bpm == 97, "WRITE: a wrong crc is refused");
  uint8_t q[8 + 512];
  ota_wr32(q, 1024), ota_wr32(q + 4, sizeof song);
  CHECK(share_ask(0x52, 0, q, 8 + 512, r) == 5 && r[4] == SHARE_INCOMPLETE, "WRITE: a gap is refused");
  kept[0] = 3;
  CHECK(share_write(0, kept, sizeof song, 0) == SHARE_BAD, "WRITE: a song version it doesn't know is refused");
  kept[0] = SONG_VERSION;
  memcpy(&s, kept, sizeof s);
  s.version = 8, s.bpm = 133;
  CHECK(share_write(0, (uint8_t *)&s, SONG8_SIZE, 0) == SHARE_OK && song.bpm == 133 && song.version == SONG_VERSION &&
            song.rxch[0] == 1,
        "WRITE: a version 8 song loads (with the MIDI in defaults)");
  playing = 1;
  CHECK(share_write(0, kept, sizeof song, 0) == SHARE_BUSY && song.bpm == 133, "WRITE: not while playing");
  playing = 0;
  CHECK(share_write(0, kept, sizeof song, 0) == SHARE_OK && !memcmp(kept, &song, sizeof song), "WRITE: the first one back");

  /* the store: the kits */
  k = share_ask(0x50, 1, 0, 0, r);
  CHECK(k == 36 && ota_rd32(r + 1) == 0 && r[31] == 1 && ota_rd32(r + 32) == STORE_MAX, "STORE: empty, 288 KiB");
  CHECK(k1 >= 0 && machine_empty(k1) && machine_count() == k1 + USER_KITS, "STORE: its four kit slots are empty");
  uint32_t n = make_bank(bank);
  CHECK(share_write(1, bank, n, 0) == SHARE_OK, "STORE: a bank written");
  CHECK(!strcmp(machine_name(k1), "MINE") && !strcmp(machine_label(k1), "MIN") && !machine_kit(k1) && machine_voices(k1) == 2 &&
            !strcmp(voice_name(k1, 0), "KICK") && !strcmp(voice_name(k1, 1), "HAT"),
        "STORE: MINE plays KICK and HAT, one a key");
  CHECK(!strcmp(machine_name(k1 + 1), "DRUMS") && machine_kit(k1 + 1) && machine_voices(k1 + 1) == V_COUNT &&
            machine_has(k1 + 1, V_BD) == 1 && machine_has(k1 + 1, V_CH) == 1 && machine_has(k1 + 1, V_SD) == 0 &&
            machine_has(k1 + 1, V_BD2) == 2 && machine_empty(k1 + 2),
        "STORE: DRUMS is a drum kit (BD2 played by BD), then two empty slots");
  CHECK(share_read(1, back) == (int)n && !memcmp(back, bank, n), "STORE: read back as it was written");
  CHECK(peak_of(k1, 0) > 1000 && peak_of(k1 + 1, V_BD) > 1000, "STORE: they sound");
  playing = 1;
  CHECK(share_write(1, bank, n, 0) == SHARE_BUSY && !machine_empty(k1), "STORE: not while playing (and kept)");
  playing = 0;
  CHECK(share_write(1, bank, n, 1) == SHARE_INCOMPLETE && machine_empty(k1), "STORE: a wrong crc leaves none");
  bank[4] = 9;
  bank[n - 1] ^= 1;
  CHECK(share_write(1, bank, n, 0) == SHARE_OK && machine_empty(k1), "STORE: a bank it can't read is no kits");
  bank[4] = 2;
  CHECK(share_write(1, bank, n, 0) == SHARE_OK && !machine_empty(k1 + 1), "STORE: and back");
  store_known = 0;
  drums_store();
  CHECK(!strcmp(machine_name(k1 + 1), "DRUMS"), "STORE: still there after a restart");
  return bad;
}

/* one hit of voice v on machine m, at the default knobs, full level: its peak and how long it rings */
#define LEN (44100 * 3)
static int32_t buf[LEN];

static void hit(int m, int v, int32_t *peak, uint32_t *tail, int bad[1]) {
  memset(&song, 0, sizeof song);
  song.track[0] = (track_t){(uint8_t)m, (uint8_t)v, 127, 0, 64, 64, 64, 64};
  drums_hit(0, v, 127);
  memset(buf, 0, sizeof buf);
  for (uint32_t i = 0; i < LEN; i += 256) {
    drums_render(0, buf + i, 256);
  }
  *peak = 0;
  *tail = 0;
  for (uint32_t i = 0; i < LEN; i++) {
    int32_t a = abs(buf[i]);
    if (a > 1000000) {
      bad[0] = 1;
    }
    if (a > *peak) {
      *peak = a;
    }
  }
  for (uint32_t i = 0; i < LEN; i++) {
    if (abs(buf[i]) > *peak / 1000) {
      *tail = i;
    }
  }
}

int main(int argc, char **argv) {
  const char *mode = argc > 1 ? argv[1] : "--check";
  if (!strcmp(mode, "--check") || !strcmp(mode, "--wav")) {
    int fails = 0;
    for (int m = 0; m < machine_count(); m++) {
      if (machine_synth(m)) {
        continue; /* a synth is not a drum: --synths plays it */
      }
      printf("%-5s", machine_name(m));
      for (int v = 0; v < machine_voices(m); v++) {
        if (machine_has(m, v) != 1) {
          continue;
        }
        int32_t peak;
        uint32_t tail;
        int bad = 0;
        hit(m, v, &peak, &tail, &bad);
        int32_t late = 0; /* the last half second: well below the peak (a cymbal may still ring) */
        for (uint32_t i = LEN - 44100 / 2; i < LEN; i++) {
          late = abs(buf[i]) > late ? abs(buf[i]) : late;
        }
        int ok = !bad && peak > 2000 && late < peak / 30;
        fails += !ok;
        printf(" %s%s:%d/%dms", ok ? "" : "!", voice_name(m, v), peak, tail * 1000 / 44100);
        if (!strcmp(mode, "--wav") && argc > 2) {
          static int16_t o[LEN];
          char path[512];
          for (uint32_t i = 0; i < LEN; i++) {
            o[i] = (int16_t)(buf[i] > 32767 ? 32767 : buf[i] < -32767 ? -32767 : buf[i]);
          }
          snprintf(path, sizeof path, "%s/%s-%s.wav", argv[2], machine_name(m), voice_name(m, v));
          wav(path, o, tail + 4410 < LEN ? tail + 4410 : LEN);
        }
      }
      printf("\n");
    }
    printf("%s: %d voices failed (silent, out of range, or not fading out)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
  }
  if (!strcmp(mode, "--synths")) { /* a note held 0.6 s, then 7.4 s to die away (a pad's release is long) */
    int fails = 0;
#define SLEN (44100 * 8)
    static int32_t sb[SLEN];
    for (int m = 0; m < machine_count(); m++) {
      int e = machine_engine(m);
      if (e < 0) {
        continue;
      }
      printf("%-7s", machine_name(m));
      for (int pr = 0; pr < synth_presets(e); pr++) {
        memset(&song, 0, sizeof song);
        song.track[0] = (track_t){(uint8_t)m, 57, 100, 0, 64};
        synth_load(0, pr);
        memset(sb, 0, sizeof sb);
        int32_t peak = 0, late = 0, bad = 0;
        for (uint32_t i = 0; i < SLEN; i += 32) {
          if (i == 32) {
            synths_note(0, 57, 100, 0, 44100 * 6 / 10);
          }
          synths_render(0, sb + i, 32);
        }
        for (uint32_t i = 0; i < SLEN; i++) {
          int32_t a = abs(sb[i]);
          bad |= a > 1000000;
          peak = a > peak ? a : peak;
          if (i > SLEN - 44100 / 2) {
            late = a > late ? a : late;
          }
        }
        int ok = !bad && peak > 1000 && late < peak / 30;
        fails += !ok;
        printf(" %s%s:%d", ok ? "" : "!", synth_preset_name(e, pr), peak);
        if (argc > 2) {
          static int16_t o[SLEN];
          char path[512];
          for (uint32_t i = 0; i < SLEN; i++) {
            o[i] = (int16_t)(sb[i] > 32767 ? 32767 : sb[i] < -32767 ? -32767 : sb[i]);
          }
          snprintf(path, sizeof path, "%s/%s-%02d.wav", argv[2], machine_name(m), pr);
          wav(path, o, SLEN);
        }
      }
      printf("\n");
    }
    printf("%s: %d presets failed\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
  }
  if (!strcmp(mode, "--midi")) { /* track 1 to MIDI on channel 3: C3, E3 (accent, ratchet x3), G3 sliding into C4 (ratchet x4) */
    extern void setup(void);
    static int32_t b[512];
    setup();
    memset(song.pat, 0, sizeof song.pat);
    song.track[0].machine = (uint8_t)(machine_count() - 1);
    song.track[0].mch = 2;
    song.track[0].prog = 6; /* program 5 */
    song.clock = 1;
    song.length = 4;
    static const uint8_t NOTE[4] = {48, 52, 55, 60}, FLAG[4] = {STEP_ON, STEP_ON | STEP_ACC | 2 << RAT_SHIFT, STEP_ON | STEP_SLIDE, STEP_ON | 3 << RAT_SHIFT}; /* E3: x3, C4: x4 */
    for (int i = 0; i < 4; i++) {
      song.pat[0].val[0][i] = NOTE[i];
      song.pat[0].flag[0][i] = FLAG[i];
    }
    engine_tempo();
    printf("machine %s, 122 BPM: a 16th is %.1f ms\n", machine_name(song.track[0].machine), 60000.0 / 122 / 4);
    midi_log = 1;
    clock_start();
    for (frame_now = 0; frame_now < 44100 * 6 / 10; frame_now += 256) {
      memset(b, 0, sizeof b);
      block(b);
    }
    clock_stop();
    block(b);
    return 0;
  }
  if (!strcmp(mode, "--calibrate")) {
    extern const int16_t *gain_table(void);
    FILE *f = fopen("src/gains.h", "w");
    fprintf(f, "/* made by test/render.c --calibrate: do not edit. Each modeled voice's gain, Q15, so that at the\n"
               " * default knobs it peaks at TARGET (drums.h) */\n#ifndef GAINS_H\n#define GAINS_H\n#include \"drums.h\"\n"
               "static const int16_t GAIN[M_MODELLED][V_COUNT] = {\n");
    for (int m = 0; m < M_MODELLED; m++) {
      fprintf(f, "  {");
      for (int v = 0; v < V_COUNT; v++) {
        int32_t peak = 0, g = 32767;
        uint32_t tail;
        int bad = 0;
        if (PATCH[m][v].kind != K_NONE) {
          hit(m, v, &peak, &tail, &bad);
          double now = gain_table()[m * V_COUNT + v] / 32768.0, raw = peak / (now * 127 * 127 * 2 / 32768.0);
          g = (int32_t)(TARGET[v] / raw * 32768);
          g = g > 32767 ? 32767 : g;
        }
        fprintf(f, "%d%s", g, v + 1 < V_COUNT ? ", " : "");
      }
      fprintf(f, "},\n");
    }
    fprintf(f, "};\n#endif\n");
    fclose(f);
    printf("wrote src/gains.h: rebuild, then --check\n");
    return 0;
  }
  if (!strcmp(mode, "--song") && argc > 2) {
    extern void setup(void);
    static int16_t o[44100 * 8 * 2];
    static int32_t b[512];
    setup();
    if (argc > 3) {
      song.pattern = (uint8_t)(atoi(argv[3]) > 0 && atoi(argv[3]) <= PATTERNS ? atoi(argv[3]) - 1 : 0);
    }
    clock_start();
    long peak = 0, over = 0;
    for (uint32_t i = 0; i < 44100 * 8; i += 256) {
      memset(b, 0, sizeof b);
      block(b);
      for (int j = 0; j < 512; j++) {
        long a = labs(b[j]);
        peak = a > peak ? a : peak;
        over += a > 30000;
        o[i * 2 + j] = (int16_t)(b[j] > 32767 ? 32767 : b[j] < -32767 ? -32767 : b[j]);
      }
    }
    FILE *f = fopen(argv[2], "wb");
    uint32_t n = 44100 * 8;
    uint32_t h[11] = {0x46464952, 36 + n * 4, 0x45564157, 0x20746d66, 16, 0x00020001, 44100, 176400, 0x00100004, 0x61746164, n * 4};
    fwrite(h, 4, 11, f);
    fwrite(o, 2, n * 2, f);
    fclose(f);
    printf("%s: peak %ld, %ld samples near full scale; hits per track:", argv[2], peak, over);
    for (int t = 0; t < TRACKS; t++) {
      printf(" %d", hits[t]);
    }
    printf("\n");
    return 0;
  }
  if (!strcmp(mode, "--share")) {
    return share_test();
  }
  if (!strcmp(mode, "--bank") && argc > 2) { /* a bank file (as the helpers make it) in the store: USER's voices */
    FILE *f = fopen(argv[2], "rb");
    uint32_t n = f ? (uint32_t)fread(flash, 1, STORE_MAX, f) : 0;
    uint8_t r[16];
    if (f) {
      fclose(f);
    }
    ota_wr32(r, STORE_MAGIC), ota_wr32(r + 4, n), ota_wr32(r + 8, ota_crc16(flash, n, 0)), ota_wr32(r + 12, ~STORE_MAGIC);
    memcpy(flash + STORE_MAX, r, 16);
    drums_store();
    for (int m = machine_find("KIT1", machine_count() - USER_KITS); m < machine_count() && !machine_empty(m); m++) {
      printf("%s (%s):", machine_name(m), machine_kit(m) ? "a drum kit" : "one a key");
      for (int v = 0; v < machine_voices(m); v++) {
        if (machine_has(m, v) == 1) {
          printf(" %s peak %d", voice_name(m, v), peak_of(m, v));
        }
      }
      printf("\n");
    }
    return 0;
  }
  fprintf(stderr, "render --check | --calibrate | --wav DIR | --song FILE\n");
  return 2;
}
