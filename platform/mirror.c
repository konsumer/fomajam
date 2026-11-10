/* SPDX-License-Identifier: GPL-3.0-only */
/* Big Screen: the screen over USB MIDI, for a viewer (fm1buddy, FM-1 Mirror), in bigscreen.h (foma's bigscreen/:
 * the protocol and both ends of it). Version 2, what a viewer asks for first (F0 7D 'B' 'S' 00 ...): only the 16 x 16
 * tiles that changed, compressed. Version 1 for viewers that only know it: pfkellogg's screen_mirror
 * (github.com/pfkellogg/fm1_open, screen_mirror/README.md), F0 7D 'F' 'S' 00 F7.
 * Nothing runs unless a viewer said hello in the last 3 s. Then what changed goes out from the framebuffer as it is
 * when it's sent, into usb.c's own ring for it, which goes out only between other MIDI: playing comes first.
 * Included by fm1.c after src/usb.c and common.c (usb.c's sysex_byte keeps the hello in usb.mirror_msg). */
#define BS_MAX_TILES ((SCREEN_W / 16) * (SCREEN_H / 16))
#define BS_MAX_W SCREEN_W
#define BS_MSG 128 /* (a note waits for the end of one: 43 packets, ~1.3 ms) */
#define BS_NO_VIEWER
#define BS_SWAPPED 1 /* (fb is stored byte-swapped for the panel) */
#define BIGSCREEN_IMPLEMENTATION
#include "bigscreen.h"

static bs_dev mr;

/* screen_flush: rows y0..y1 (exclusive) changed */
static void mirror_rows(int y0, int y1) {
  bs_dev_changed(&mr, 0, y0, SCREEN_W, y1);
}

/* the main loop: a hello, and what's waiting to go out */
static void mirror_service(void) {
  const uint8_t* m;
  uint32_t n, i;
  if (!mr.fb) {
    bs_dev_init(&mr, fb, SCREEN_W, SCREEN_H, SCREEN_W, 1); /* (stored byte-swapped for the panel) */
  }
  if (usb.mirror_len) {
    uint8_t h[sizeof usb.mirror_msg];
    n = usb.mirror_len;
    for (i = 0; i < n; i++) {
      h[i] = usb.mirror_msg[i];
    }
    usb.mirror_len = 0;
    bs_dev_sysex(&mr, h, n, fm1_ms);
  }
  if (!bs_dev_active(&mr)) {
    return;
  }
  if (!usb.config) {
    bs_dev_init(&mr, fb, SCREEN_W, SCREEN_H, SCREEN_W, 1); /* (unplugged: nobody's looking) */
    return;
  }
  while (MRQ - (mq_w - mq_r) >= (BS_MSG + 2u) / 3u && (n = bs_dev_next(&mr, fm1_ms, &m))) {
    for (i = 0; i < n; i += 3u) { /* as ota_wire_send packs it: CIN 4 continues, 5/6/7 end */
      uint32_t k = n - i >= 3u ? 3u : n - i, cin = k == 3u && i + 3u < n ? 4u : 4u + k;
      mr_q[mq_w % MRQ] = cin | (uint32_t)m[i] << 8 | (k > 1u ? (uint32_t)m[i + 1] << 16 : 0u) |
                         (k > 2u ? (uint32_t)m[i + 2] << 24 : 0u);
      RING_PUBLISH();
      mq_w++;
    }
  }
}
