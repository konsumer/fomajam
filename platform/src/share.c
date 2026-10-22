/* SPDX-License-Identifier: GPL-3.0-only */
/* Sharing (fm1.h share_get / share_put): a helper on USB (fm1buddy) reads the app's block and writes one back, in the
 * update's frames (F0 pack7(00 59 cmd len24 body ~sum) F7, ota.c). The helper asks, the FM-1 answers with the same
 * command; numbers are little-endian:
 *
 *   0x50 INFO                                 -> 01, size u32, chunk u16, name[16], version[8]
 *   0x51 READ   off u32, len u16              -> off u32, len u16, the bytes (len 0: past the end)
 *   0x52 WRITE  off u32, total u32, bytes     -> off u32, status   (off 0 starts over; a chunk sent again is fine)
 *   0x53 DONE   total u32, crc16 u16          -> status: share_put's, or SHARE_INCOMPLETE
 *
 * The crc is ota.c's (poly 0x1021, init 0) over the whole block. Nothing reaches the app before DONE, and after it
 * (whatever it said) a block starts over from 0. Included by
 * fm1.c after ota.c; ota_service() hands it the frames that aren't the update's (OTA_MORE). */
#define SHARE_CHUNK 512u
#define SHARE_MSG (7u + 6u + SHARE_CHUNK)

static uint8_t share_buf[SHARE_MAX] POOL;
static uint8_t share_msg[SHARE_MSG] POOL, share_wire[2u + (SHARE_MSG * 8u + 6u) / 7u] POOL;
static uint32_t share_total, share_got;

__attribute__((weak)) const void *share_get(uint32_t *n) {
  *n = 0;
  return share_buf;
}
__attribute__((weak)) int share_put(const void *p, uint32_t n) {
  (void)p, (void)n;
  return SHARE_BAD;
}

/* the reply: its body is already at share_msg + 6 */
static void share_send(uint32_t cmd, uint32_t n) {
  uint32_t i, s = 0, w;
  share_msg[0] = 0;
  share_msg[1] = 0x59;
  share_msg[2] = (uint8_t)cmd;
  share_msg[3] = (uint8_t)n;
  share_msg[4] = (uint8_t)(n >> 8);
  share_msg[5] = (uint8_t)(n >> 16);
  for (i = 0; i < n; i++) {
    s += share_msg[6 + i];
  }
  share_msg[6 + n] = (uint8_t)~s;
  share_wire[0] = 0xF0;
  w = 1u + ota_pack7(share_msg, 7u + n, share_wire + 1);
  share_wire[w++] = 0xF7;
  ota_wire_send(share_wire, w);
}

static void share_text(uint8_t *o, const char *s, uint32_t n) {
  uint32_t i = 0;
  for (; i < n && s[i]; i++) {
    o[i] = (uint8_t)s[i];
  }
  for (; i < n; i++) {
    o[i] = 0;
  }
}

/* a frame from the helper: cmd and its body (b, n bytes) */
static void share_frame(uint32_t cmd, const uint8_t *b, uint32_t n) {
  uint8_t *o = share_msg + 6;
  uint32_t size, off, len, total, st;
  const uint8_t *p;
  if (cmd == 0x50u) {
    share_get(&size);
    o[0] = 1;
    ota_wr32(o + 1, size);
    ota_wr16(o + 5, SHARE_CHUNK);
    share_text(o + 7, APP_NAME, 16);
    share_text(o + 23, APP_VER, 8);
    share_send(cmd, 31);
  } else if (cmd == 0x51u && n == 6u) {
    p = share_get(&size);
    off = ota_rd32(b);
    len = ota_rd16(b + 4);
    len = off >= size ? 0 : len > size - off ? size - off
                                             : len;
    len = len > SHARE_CHUNK ? SHARE_CHUNK : len;
    ota_wr32(o, off);
    ota_wr16(o + 4, len);
    memcpy(o + 6, p + off, len);
    share_send(cmd, 6u + len);
  } else if (cmd == 0x52u && n >= 8u) {
    off = ota_rd32(b);
    total = ota_rd32(b + 4);
    len = n - 8u;
    if (!off) {
      share_total = total;
      share_got = 0;
    }
    if (total > SHARE_MAX || len > SHARE_CHUNK) {
      st = SHARE_BAD;
    } else if (total != share_total || off > share_got || len > total - off || off > total) {
      st = SHARE_INCOMPLETE; /* not the one it started, or a gap: start over */
    } else {
      memcpy(share_buf + off, b + 8, len);
      share_got = off + len > share_got ? off + len : share_got;
      st = SHARE_OK;
    }
    ota_wr32(o, off);
    o[4] = (uint8_t)st;
    share_send(cmd, 5);
  } else if (cmd == 0x53u && n == 6u) {
    total = ota_rd32(b);
    if (total != share_total || share_got != total || ota_crc16(share_buf, total, 0) != ota_rd16(b + 4)) {
      st = SHARE_INCOMPLETE;
    } else {
      st = (uint32_t)share_put(share_buf, total);
    }
    share_total = share_got = 0; /* done either way: the next one starts at 0 */
    o[0] = (uint8_t)st;
    share_send(cmd, 1);
  }
}
