// SPDX-License-Identifier: GPL-3.0-only
// Sharing (platform/src/share.c): the firmware's block (fomajam's song) read from the FM-1 or the emulator, and
// written back. The same messages fm1buddy sends, in M-VAVE's frames (F0 pack7(00 59 cmd len24 body ~sum) F7), each
// asked and answered with the same command:
//
//   0x50 INFO                              -> 01, size u32, chunk u16, name[16], version[8]
//   0x51 READ   off u32, len u16           -> off u32, len u16, the bytes
//   0x52 WRITE  off u32, total u32, bytes  -> off u32, status   (off 0 starts over; a piece sent again is fine)
//   0x53 DONE   total u32, crc16 u16       -> status
//
// A link is anything with send(bytes), read(ms) (the next SysEx, or null) and drain(): fm1ota.js's for the FM-1,
// EmuLink in app.js for the emulator.
import { pack7, unpack7 } from './fm1ota.js'

export const SHARE = { OK: 0, BUSY: 1, BAD: 2, INCOMPLETE: 3, FAILED: 4 }
const INFO = 0x50
const READ = 0x51
const WRITE = 0x52
const DONE = 0x53

const wr16 = (v) => [v & 0xff, (v >> 8) & 0xff]
const wr32 = (v) => [...wr16(v), ...wr16(v >>> 16)]
const rd16 = (b, i) => b[i] | (b[i + 1] << 8)
const rd32 = (b, i) => (rd16(b, i) | (rd16(b, i + 2) << 16)) >>> 0
const text = (b) => String.fromCharCode(...b).replace(/\0.*$/, '')

// errors carry a code the page translates: none, gone, busy, bad, incomplete, failed, unsaid
function fail(code) {
  const e = new Error(code)
  e.code = code
  return e
}

// ota.c's: poly 0x1021, from 0
export function crc16(data) {
  let c = 0
  for (const b of data) {
    c ^= b << 8
    for (let i = 0; i < 8; i++) c = c & 0x8000 ? (c << 1) ^ 0x1021 : c << 1
    c &= 0xffff
  }
  return c
}

export function frame(cmd, body = []) {
  const n = body.length
  const m = [0x00, 0x59, cmd, n & 0xff, (n >> 8) & 0xff, (n >> 16) & 0xff, ...body]
  let s = 0
  for (const b of body) s += b
  m.push(~s & 0xff)
  return [0xf0, ...pack7(m), 0xf7]
}

// the body of an answer to cmd, or null
export function unframe(pkt, cmd) {
  if (pkt.length < 10 || pkt[0] !== 0xf0 || pkt[pkt.length - 1] !== 0xf7) return null
  const d = unpack7(pkt.slice(1, -1))
  if (d.length < 7 || d[0] !== 0 || d[1] !== 0x59 || d[2] !== cmd) return null
  const n = d[3] | (d[4] << 8) | (d[5] << 16)
  if (n + 7 > d.length) return null
  let s = 0
  for (let i = 0; i < n; i++) s += d[6 + i]
  return (~s & 0xff) === d[6 + n] ? d.slice(6, 6 + n) : null
}

// cmd with its body, up to tries times: the answer's body that good() takes, or null
async function ask(link, cmd, body, good, tries = 3, ms = 1000) {
  const msg = frame(cmd, body)
  for (let t = 0; t < tries; t++) {
    link.drain()
    if (link.send(msg) === false) throw fail('gone')
    const end = Date.now() + ms
    while (Date.now() < end) {
      const p = await link.read(end - Date.now())
      if (!p) break
      const b = unframe(p, cmd)
      if (b && good(b)) return b
    }
  }
  return null
}

// what it shares: { size, chunk, name, version }, or null (it doesn't)
export async function info(link, tries = 3) {
  const b = await ask(link, INFO, [], (b) => b.length >= 31 && b[0] === 1, tries, 800)
  return b && { size: rd32(b, 1), chunk: rd16(b, 5), name: text(b.slice(7, 23)), version: text(b.slice(23, 31)) }
}

// its block
export async function readBlock(link, onProgress = () => {}) {
  const i = await info(link)
  if (!i) throw fail('none')
  const out = new Uint8Array(i.size)
  for (let off = 0; off < i.size; off += 512) {
    const want = Math.min(512, i.size - off)
    const b = await ask(link, READ, [...wr32(off), ...wr16(want)], (b) => b.length === 6 + want && rd32(b, 0) === off && rd16(b, 4) === want)
    if (!b) throw fail('gone')
    out.set(b.slice(6), off)
    onProgress(off + want, i.size)
  }
  return out
}

const CODES = ['ok', 'busy', 'bad', 'incomplete', 'failed']

// a block to it (which saves it); throws with the reason it gave
export async function writeBlock(link, data, onProgress = () => {}) {
  if (!(await info(link))) throw fail('none')
  for (let off = 0; off < data.length; off += 512) {
    const piece = data.slice(off, off + 512)
    const b = await ask(link, WRITE, [...wr32(off), ...wr32(data.length), ...piece], (b) => b.length === 5 && rd32(b, 0) === off)
    if (!b) throw fail('gone')
    if (b[4] !== SHARE.OK) throw fail(CODES[b[4]] || 'bad')
    onProgress(off + piece.length, data.length)
  }
  // once: it starts over after DONE, whatever it said (and saving takes it a moment)
  const b = await ask(link, DONE, [...wr32(data.length), ...wr16(crc16(data))], (b) => b.length === 1, 1, 3000)
  if (!b) throw fail('unsaid')
  if (b[0] !== SHARE.OK) throw fail(CODES[b[0]] || 'bad')
}
