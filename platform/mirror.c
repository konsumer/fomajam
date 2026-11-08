/* SPDX-License-Identifier: GPL-3.0-only */
/* Big Screen: the screen over USB MIDI, for a viewer (fm1buddy, FM-1 Mirror), in pfkellogg's screen_mirror
 * protocol (github.com/pfkellogg/fm1_open, screen_mirror/README.md):
 *   in:  F0 7D 'F' 'S' 00 F7, about once a second (01: and the whole screen again; 02: only the sound's name)
 *   out: F0 7D 'F' 'S' 01 row_lo row_hi col_lo col_hi {count c0-6 c7-13 c14-15}... F7, 70 bytes at most
 * Nothing runs unless a viewer asked in the last 3 s. Then the rows that changed go out from the framebuffer as
 * it is when they're sent, into usb.c's own ring for them, which goes out only between other MIDI: playing comes
 * first.
 * Included by fm1.c after src/usb.c and common.c (usb.c's sysex_byte sets usb.mirror_hello). */
#define MR_TIMEOUT 3000u
#define MR_MSG 70u

static uint8_t mr_rows[SCREEN_H / 8]; /* rows changed since they last went out */
static uint32_t mr_heard;             /* fm1_ms at the last hello */
static uint8_t mr_live;
static int mr_row = -1, mr_x;      /* the row going out, and where in it */
static int mr_last = SCREEN_H - 1; /* the last row that went out: the next search starts after it */

/* screen_flush: rows y0..y1 (exclusive) changed */
static void mirror_rows(int y0, int y1) {
  if (!mr_live) {
    return;
  }
  for (; y0 < y1; y0++) {
    mr_rows[y0 >> 3] |= (uint8_t)(1u << (y0 & 7));
  }
}

static int mr_next(void) {
  for (int i = 1; i <= SCREEN_H; i++) {
    int y = (mr_last + i) % SCREEN_H;
    if (mr_rows[y >> 3] & 1u << (y & 7)) {
      mr_rows[y >> 3] &= (uint8_t) ~(1u << (y & 7));
      return y;
    }
  }
  return -1;
}

/* one message of the row going out, from mr_x: false if there's no room for it now */
static int mr_send(void) {
  const uint16_t *px = fb + mr_row * SCREEN_W;
  uint8_t m[MR_MSG];
  uint32_t k = 9, x = (uint32_t)mr_x, i;
  if (MRQ - (mq_w - mq_r) < (MR_MSG + 2u) / 3u) {
    return 0;
  }
  m[0] = 0xF0, m[1] = 0x7D, m[2] = 'F', m[3] = 'S', m[4] = 1;
  m[5] = (uint8_t)(mr_row & 127), m[6] = (uint8_t)(mr_row >> 7);
  m[7] = (uint8_t)(x & 127u), m[8] = (uint8_t)(x >> 7);
  while (x < SCREEN_W && k + 5u <= MR_MSG) {
    uint32_t run = 1, c = fb_swap(px[x]); /* (stored byte-swapped for the panel) */
    while (x + run < SCREEN_W && run < 127u && px[x + run] == px[x]) {
      run++;
    }
    m[k] = (uint8_t)run, m[k + 1] = (uint8_t)(c & 127u), m[k + 2] = (uint8_t)(c >> 7 & 127u), m[k + 3] = (uint8_t)(c >> 14);
    k += 4u;
    x += run;
  }
  m[k++] = 0xF7;
  for (i = 0; i < k; i += 3u) { /* as ota_wire_send packs it: CIN 4 continues, 5/6/7 end */
    uint32_t n = k - i >= 3u ? 3u : k - i, cin = n == 3u && i + 3u < k ? 4u : 4u + n;
    mr_q[mq_w % MRQ] = cin | (uint32_t)m[i] << 8 | (n > 1u ? (uint32_t)m[i + 1] << 16 : 0u) |
                           (n > 2u ? (uint32_t)m[i + 2] << 24 : 0u);
    RING_PUBLISH();
    mq_w++;
  }
  mr_x = (int)x;
  if (x >= SCREEN_W) {
    mr_last = mr_row;
    mr_row = -1;
  }
  return 1;
}

/* the main loop: a hello, and what's waiting to go out */
static void mirror_service(void) {
  uint8_t h = usb.mirror_hello;
  if (h) {
    usb.mirror_hello = 0;
    if (h != 3u) { /* (02: the sound's name only, which this doesn't send) */
      if (!mr_live || h == 2u) {
        for (uint32_t i = 0; i < sizeof mr_rows; i++) {
          mr_rows[i] = 0xFF; /* a new viewer, or it asked: all of it */
        }
      }
      mr_live = 1;
      mr_heard = fm1_ms;
    }
  }
  if (!mr_live) {
    return;
  }
  if (fm1_ms - mr_heard >= MR_TIMEOUT || !usb.config) {
    mr_live = 0;
    mr_row = -1;
    for (uint32_t i = 0; i < sizeof mr_rows; i++) {
      mr_rows[i] = 0;
    }
    return;
  }
  for (;;) {
    if (mr_row < 0) {
      if ((mr_row = mr_next()) < 0) {
        return;
      }
      mr_x = 0;
    }
    if (!mr_send()) {
      return;
    }
  }
}
