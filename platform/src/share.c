/* SPDX-License-Identifier: GPL-3.0-only */
/* Sharing (fm1.h share_get / share_put, store_data): a helper on USB (fm1buddy, the page's Songs tab) reads the app's
 * blocks and writes them back, in the update's frames (F0 pack7(00 59 cmd len24 body ~sum) F7, ota.c). The helper
 * asks, the FM-1 answers with the same command; numbers are little-endian. Block 0 is the app's (share_get /
 * share_put: its song), block 1 the store (flash, store_data: its samples):
 *
 *   0x50 INFO   block                            -> 01, size u32, chunk u16, name[16], version[8], block, max u32
 *   0x51 READ   block, off u32, len u16          -> off u32, len u16, the bytes (len 0: past the end)
 *   0x52 WRITE  block, off u32, total u32, bytes -> off u32, status   (off 0 starts over; a piece sent again is fine)
 *   0x53 DONE   block, total u32, crc16 u16      -> status: share_put's (or the store's), or SHARE_INCOMPLETE
 *
 * The crc is ota.c's (poly 0x1021, init 0) over the whole block. A block's pieces come in order. Block 0 is kept in
 * RAM and reaches the app only at DONE. Block 1 goes straight to flash: its first piece asks the app (store_begin)
 * and forgets what was there, and only a DONE whose crc matches what's in flash makes it count (a record after it).
 * After DONE (whatever it said) a block starts over from 0; the same DONE again gets the same answer (its answer was
 * lost on the way).
 *
 * Included by fm1.c and web.c after ota.c (ota_service() hands it the frames that aren't the update's: OTA_MORE),
 * which give the store as STORE_MAX bytes and a 4 KiB sector for the record after them:
 *   store_mem()             all of it, readable as memory
 *   store_erase(off)        a 4 KiB sector of it, 0 = ok
 *   store_prog(off, p, n)   write n bytes (p in RAM) where it's erased, 0 = ok */
#define SHARE_CHUNK 512u
#define SHARE_MSG (7u + 6u + SHARE_CHUNK)
#define STORE_SECTOR 4096u
#define STORE_MAGIC 0x54534A46u /* "FJST" */

static uint8_t share_buf[SHARE_MAX] POOL;
static uint8_t share_msg[SHARE_MSG] POOL, share_wire[2u + (SHARE_MSG * 8u + 6u) / 7u] POOL;
static uint32_t share_total, share_got; /* the block coming in: its size, how much of it came */
static uint8_t share_block = 0xFF;      /* which (0xFF: none) */
static uint32_t store_size;             /* what's in the store, checked (0: nothing) */
static uint8_t store_known;             /* store_size is */
static uint32_t store_erased;           /* while writing it: erased up to here */
static uint8_t done_was[8];             /* the last DONE (block, total, crc) and its answer */

__attribute__((weak)) const void *share_get(uint32_t *n) {
  *n = 0;
  return share_buf;
}
__attribute__((weak)) int share_put(const void *p, uint32_t n) {
  (void)p, (void)n;
  return SHARE_BAD;
}
__attribute__((weak)) int store_begin(void) { return SHARE_OK; }
__attribute__((weak)) void store_done(void) {}

/* the store's record: after the data, written last */
static void store_check(void) {
  const uint8_t *r = store_mem() + STORE_MAX;
  uint32_t n = ota_rd32(r + 4);
  store_size = ota_rd32(r) == STORE_MAGIC && ota_rd32(r + 12) == ~STORE_MAGIC && n <= STORE_MAX &&
                       ota_crc16(store_mem(), n, 0) == ota_rd32(r + 8)
                   ? n
                   : 0;
  store_known = 1;
}

const uint8_t *store_data(uint32_t *n) {
  if (!store_known) {
    store_check();
  }
  *n = store_size;
  return store_size ? store_mem() : 0;
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

/* block k: its bytes and size */
static const uint8_t *share_of(uint32_t k, uint32_t *size) {
  if (!k) {
    return share_get(size);
  }
  store_data(size);
  return store_mem();
}

/* a piece of block 1: to flash, erasing ahead of it */
static uint32_t store_piece(uint32_t off, const uint8_t *p, uint32_t len) {
  if (!off) {
    uint32_t st = (uint32_t)store_begin();
    if (st != SHARE_OK) {
      return st;
    }
    store_size = 0; /* forgotten: the record first */
    store_known = 1;
    store_erased = 0;
    if (store_erase(STORE_MAX)) {
      store_done();
      return SHARE_FAILED;
    }
  }
  if (off + len <= share_got) {
    return SHARE_OK; /* sent again: it's there */
  }
  while (store_erased < off + len) {
    if (store_erase(store_erased)) {
      return SHARE_FAILED;
    }
    store_erased += STORE_SECTOR;
  }
  return store_prog(off, p, len) ? SHARE_FAILED : SHARE_OK;
}

/* DONE for block 1: the record, if what's in flash is what was meant */
static uint32_t store_finish(uint32_t total, uint32_t crc) {
  uint32_t st = SHARE_INCOMPLETE;
  if (ota_crc16(store_mem(), total, 0) == crc) {
    uint8_t r[16];
    ota_wr32(r, STORE_MAGIC);
    ota_wr32(r + 4, total);
    ota_wr32(r + 8, crc);
    ota_wr32(r + 12, ~STORE_MAGIC);
    st = store_prog(STORE_MAX, r, sizeof r) ? SHARE_FAILED : SHARE_OK;
  }
  store_check();
  store_done();
  return st;
}

/* a frame from the helper: cmd and its body (b, n bytes) */
static void share_frame(uint32_t cmd, const uint8_t *b, uint32_t n) {
  uint8_t *o = share_msg + 6;
  uint32_t k = n ? b[0] : 0xFF, size, off, len, total, st;
  const uint8_t *p;
  if (k > 1u) {
    return;
  }
  b++, n--;
  if (cmd == 0x50u && !n) {
    share_of(k, &size);
    o[0] = 1;
    ota_wr32(o + 1, size);
    ota_wr16(o + 5, SHARE_CHUNK);
    share_text(o + 7, APP_NAME, 16);
    share_text(o + 23, APP_VER, 8);
    o[31] = (uint8_t)k;
    ota_wr32(o + 32, k ? STORE_MAX : SHARE_MAX);
    share_send(cmd, 36);
  } else if (cmd == 0x51u && n == 6u) {
    p = share_of(k, &size);
    off = ota_rd32(b);
    len = ota_rd16(b + 4);
    len = off >= size ? 0 : len > size - off ? size - off : len;
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
      done_was[0] = 0;
      share_block = (uint8_t)k;
      share_total = total;
      share_got = 0;
    }
    if (total > (k ? STORE_MAX : SHARE_MAX) || len > SHARE_CHUNK) {
      st = SHARE_BAD;
    } else if (k != share_block || total != share_total || off > share_got || off > total || len > total - off) {
      st = SHARE_INCOMPLETE; /* not the one it started, or a gap: start over */
    } else if (k) {
      st = store_piece(off, b + 8, len);
    } else {
      memcpy(share_buf + off, b + 8, len);
      st = SHARE_OK;
    }
    if (st == SHARE_OK) {
      share_got = off + len > share_got ? off + len : share_got;
    } else if (!off) {
      share_block = 0xFF;
    }
    ota_wr32(o, off);
    o[4] = (uint8_t)st;
    share_send(cmd, 5);
  } else if (cmd == 0x53u && n == 6u) {
    total = ota_rd32(b);
    if (k != share_block && done_was[0] == k + 1u && !memcmp(done_was + 1, b, 6)) {
      st = done_was[7]; /* asked again: as before */
    } else if (k != share_block || total != share_total || share_got != total) {
      st = SHARE_INCOMPLETE;
      if (k == 1u && k == share_block) {
        store_check();
        store_done(); /* (it was forgotten: nothing) */
      }
    } else if (k) {
      st = store_finish(total, ota_rd16(b + 4));
    } else if (ota_crc16(share_buf, total, 0) != ota_rd16(b + 4)) {
      st = SHARE_INCOMPLETE;
    } else {
      st = (uint32_t)share_put(share_buf, total);
    }
    if (k == share_block) {
      done_was[0] = (uint8_t)(k + 1u);
      memcpy(done_was + 1, b, 6);
      done_was[7] = (uint8_t)st;
    }
    share_total = share_got = 0; /* done either way: the next one starts at 0 */
    share_block = 0xFF;
    o[0] = (uint8_t)st;
    share_send(cmd, 1);
  }
}
