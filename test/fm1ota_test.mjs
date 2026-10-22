// The installer writes only to a device of the package's own model: the FM-1 and the SMK-37 share the chip and its
// key, so their update loaders would take each other's packages. Run: node test/fm1ota_test.mjs
import { Updater, sameModel } from '../web/fm1ota.js'
const assert = (c, m) => {
  if (!c) {
    console.error('FAIL', m)
    process.exit(1)
  } else console.log('ok', m)
}
assert(sameModel('ota-SMK-37 Pro', 'SMK-37 Pro'), 'SMK loader takes an SMK Pro package')
assert(!sameModel('ota-FM-1', 'SMK-37 Pro'), 'FM-1 loader refuses an SMK package')
assert(!sameModel('ota-SMK-37 Elite', 'SMK-37 Pro'), 'Elite loader refuses a Pro package')
function fake(model) {
  const up = new Updater({ inputs: new Map(), outputs: new Map() })
  let wrote = false
  up.find = async () => ({ link: { close() {} }, id: { model, text: model + '_900' } })
  up.write = async () => {
    wrote = true
    return { finished: true }
  }
  return { up, wrote: () => wrote }
}
for (const [loader, product, ok] of [
  ['ota-SMK-37 Pro', 'SMK-37 Pro_961', true],
  ['ota-FM-1', 'SMK-37 Pro_961', false],
  ['ota-SMK-37 Pro', 'FM-1_961', false],
  ['ota-SMK-37 Pro', '', false],
  ['ota-FM-1', 'FM-1_961', true]
]) {
  const f = fake(loader)
  let err = null
  try {
    await f.up.resume(new Uint8Array(4), null, product)
  } catch (e) {
    err = e
  }
  assert(ok ? f.wrote() && !err : !f.wrote() && err && err.code === 'model', `resume ${loader} <- ${product || '(no identity)'}: ${ok ? 'written' : 'refused'}`)
}

// packages: the 20-block container (ours) and the 36-block one (stock SMK-37 Pro v016) give the same logical image
import { readFileSync, existsSync } from 'node:fs'
import { productOf, logicalImage } from '../web/fm1pkg.js'
const BLK = 0x30,
  KEEP = 0x2f
function wrap(img, product, blocks) {
  const out = new Uint8Array(img.length + blocks)
  for (let i = 0; i < blocks; i++) {
    out.set(img.subarray(i * KEEP, (i + 1) * KEEP), i * BLK)
    out[i * BLK + KEEP] = i < product.length ? (product.charCodeAt(i) + i + 1) & 0xff : 0x7d
  }
  out.set(img.subarray(blocks * KEEP), blocks * BLK)
  return out
}
for (const f of ['build/web/firmware.fwsc', 'build/web/firmware-smk37.fwsc']) {
  if (!existsSync(f)) continue // (run ./fm1 build first)
  const pkg = new Uint8Array(readFileSync(f))
  const img = logicalImage(pkg),
    id = productOf(pkg)
  assert(img.length === pkg.length - 20, `${f}: 20 blocks`)
  const stock = wrap(img, id, 36)
  assert(productOf(stock) === id && logicalImage(stock).every((b, i) => b === img[i]), `${f} as a 36-block package: the same image`)
}
