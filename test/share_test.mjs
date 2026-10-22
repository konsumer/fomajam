// The song and the samples read and written by the web page's manager (web/fm1share.js, web/fm1bank.js), against the
// emulator's firmware (build/web/firmware.wasm, platform/src/share.c inside): the same messages as with an FM-1, and
// as fm1buddy's. Run: ./fm1 build, then node test/share_test.mjs (with build/render from test/render.c, it also
// checks that the firmware's own code reads the bank: render --bank)
import { readFileSync, writeFileSync, existsSync } from 'node:fs'
import { execFileSync } from 'node:child_process'
import { info, readBlock, writeBlock, frame, SONG, SAMPLES } from '../web/fm1share.js'
import { readWav, kit, build as buildBank, describe, label } from '../web/fm1bank.js'

const assert = (c, m) => {
  if (!c) {
    console.error('FAIL', m)
    process.exit(1)
  } else console.log('ok', m)
}

const env = new Proxy({}, { get: () => () => 0 })
const { instance } = await WebAssembly.instantiate(readFileSync(new URL('../build/web/firmware.wasm', import.meta.url)), { env })
const ex = instance.exports
const mem = () => ex.memory.buffer
ex.web_boot()

// the emulator as a link: SysEx in, its answers out as the clock runs (a few ms a render)
const link = {
  q: [],
  lose: 0, // lose every nth message sent (0: none)
  sent: 0,
  send(bytes) {
    if (this.lose && ++this.sent % this.lose === 0) return true
    new Uint8Array(mem(), ex.web_sysex_buf(), bytes.length).set(bytes)
    ex.web_sysex_in(bytes.length)
    return true
  },
  drain() {
    this.q.length = 0
  },
  async read(ms) {
    for (let i = 0; !this.q.length && i < 100; i++) {
      ex.web_render(128)
      const n = ex.web_sysex_out_len()
      if (n) {
        const b = Array.from(new Uint8Array(mem(), ex.web_sysex_out(), n))
        ex.web_sysex_out_clear()
        for (let s = 0; s < b.length;) {
          const e = b.indexOf(0xf7, s)
          this.q.push(b.slice(s, e + 1))
          s = e + 1
        }
      }
    }
    return this.q.shift() || null
  }
}
const stored = () => new Uint8Array(mem(), ex.web_store(), Math.max(0, ex.web_store_len())).slice()
const code = async (p) => {
  try {
    await p
    return 'ok'
  } catch (e) {
    return e.code
  }
}

const i = await info(link)
assert(i && i.name === 'FOMAJAM' && i.size === 11914 && i.chunk === 512, `INFO: ${i && i.name} ${i && i.version}, ${i && i.size} bytes`)
const song = await readBlock(link)
assert(song.length === 11914 && song[0] === 9, 'READ: a version 9 song')

const changed = song.slice()
changed[1] = 97 // the tempo
await writeBlock(link, SONG, changed)
assert((await readBlock(link))[1] === 97, 'WRITE: the changed song is the one it has')
assert(stored().length === 11914 && stored()[1] === 97, 'WRITE: and it saved it')

const odd = song.slice()
odd[0] = 3
assert((await code(writeBlock(link, SONG, odd))) === 'bad', 'WRITE: a version it does not know is refused')
assert((await readBlock(link))[1] === 97, 'WRITE: and the song stays')

link.lose = 5
await writeBlock(link, SONG, song)
link.lose = 0
assert((await readBlock(link))[1] === song[1], 'WRITE: with messages lost on the way (asked again)')

link.send(frame(0x11)) // (not ours: its identity, as the installer asks)
assert((await link.read(100)) !== null, 'it still answers who it is')

// the samples: two kits of .wav files (16-bit stereo 44.1 kHz, 8-bit mono 11 kHz) made into a bank, to the emulator
// and back: MY KIT (a drum kit: bd.wav, ch.wav) and LOOPS (one a key)
function wav(rate, ch, bits, seconds, f) {
  const n = Math.round(rate * seconds)
  const w = bits / 8
  const b = new Uint8Array(44 + n * ch * w)
  const d = new DataView(b.buffer)
  b.set(
    [...'RIFF'].map((c) => c.charCodeAt(0)),
    0
  )
  d.setUint32(4, 36 + n * ch * w, true)
  b.set(
    [...'WAVEfmt '].map((c) => c.charCodeAt(0)),
    8
  )
  d.setUint32(16, 16, true)
  d.setUint16(20, 1, true)
  d.setUint16(22, ch, true)
  d.setUint32(24, rate, true)
  d.setUint32(28, rate * ch * w, true)
  d.setUint16(32, ch * w, true)
  d.setUint16(34, bits, true)
  b.set(
    [...'data'].map((c) => c.charCodeAt(0)),
    36
  )
  d.setUint32(40, n * ch * w, true)
  for (let i = 0; i < n; i++)
    for (let c = 0; c < ch; c++) {
      const v = f(i / rate)
      if (bits === 16) d.setInt16(44 + (i * ch + c) * 2, Math.round(v * 20000), true)
      else b[44 + i * ch + c] = 128 + Math.round(v * 100)
    }
  return b
}
const kickWav = wav(44100, 2, 16, 0.5, (t) => Math.sin(2 * Math.PI * (50 + 100 * Math.exp(-t * 30)) * t) * Math.exp(-t * 6))
const hatWav = wav(11025, 1, 8, 0.2, () => Math.random() * 2 - 1)
const kick = readWav(kickWav)
assert(kick.rate === 44100 && Math.abs(kick.x.length - 22050) < 2 && readWav(hatWav).rate === 11025, '.wav files read')
assert(label('01-Kick Drum.wav') === 'KICKDR' && label('hat.wav') === 'HAT' && label('My Kit', 8) === 'MYKIT', 'names: 01-Kick Drum.wav is KICKDR')
const kits = [
  kit('My Kit', [
    { name: 'ch.wav', bytes: hatWav },
    { name: 'BD.wav', bytes: kickWav }
  ]),
  kit('loops', [
    { name: '2-hat.wav', bytes: hatWav },
    { name: '1-kick.wav', bytes: kickWav },
    { name: 'notes.txt', bytes: new Uint8Array(3) }
  ])
]
assert(kits[0].drum && !kits[1].drum, 'a folder of bd.wav, ch.wav is a drum kit; others one a key')
const bank = buildBank(kits)
const names = describe(bank)
assert(names && names.map((k) => `${k.name}:${k.samples.map((s) => s.name).join('+')}`).join() === 'MYKIT:BD+CH,LOOPS:KICK+HAT', `a bank: ${bank.length} bytes, ${names.map((k) => k.name).join(', ')}`)

const before = await info(link, SAMPLES)
assert(before && before.size === 0 && before.max === 288 * 1024, 'STORE: empty, 288 KiB')
await writeBlock(link, SAMPLES, bank)
const back = await readBlock(link, SAMPLES)
assert(back.length === bank.length && back.every((v, i) => v === bank[i]), 'STORE: the bank written, and read back the same')
assert((await code(writeBlock(link, SAMPLES, new Uint8Array(300 * 1024)))) === 'big', 'STORE: too big is refused before sending')
link.lose = 7
await writeBlock(link, SAMPLES, bank)
link.lose = 0
assert((await info(link, SAMPLES)).size === bank.length, 'STORE: with messages lost on the way')

const render = new URL('../build/render', import.meta.url).pathname
if (existsSync(render)) {
  const file = new URL('../build/test-bank.bin', import.meta.url).pathname
  writeFileSync(file, bank)
  const out = execFileSync(render, ['--bank', file]).toString()
  assert(/MYKIT \(a drum kit\): BD peak [1-9]\d{3,} CH peak [1-9]\d{3,}/.test(out) && /LOOPS \(one a key\): KICK peak [1-9]\d{3,} HAT peak [1-9]\d{3,}/.test(out), `the firmware plays it: ${out.trim().replace(/\n/g, '; ')}`)
}
