// SPDX-License-Identifier: GPL-3.0-only
// The FM-1 in an AudioWorklet: firmware.wasm is your code on platform/web.c. Each render quantum
// asks it for 128 frames, which runs the device's clock forward (loop() once per millisecond);
// about 30 times a second the screen, the LEDs, MIDI out, print() and the settings go to the page. SysEx goes both ways
// as with the FM-1 on USB (fm1share.js): in one message at a time, its answers out as soon as they're there.

class FM1 extends AudioWorkletProcessor {
  constructor() {
    super()
    this.ex = null
    this.port.onmessage = (e) => this.onMessage(e.data)
  }

  async onMessage(m) {
    if (m.type === 'load') {
      this.ex = null
      try {
        const env = new Proxy({}, { get: () => () => 0 })
        const { instance } = await WebAssembly.instantiate(m.wasm, { env })
        const ex = instance.exports
        this.mem = ex.memory
        if (m.store) {
          new Uint8Array(this.mem.buffer, ex.web_store(), m.store.length).set(m.store)
          ex.web_store_set_len(m.store.length)
        }
        if (m.flash) {
          // the store (samples): its data at the start, its record after STORE_MAX
          const f = new Uint8Array(this.mem.buffer, ex.web_flash(), ex.web_flash_len())
          f.set(m.flash.data.subarray(0, f.length - 4096))
          f.set(m.flash.record, f.length - 4096)
        }
        ex.web_master(m.master)
        this.sentFb = -1
        this.sentStore = ex.web_store_gen()
        this.sentFlash = ex.web_flash_gen()
        this.last = 0
        this.midi = []
        ex.web_boot()
        this.ex = ex
        this.port.postMessage({ type: 'ready' })
      } catch (err) {
        this.crash(err)
      }
      return
    }
    const ex = this.ex
    if (!ex) return
    if (m.type === 'keys') ex.web_keys(m.mask >>> 0)
    else if (m.type === 'buttons') ex.web_buttons(m.mask >>> 0)
    else if (m.type === 'encoder') ex.web_encoder(m.id, m.n | 0)
    else if (m.type === 'master') ex.web_master(m.value | 0)
    else if (m.type === 'midi') ex.web_midi_in(m.data[0], m.data[1] || 0, m.data[2] || 0)
    else if (m.type === 'sysex' && m.data.length <= 640) {
      new Uint8Array(this.mem.buffer, ex.web_sysex_buf(), m.data.length).set(m.data)
      ex.web_sysex_in(m.data.length) // (busy: dropped, and asked again)
    }
  }

  crash(err) {
    this.ex = null
    this.port.postMessage({ type: 'crash', message: String((err && err.message) || err) })
  }

  publish() {
    const ex = this.ex
    const msg = { type: 'frame', ms: ex.web_now(), keys: ex.web_led_keys() >>> 0, buttons: ex.web_led_buttons() >>> 0, backlight: ex.web_led_backlight ? ex.web_led_backlight() : 0 }
    const transfer = []
    const gen = ex.web_fb_gen()
    if (gen !== this.sentFb) {
      this.sentFb = gen
      msg.fb = new Uint16Array(this.mem.buffer, ex.web_fb(), 240 * 240).slice()
      transfer.push(msg.fb.buffer)
    }
    const sg = ex.web_store_gen()
    if (sg !== this.sentStore) {
      this.sentStore = sg
      msg.store = new Uint8Array(this.mem.buffer, ex.web_store(), ex.web_store_len()).slice()
    }
    const fg = ex.web_flash_gen()
    if (fg !== this.sentFlash) {
      this.sentFlash = fg
      const f = new Uint8Array(this.mem.buffer, ex.web_flash(), ex.web_flash_len())
      const record = f.slice(f.length - 4096, f.length - 4096 + 16)
      const n = record[4] | (record[5] << 8) | (record[6] << 16) | (record[7] << 24)
      msg.flash = { data: f.slice(0, Math.min(n, f.length - 4096)), record }
    }
    const p = ex.web_log()
    const bytes = new Uint8Array(this.mem.buffer, p)
    let n = 0
    while (bytes[n]) n++
    if (n) msg.log = String.fromCharCode(...bytes.subarray(0, n)) // (no TextDecoder in a worklet)
    if (this.midi.length) {
      msg.midi = this.midi
      this.midi = []
    }
    this.port.postMessage(msg, transfer)
  }

  process(inputs, outputs) {
    const ex = this.ex
    if (!ex) return true
    const out = outputs[0]
    const n = out[0].length
    try {
      ex.web_render(n)
      for (let m; (m = ex.web_midi_out()) >= 0;) this.midi.push(m)
      const k = ex.web_sysex_out_len()
      if (k) {
        this.port.postMessage({ type: 'sysex', data: new Uint8Array(this.mem.buffer, ex.web_sysex_out(), k).slice() })
        ex.web_sysex_out_clear()
      }
    } catch (err) {
      this.crash(err)
      return true
    }
    out[0].set(new Float32Array(this.mem.buffer, ex.web_out_l(), n))
    if (out[1]) out[1].set(new Float32Array(this.mem.buffer, ex.web_out_r(), n))
    if (currentTime - this.last >= 1 / 30) {
      this.last = currentTime
      this.publish()
    }
    return true
  }
}

registerProcessor('fm1', FM1)
