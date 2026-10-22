// The song read and written by the web page's manager (web/fm1share.js), against the emulator's firmware
// (build/web/firmware.wasm, platform/src/share.c inside): the same messages as with an FM-1, and as fm1buddy's.
// Run: ./fm1 build, then node test/share_test.mjs
import { readFileSync } from 'node:fs'
import { info, readBlock, writeBlock, frame } from '../web/fm1share.js'

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
await writeBlock(link, changed)
assert((await readBlock(link))[1] === 97, 'WRITE: the changed song is the one it has')
assert(stored().length === 11914 && stored()[1] === 97, 'WRITE: and it saved it')

const odd = song.slice()
odd[0] = 3
assert((await code(writeBlock(link, odd))) === 'bad', 'WRITE: a version it does not know is refused')
assert((await readBlock(link))[1] === 97, 'WRITE: and the song stays')

link.lose = 5
await writeBlock(link, song)
link.lose = 0
assert((await readBlock(link))[1] === song[1], 'WRITE: with messages lost on the way (asked again)')

link.send(frame(0x11)) // (not ours: its identity, as the installer asks)
assert((await link.read(100)) !== null, 'it still answers who it is')
