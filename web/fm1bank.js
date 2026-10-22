// SPDX-License-Identifier: GPL-3.0-only
// fomajam's sample bank (src/drums.c: the store's kits, the machines after MIDI): folders of .wav files made into what
// the store holds (fm1share.js block 1), and read back out of one. fm1buddy makes the same bank on its own.
//
// A kit is a folder: its name is the machine's name. When every file in it is named after a drum voice (bd.wav,
// sd.wav, ch.wav, ...: VOICES), it's a drum kit: it plays as the other drum machines (and its voices that are missing
// are played by the others, as theirs are). Otherwise it's one sample a key, in the files' order.
//
//   "FJB1", kits (1 byte, up to 4), entries (1 byte), 0
//   kits x 12 bytes: name (8, A-Z 0-9, 0 after), first entry, entries (up to 27), 1 if a drum kit, 0
//   entries x 20 bytes: name (8), offset u32 (from the bank's start), len u32 (samples), rate u16 (Hz),
//     idx0 (the IMA step index it starts at), its voice in a drum kit (an index in VOICES)
//   the samples: IMA ADPCM, 4 bits each, the first in the low nibble
//
// Each .wav becomes 22050 Hz mono (as tools/samples.py makes them), its silent tail cut, at the level the firmware's
// own recordings of that voice have (one a key: -3 dB).

export const RATE = 22050
export const MAX_KITS = 4
export const MAX_SAMPLES = 27
export const VOICES = ['BD', 'BD2', 'SD', 'SD2', 'RS', 'CP', 'LT', 'MT', 'HT', 'CH', 'OH', 'CY', 'RD', 'CB', 'CB2', 'TB', 'LC', 'MC', 'HC', 'TI', 'MA', 'CL']
// the peak of each (src/drums.h TARGET)
const TARGET = [27852, 27852, 24575, 24575, 19660, 19660, 22937, 22937, 22937, 13107, 14745, 16384, 14745, 16384, 16384, 13107, 21299, 21299, 21299, 19660, 11468, 16384]
const PEAK = 0.7 * 32767

// prettier-ignore
const STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
  107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876,
  963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
  5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086,
  29794, 32767]
const INDEX = [-1, -1, -1, -1, 2, 4, 6, 8]
const clamp = (v, lo, hi) => (v < lo ? lo : v > hi ? hi : v)

// a .wav's samples (mono, -32768..32767) and rate; throws if it isn't one it can read
export function readWav(bytes) {
  const d = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength)
  const tag = (o) => String.fromCharCode(bytes[o], bytes[o + 1], bytes[o + 2], bytes[o + 3])
  if (bytes.length < 12 || tag(0) !== 'RIFF' || tag(8) !== 'WAVE') throw new Error('not a .wav')
  let fmt = null
  for (let o = 12; o + 8 <= bytes.length;) {
    const id = tag(o)
    const n = d.getUint32(o + 4, true)
    if (id === 'fmt ') fmt = { format: d.getUint16(o + 8, true), ch: d.getUint16(o + 10, true), rate: d.getUint32(o + 12, true), bits: d.getUint16(o + 22, true) }
    else if (id === 'data' && fmt) {
      const { format, ch, rate, bits } = fmt
      const width = bits / 8
      const end = Math.min(bytes.length, o + 8 + n)
      const frames = Math.floor((end - o - 8) / (width * ch))
      if (!(format === 1 || format === 3 || format === 0xfffe) || ![8, 16, 24, 32].includes(bits) || !ch) throw new Error(`a .wav it can't read (format ${format}, ${bits} bits)`)
      const out = new Float64Array(frames)
      const float = format === 3
      for (let f = 0; f < frames; f++) {
        let s = 0
        for (let c = 0; c < ch; c++) {
          const p = o + 8 + (f * ch + c) * width
          s += float ? d.getFloat32(p, true) * 32767 : width === 1 ? (bytes[p] - 128) << 8 : width === 2 ? d.getInt16(p, true) : width === 3 ? ((d.getInt16(p + 1, true) << 8) | bytes[p]) >> 8 : d.getInt32(p, true) >> 16
        }
        out[f] = s / ch
      }
      return { x: out, rate }
    }
    o += 8 + n + (n & 1)
  }
  throw new Error('no sound in it')
}

// at RATE, the silent tail cut, its peak at want
export function prepare(x, rate, want = PEAK) {
  let y = x
  if (rate !== RATE) {
    const step = rate / RATE
    y = new Float64Array(Math.max(0, Math.floor((x.length - 1) / step)))
    for (let k = 0; k < y.length; k++) {
      const p = k * step
      const i = Math.floor(p)
      y[k] = x[i] * (1 - (p - i)) + x[i + 1] * (p - i)
    }
  }
  let peak = 0
  for (const v of y) peak = Math.max(peak, Math.abs(v))
  let end = y.length
  while (end > 1 && Math.abs(y[end - 1]) < peak / 500) end--
  const g = peak ? want / peak : 0
  return Array.from(y.subarray(0, end), (v) => Math.round(v * g))
}

// IMA ADPCM from step index idx: the codes, and how far off it was (the decoder in src/drums.c takes the same steps)
function adpcm(x, idx) {
  let pred = 0,
    err = 0
  const codes = []
  for (const v of x) {
    let step = STEP[idx],
      d = v - pred,
      code = 0
    if (d < 0) ((code = 8), (d = -d))
    let diff = step >> 3
    if (d >= step) ((code |= 4), (d -= step), (diff += step))
    if (d >= step >> 1) ((code |= 2), (d -= step >> 1), (diff += step >> 1))
    if (d >= step >> 2) ((code |= 1), (diff += step >> 2))
    pred = clamp(code & 8 ? pred - diff : pred + diff, -32768, 32767)
    idx = clamp(idx + INDEX[code & 7], 0, 88)
    codes.push(code)
    err += (v - pred) ** 2
  }
  return { codes, err }
}

// the codes packed two a byte, and the step index that follows its attack best (as tools/roms.py)
export function encode(x) {
  let idx0 = 0,
    best = Infinity
  for (let i = 0; i <= 88; i += 2) {
    const e = adpcm(x.slice(0, 256), i).err
    if (e < best) ((best = e), (idx0 = i))
  }
  const { codes } = adpcm(x, idx0)
  if (codes.length & 1) codes.push(0)
  const data = new Uint8Array(codes.length / 2)
  for (let i = 0; i < data.length; i++) data[i] = codes[2 * i] | (codes[2 * i + 1] << 4)
  return { data, idx0, len: codes.length }
}

// the name shown: the file's (or the folder's), in capitals, without leading numbers, up to max characters
// (01-kick.wav: KICK)
export function label(file, max = 6) {
  const n = file
    .replace(/\.wav$/i, '')
    .replace(/^[0-9]+[-_ ]*/, '')
    .toUpperCase()
    .replace(/[^A-Z0-9]/g, '')
  return n.slice(0, max) || 'SMP'
}

// a kit from a folder: name, and files [{ name, bytes }] (.wav, in the order they play) -> { name, drum, samples }
export function kit(name, files) {
  files = files.filter((f) => /\.wav$/i.test(f.name)).sort((a, b) => a.name.localeCompare(b.name, undefined, { numeric: true }))
  if (!files.length) throw new Error(`${name}: no .wav files`)
  const drum = files.every((f) => VOICES.includes(f.name.replace(/\.wav$/i, '').toUpperCase()))
  if (!drum && files.length > MAX_SAMPLES) throw new Error(`${name}: ${files.length} samples, ${MAX_SAMPLES} at most`)
  const samples = files.map((f) => {
    let w
    try {
      w = readWav(f.bytes)
    } catch (e) {
      throw new Error(`${name}/${f.name}: ${e.message}`)
    }
    const voice = drum ? VOICES.indexOf(f.name.replace(/\.wav$/i, '').toUpperCase()) : 0
    return { name: label(f.name), voice, x: prepare(w.x, w.rate, drum ? TARGET[voice] : PEAK) }
  })
  return { name: label(name, 8), drum, samples }
}

// kits [{ name, drum, samples: [{ name, voice, x }] }] -> the bank
export function build(kits) {
  if (kits.length > MAX_KITS) throw new Error(`${kits.length} kits: ${MAX_KITS} at most`)
  const entries = kits.flatMap((k) => k.samples.map((s) => ({ name: s.name, voice: k.drum ? s.voice : 0, ...encode(s.x) })))
  let at = 8 + 12 * kits.length + 20 * entries.length
  const b = new Uint8Array(entries.reduce((n, e) => n + e.data.length, at))
  const d = new DataView(b.buffer)
  const name = (o, s) => [...s.slice(0, 8)].forEach((c, k) => (b[o + k] = c.charCodeAt(0)))
  b.set([70, 74, 66, 49, kits.length, entries.length]) // "FJB1"
  let first = 0
  kits.forEach((k, i) => {
    const o = 8 + 12 * i
    name(o, k.name)
    b[o + 8] = first
    b[o + 9] = k.samples.length
    b[o + 10] = k.drum ? 1 : 0
    first += k.samples.length
  })
  entries.forEach((e, i) => {
    const o = 8 + 12 * kits.length + 20 * i
    name(o, e.name)
    d.setUint32(o + 8, at, true)
    d.setUint32(o + 12, e.len, true)
    d.setUint16(o + 16, RATE, true)
    b[o + 18] = e.idx0
    b[o + 19] = e.voice
    b.set(e.data, at)
    at += e.data.length
  })
  return b
}

// a bank's kits: [{ name, drum, samples: [{ name, seconds }] }], or null if it isn't one
export function describe(b) {
  if (b.length < 8 || String.fromCharCode(...b.subarray(0, 4)) !== 'FJB1' || b[4] > MAX_KITS) return null
  const d = new DataView(b.buffer, b.byteOffset, b.byteLength)
  const text = (o) => String.fromCharCode(...b.subarray(o, o + 8)).replace(/\0.*$/, '')
  const base = 8 + 12 * b[4]
  if (base + 20 * b[5] > b.length) return null
  const out = []
  for (let i = 0; i < b[4]; i++) {
    const o = 8 + 12 * i
    const samples = []
    for (let k = 0; k < b[o + 9]; k++) {
      const e = base + 20 * (b[o + 8] + k)
      samples.push({ name: b[o + 10] ? VOICES[b[e + 19]] : text(e), seconds: d.getUint32(e + 12, true) / d.getUint16(e + 16, true) })
    }
    out.push({ name: text(o), drum: !!b[o + 10], samples })
  }
  return out
}
