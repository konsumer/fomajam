// SPDX-License-Identifier: GPL-3.0-only
// A fomajam song (src/groove.h, version 10 or 9: what fm1share.js reads as block 0) as Standard MIDI Files, one for each
// pattern with steps in it, for a DAW or another sequencer. fm1buddy makes the same files.
//
// Each track with steps is a MIDI track: drum tracks on channel 10 with General MIDI drum notes (as the firmware takes
// them in), the others on channels 1-9 (track 1 on 1) with their notes. A drum track is one whose steps (and its own
// voice) are all drum voices (0-21): a synth's notes never are that low in practice. Accents are velocity 127 (else
// 100), ratchets are repeated notes, slides and TIE hold a note into the next, GATE is how long the others are held.
// The tempo and swing are the song's; muted tracks are left out.

const PPQ = 96 // a 16th: 24 ticks
const SIZES = { 9: 11914, 10: 11938 } // (10 added the Bluetooth device after the rest: the same layout before it)
const TRACKS = 9
const STEPS = 16
const PATTERNS = 40
const TRACK_AT = 12 // song_t: track[] (40 bytes each), then 8 more bytes, then pat[] (288 bytes each)
const TRACK_SIZE = 40
const PAT_AT = 382
const STEP_ON = 1
const STEP_ACC = 2
const STEP_SLIDE = 4
const RATCHET = [1, 2, 3, 4, 6, 8, 8, 8]
const GATE_TIE = 101
// each drum voice's General MIDI note (BD BD2 SD SD2 RS CP LT MT HT CH OH CY RD CB CB2 TB LC MC HC TI MA CL)
const GM = [36, 35, 38, 40, 37, 39, 41, 45, 48, 42, 46, 49, 51, 56, 68, 54, 64, 61, 60, 65, 69, 75]

// the song's own: { bpm, swing, length, mute, tracks: [{ voice, gate }], pat(p, t, s) -> { val, flag } }, or null
export function readSong(b) {
  if (b.length !== SIZES[b[0]]) return null
  return {
    bpm: b[1],
    swing: b[2],
    length: Math.max(1, Math.min(STEPS, b[3])),
    mute: b[8] | (b[9] << 8),
    tracks: Array.from({ length: TRACKS }, (_, t) => ({ voice: b[TRACK_AT + t * TRACK_SIZE + 1], gate: b[TRACK_AT + t * TRACK_SIZE + 30] })),
    pat: (p, t, s) => ({ val: b[PAT_AT + p * 288 + t * STEPS + s], flag: b[PAT_AT + p * 288 + 144 + t * STEPS + s] })
  }
}

// variable-length quantity
const vlq = (v) => {
  const out = [v & 0x7f]
  while ((v >>= 7)) out.unshift((v & 0x7f) | 0x80)
  return out
}
const text = (s) => [...s].map((c) => c.charCodeAt(0))
const u32 = (v) => [(v >>> 24) & 255, (v >>> 16) & 255, (v >>> 8) & 255, v & 255]

function chunk(id, data) {
  return [...text(id), ...u32(data.length), ...data]
}

// events [{ at, data }] -> an MTrk, ending it
function track(events) {
  events.sort((a, b) => a.at - b.at || a.order - b.order)
  const out = []
  let now = 0
  for (const e of events) {
    out.push(...vlq(e.at - now), ...e.data)
    now = e.at
  }
  out.push(0, 0xff, 0x2f, 0)
  return chunk('MTrk', out)
}

// which tracks are drums: their steps (all patterns) and their voice are drum voices
export function drumTracks(song) {
  return song.tracks.map((tr, t) => {
    if (tr.voice > 21) return false
    for (let p = 0; p < PATTERNS; p++) for (let s = 0; s < STEPS; s++) if (song.pat(p, t, s).flag & STEP_ON && song.pat(p, t, s).val > 21) return false
    return true
  })
}

// pattern p as a Standard MIDI File (type 1), or null when it has no steps on
export function patternMidi(bytes, p) {
  const song = readSong(bytes)
  if (!song) throw new Error('not a fomajam song this knows (version 9 or 10)')
  const drums = drumTracks(song)
  // when step s starts, and how long it is (swing: the odd ones late)
  const even = Math.round((2 * 24 * (200 + song.swing)) / 400)
  const start = (s) => Math.floor(s / 2) * 48 + (s & 1 ? even : 0)
  const len = (s) => (s & 1 ? 48 - even : even)
  const end = start(song.length - 1) + len(song.length - 1)
  const tracks = []
  for (let t = 0; t < TRACKS; t++) {
    if ((song.mute >> t) & 1) continue
    const ch = drums[t] ? 9 : t
    const ev = []
    const note = (at, n, vel, dur) => {
      ev.push({ at, order: 1, data: [0x90 | ch, n, vel] }, { at: at + Math.max(1, dur), order: 0, data: [0x80 | ch, n, 0] })
    }
    for (let s = 0; s < song.length; s++) {
      const { val, flag } = song.pat(p, t, s)
      if (!(flag & STEP_ON)) continue
      const n = drums[t] ? GM[val] : val & 127
      const vel = flag & STEP_ACC ? 127 : 100
      const r = RATCHET[(flag & 0x38) >> 3]
      if (r > 1) {
        const every = len(s) / r
        for (let k = 0; k < r; k++) note(Math.round(start(s) + k * every), n, vel, Math.round(every / 2))
        continue
      }
      let dur
      if (drums[t]) dur = len(s) / 2
      else if (flag & STEP_SLIDE)
        dur = len(s) + 2 // into the next: legato
      else if (song.tracks[t].gate >= GATE_TIE) {
        let next = s + 1
        while (next < song.length && !(song.pat(p, t, next).flag & STEP_ON)) next++
        dur = (next < song.length ? start(next) : end) - start(s)
      } else dur = (len(s) * Math.max(1, song.tracks[t].gate)) / 100
      note(start(s), n, vel, Math.round(dur))
    }
    if (ev.length) tracks.push(track([{ at: 0, order: 0, data: [0xff, 0x03, ...vlq(`Track ${t + 1}`.length), ...text(`Track ${t + 1}`)] }, ...ev]))
  }
  if (!tracks.length) return null
  const us = Math.round(60000000 / Math.max(1, song.bpm))
  const head = track([
    { at: 0, order: 0, data: [0xff, 0x51, 3, (us >> 16) & 255, (us >> 8) & 255, us & 255] },
    { at: 0, order: 0, data: [0xff, 0x58, 4, 4, 2, 24, 8] },
    { at: end, order: 0, data: [0xff, 0x01, 0] } // (the pattern's length, for a loop)
  ])
  return new Uint8Array([...chunk('MThd', [0, 1, 0, tracks.length + 1, 0, PPQ]), ...head, ...tracks.flat()])
}

// every pattern with steps: [{ name: 'pattern-01.mid', bytes }]
export function songMidi(bytes) {
  const out = []
  for (let p = 0; p < PATTERNS; p++) {
    const m = patternMidi(bytes, p)
    if (m) out.push({ name: `pattern-${String(p + 1).padStart(2, '0')}.mid`, bytes: m })
  }
  return out
}
