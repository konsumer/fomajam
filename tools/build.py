#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build everything from src/ (run it in the toolchain container: ./fm1 build).

  tools/build.py              build/web/: the site (emulator, installer, rescue) with
                              firmware.wasm (the emulator) and firmware.fwsc (the FM-1 package)
  tools/build.py --dev PORT   the same, then serve build/web on PORT and rebuild on every change

firmware.json "boards" (default ["fm1"]) adds "smk37": the M-Vave SMK-37 (same chip, its own panel and flash map;
platform/hal/smk_input.h), one package per model name (SMK-37 Pro, SMK-37 Elite, MKE-P37), since M-Vave's updater
only takes a unit's own model. "fm1bt" adds a second FM-1 package, NAME-bt-VERSION.fwsc: Bluetooth MIDI (the
bluetooth option, platform/bt.c) in place of the ROM machines that no longer fit (BT_ROM_BUDGET, build/gen-bt/roms.h).
--dev builds the FM-1 only.

The FM-1 part is Dinghy's / Felucca's build (Leo Kuroshita, Hügelton Instruments; zvenson):
the app, the update loader, the checks that keep a build from bricking the FM-1, the package.
"""

import argparse
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLAT = ROOT / "platform"
SRC = ROOT / "src"
WEB = ROOT / "web"
OUT = ROOT / "build"
FW = OUT / "fm1"
LDR = OUT / "loader"
SITE = OUT / "web"
SAMPLES = ROOT / "samples"
sys.path.insert(0, str(ROOT / "tools"))
import fm1pkg_make  # noqa: E402
import lz4blk  # noqa: E402
import roms  # noqa: E402
import samples  # noqa: E402

TC = Path(os.environ.get("JIELI_TOOLCHAIN", "/opt/jieli/toolchain"))
APP_XIP = 0x02000120  # app.bin offset 0 in the XIP map; the SPL jumps here
APP_SLOT = fm1pkg_make.APP_SLOT
LOADER_LOAD = 0x01C0A800
LOADER_NAME = b"usb_hid_ota.bin"  # the file name the SPL looks for
CFLAGS = ["-Os", "-ffunction-sections", "-fno-builtin", "-Wall", "-Wno-unused-function"]
INCLUDES = ["-Iplatform", "-Iplatform/hal", "-Iplatform/src", "-Iplatform/gen", "-Ibuild/gen", "-Ivendor/felucca-engines/src", "-Ivendor/felucca"]
LINE = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*\t(.*)$")


class BuildError(Exception):
  pass


def run(cmd):
  r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
  if r.returncode:
    raise BuildError((r.stdout + r.stderr).strip() or f"{cmd[0]} failed")
  if r.stderr.strip():
    print(r.stderr.rstrip(), file=sys.stderr)
  return r.stdout


def rel(a):
  return str(Path(a).resolve().relative_to(ROOT)) if isinstance(a, Path) else a


def tc(tool, *args):
  """a JieLi toolchain binary: cc, ld, objcopy, objdump"""
  args = [rel(a) for a in args]
  if tool == "cc":
    return run([str(TC / "pi32v2/bin/clang"), "-target", "pi32v2", *args])
  if tool == "ld":
    return run([str(TC / "pi32v2/bin/ld"), *args])
  return run([str(TC / "common/bin" / tool), *args])


def tc_all(*cmds):
  with ThreadPoolExecutor(len(cmds)) as ex:
    return list(ex.map(lambda c: tc(*c), cmds))


# firmware.json "options": the platform's parts you can leave out (all in unless set false), as FOMA_* in fm1.h
OPTIONS = {"usb_audio": "FOMA_USB_AUDIO", "trs_midi": "FOMA_TRS_MIDI", "midi_clock": "FOMA_MIDI_CLOCK", "bluetooth": "FOMA_BLUETOOTH"}
OFF_BY_DEFAULT = {"bluetooth"}


# the boards firmware.json "boards" can list. smk37: one target per model name (one keyboard, three names)
SMK_MODELS = [("SMK-37 Pro", ""), ("SMK-37 Elite", "-elite"), ("MKE-P37", "-mke")]
BOARDS = ("fm1", "fm1bt", "smk37")
GEN_BT = OUT / "gen-bt"  # the fm1bt build's generated headers, ahead of build/gen: fewer ROM machines
BT_ROM_BUDGET = roms.BUDGET - 47 * 1024  # the Bluetooth stack's room
BT_ROM_SECONDS = 0.77  # and its sounds a little shorter than roms.MAX_SECONDS, so the eight machines still fit


def targets(cfg):
  """what to build: board, model name, identity, package names (build/, build/web/), output folders, -D flags"""
  minor = cfg["product"][-1]
  digits = cfg["identity"].split("_", 1)[1]
  out = []
  for board in cfg["boards"]:
    if board == "fm1":
      out.append(
        dict(
          board="fm1",
          model="FM-1",
          product=cfg["product"],
          pkg=f"{cfg['file']}-{cfg['version']}.fwsc",
          site="firmware.fwsc",
          fw=FW,
          ldr=LDR,
          defines=cfg["defines"],
        )
      )
      continue
    if board == "fm1bt":
      defines = [d if not d.startswith("-DFOMA_BLUETOOTH=") else "-DFOMA_BLUETOOTH=1" for d in cfg["defines"]]
      out.append(
        dict(
          board="fm1",
          model="FM-1 BT",
          product=cfg["product"],
          pkg=f"{cfg['file']}-bt-{cfg['version']}.fwsc",
          site="firmware-bt.fwsc",
          fw=OUT / "fm1bt",
          ldr=OUT / "fm1bt" / "loader",
          defines=defines,
          includes=[f"-I{GEN_BT.relative_to(ROOT)}"],
        )
      )
      continue
    for model, suffix in SMK_MODELS:
      stem = f"smk37{suffix}"
      # no TRS MIDI in on the SMK-37 (its pin is a key-matrix row): the option is off there
      defines = [d if not d.startswith("-DFOMA_TRS_MIDI=") else "-DFOMA_TRS_MIDI=0" for d in cfg["defines"]]
      out.append(
        dict(
          board="smk37",
          model=model,
          product=f"{model}_{digits}{minor}",
          pkg=f"{cfg['file']}-{stem}-{cfg['version']}.fwsc",
          site=f"firmware-{stem}.fwsc",
          fw=OUT / stem,
          ldr=OUT / stem / "loader",
          defines=[*defines, "-DFOMA_SMK37=1", f'-DSMK_MODEL="{model}"'],
        )
      )
  return out


def sources():
  return sorted(SRC.glob("**/*.c"))


def git_version():
  """the version from the last v* tag: 0.1.6 on the tag, 0.1.6-3 three commits after it; None without git or tags"""
  d = os.environ.get("GITVER")  # ./fm1 reads it outside Docker (the image has no git)
  if d is None:
    try:
      d = subprocess.run(["git", "describe", "--tags", "--match", "v[0-9]*", "--long"], cwd=ROOT, capture_output=True, text=True).stdout
    except OSError:
      return None
  m = re.fullmatch(r"v(.+)-(\d+)-g[0-9a-f]+", d.strip())
  if not m:
    return None
  return m[1] if m[2] == "0" else f"{m[1]}-{m[2]}"


def config():
  c = json.loads((ROOT / "firmware.json").read_text())
  c["version"] = git_version() or c.get("version", "")
  m = re.fullmatch(r"(\d+)\.(\d+)(\.\d+)?(-[A-Za-z0-9]+)?", c.get("version", ""))
  if not m:
    raise BuildError(f'version "{c["version"]}" (git tag or firmware.json) must look like 0.1, 1.2.3 or 0.1-beta')
  if not re.fullmatch(r"[A-Z0-9 _.+-]{1,16}", c.get("name", "")):
    raise BuildError('firmware.json: "name" must be 1..16 capitals, digits, spaces or _.+-')
  c["product"] = c["identity"] + m[2][-1]  # FM-1_95 + the minor version's last digit
  if not re.fullmatch(r"FM-1_\d{3}", c["product"]):
    raise BuildError('firmware.json: "identity" must be FM-1_ and two digits (the third is your minor version)')
  c["file"] = re.sub(r"[^a-z0-9.-]+", "-", c["name"].lower()).strip("-") or "firmware"
  opts = c.get("options", {})
  bad = sorted(set(opts) - set(OPTIONS))
  if bad:
    raise BuildError(f"firmware.json: unknown option {', '.join(bad)} (there are: {', '.join(OPTIONS)})")
  c["defines"] = [f"-D{OPTIONS[k]}={int(bool(opts.get(k, k not in OFF_BY_DEFAULT)))}" for k in OPTIONS]
  c["boards"] = c.get("boards", ["fm1"])
  if not c["boards"] or not set(c["boards"]) <= set(BOARDS) or len(set(c["boards"])) != len(c["boards"]):
    raise BuildError(f'firmware.json: "boards" lists some of {", ".join(BOARDS)}, each once')
  return c


# ---- the emulator: the same code against platform/web.c, as WebAssembly


def build_wasm(cfg):
  SITE.mkdir(parents=True, exist_ok=True)
  run(
    [
      "clang",
      "--target=wasm32",
      "-O2",
      "-nostdlib",
      "-fno-builtin",
      "-Wall",
      "-Wno-unused-function",
      "-Wno-incompatible-library-redeclaration",
      *INCLUDES,
      f'-DAPP_NAME="{cfg["name"]}"',
      f'-DAPP_VER="{cfg["version"]}"',
      f'-DFELUCCA_ID="{cfg["product"]}"',
      *cfg["defines"],
      "-Wl,--no-entry",
      "-Wl,-z,stack-size=1048576",
      "-o",
      rel(SITE / "firmware.wasm"),
      "platform/web.c",
      *[rel(s) for s in sources()],
    ]
  )
  return (SITE / "firmware.wasm").stat().st_size


# ---- the update loader


def crc16(d, c=0):
  for b in d:
    c ^= b << 8
    for _ in range(8):
      c = ((c << 1) ^ 0x1021) if c & 0x8000 else c << 1
    c &= 0xFFFF
  return c


def ldr_head(dcrc, a, b, attr, name):
  body = struct.pack("<HIIBBH16s", dcrc, a, b, attr, 0, 0, name)
  return struct.pack("<H", crc16(body)) + body


def ldr_wrap(image):
  """ota.bin = outer JLFS head (32 B) + inner head (32 B, load address) +
  blocks [clen u32][dlen u32][LZ4 block], dlen 4096 except the last"""
  blocks = bytearray()
  for i in range(0, len(image), 4096):
    raw = image[i : i + 4096]
    c = lz4blk.compress(raw)
    if len(c) > 4096 or lz4blk.decompress(c) != raw:
      raise BuildError(f"loader: block {i // 4096} does not compress below 4096 B")
    blocks += struct.pack("<II", len(c), len(raw)) + c
  inner = ldr_head(crc16(image), len(image), LOADER_LOAD, 0, LOADER_NAME)
  body = inner + bytes(blocks)
  return ldr_head(crc16(body), 0x20, len(body), 0x41, LOADER_NAME) + body


def build_loader(t):
  LDR = t["ldr"]
  LDR.mkdir(parents=True, exist_ok=True)
  src = PLAT / "loader"
  board = [d for d in t["defines"] if d.startswith(("-DFOMA_SMK37", "-DSMK_MODEL"))]  # (the FM-1's: none)
  tc_all(
    ("cc", "-c", src / "crt0_ldr.S", "-o", LDR / "crt0_ldr.o"),
    ("cc", *CFLAGS, *INCLUDES, *board, "-c", src / "loader.c", "-o", LDR / "loader.o"),
  )
  elf = LDR / "loader.elf"
  tc("ld", "--gc-sections", "-e", "_start", "-T", src / "loader.ld", LDR / "crt0_ldr.o", LDR / "loader.o", "-o", elf)
  _, dis, hdr = tc_all(("objcopy", "-O", "binary", "-j", ".text", elf, LDR / "loader.bin"), ("objdump", "-d", elf), ("objdump", "-h", elf))
  (LDR / "loader.dis").write_text(dis)
  for ln in hdr.splitlines():  # the loader rewrites the flash: nothing may run from XIP
    p = ln.split()
    if len(p) > 4 and p[1].startswith(".") and int(p[3], 16) >= 0x02000000 and int(p[2], 16):
      raise BuildError(f"loader: section {p[1]} at {p[3]} is outside RAM")
  image = (LDR / "loader.bin").read_bytes()
  if not image or len(image) > 0x14000:
    raise BuildError("loader: image empty or too big")
  return ldr_wrap(image)


# ---- the app


def build_app(cfg, t):
  FW = t["fw"]
  FW.mkdir(parents=True, exist_ok=True)
  flags = [
    *CFLAGS,
    *t.get("includes", []),
    *INCLUDES,
    f'-DAPP_NAME="{cfg["name"]}"',
    f'-DAPP_VER="{cfg["version"]}"',
    f'-DFELUCCA_ID="{t["product"]}"',
    *t["defines"],
  ]
  objs, cmds = [], []
  for s in [PLAT / "crt0.S", PLAT / "hal/fm1_vec.S", PLAT / "hal/fm1_isr.S"]:
    objs.append(FW / (s.stem + ".o"))
    cmds.append(("cc", "-c", s, "-o", objs[-1]))
  for s in [PLAT / "fm1.c", *sources()]:
    objs.append(FW / (("platform_" if s.parent == PLAT else "app_") + s.stem + ".o"))
    cmds.append(("cc", *flags, "-c", s, "-o", objs[-1]))
  tc_all(*cmds)
  elf = FW / "firmware.elf"
  ld = PLAT / "app.ld"
  slot = fm1pkg_make.BOARDS[t["board"]][1]
  if slot != APP_SLOT:  # another board's app slot (SMK-37): the same script, its XIP length
    txt = ld.read_text()
    if txt.count(f"LENGTH = 0x{APP_SLOT:X}") != 1:
      raise BuildError("app.ld: the XIP LENGTH line was not found")
    ld = FW / "app.ld"
    ld.write_text(txt.replace(f"LENGTH = 0x{APP_SLOT:X}", f"LENGTH = 0x{slot:X}"))
  try:
    tc("ld", "--gc-sections", "-e", "_start", "-T", ld, *objs, "-o", elf)
  except BuildError as e:
    raise BuildError(explain_link(str(e))) from None
  for sect in ("text.bin", "data.bin", "ramtext.bin"):
    (FW / sect).unlink(missing_ok=True)
  *_, syms, dis, rt = tc_all(
    ("objcopy", "-O", "binary", "-j", ".text", elf, FW / "text.bin"),
    ("objcopy", "-O", "binary", "-j", ".data", elf, FW / "data.bin"),
    ("objcopy", "-O", "binary", "-j", ".ram_text", elf, FW / "ramtext.bin"),
    ("objdump", "-t", elf),
    ("objdump", "-d", elf),
    ("objdump", "-d", "-j", ".ram_text", elf),
  )
  (FW / "firmware.dis").write_text(dis)

  def symv(name):
    return int(re.search(r"^([0-9a-f]+) .*\s" + name + r"$", syms, re.M).group(1), 16)

  img = bytearray((FW / "text.bin").read_bytes())
  # .ram_text and .data follow .text at their load addresses; crt0 copies them by words
  for sect, lname in (("ramtext.bin", "_rt_load"), ("data.bin", "_data_load")):
    load = symv(lname)
    if load % 4:
      raise BuildError(f"{lname} {load:#x} is not word aligned")
    blob = (FW / sect).read_bytes() if (FW / sect).exists() else b""
    if blob:
      if load - APP_XIP < len(img):
        raise BuildError(f"{lname} overlaps the image")
      img += b"\xff" * (load - APP_XIP - len(img))
      img += blob
  img += b"\xff" * (-len(img) % 4)
  (FW / "firmware.bin").write_bytes(img)
  return bytes(img), syms, dis, rt


LINK_HINTS = [
  (
    r"__(?:float|fix|extend|trunc)\w*|__(?:add|sub|mul|div|neg|cmp|eq|ne|lt|le|gt|ge|un)[sd]f[23]\b",
    "float or double: the FM-1 has no FPU and no float library. Use integers (fixed point, e.g. Q15)",
  ),
  (r"__u?(?:div|mod)di3\b", "a 64-bit division or modulo: there is no libgcc on the FM-1. Shift, or divide 32-bit values"),
  (
    r"\b(?:printf|sprintf|snprintf|malloc|free|strlen|strcpy|sin|cos|pow|sqrt)\b",
    "there is no C library on the FM-1: see fm1.h for what there is (format_int, sine, ...)",
  ),
]


def explain_link(err):
  """the linker's 'undefined reference' errors, in words, with where they come from"""
  out = []
  for ln in err.splitlines():
    m = re.search(r"^(\S+?):\(\.text\.(\w+)[^)]*\): undefined reference to `(\w+)'", ln)
    if not m:
      continue
    why = next((h for rx, h in LINK_HINTS if re.search(rx, m[3])), f"{m[3]} does not exist")
    line = f"  in {m[2]}(): {why} ({m[3]})"
    if line not in out:
      out.append(line)
  return "link failed:\n" + "\n".join(out) if out else err


def check(img, syms, dis, rt, slot=APP_SLOT):
  errors, notes = [], []
  m = re.search(r"^([0-9a-f]+) .*\s_start$", syms, re.M)
  if not m or int(m.group(1), 16) != APP_XIP:
    errors.append(f"_start is not at {APP_XIP:#x}")
  if img[:4] != bytes.fromhex("04818000"):
    errors.append(f"image starts with {img[:4].hex()}, not the entry stub")
  rt_calls = [ln for ln in rt.splitlines() if re.search(r"\bcall\b", ln)]
  if rt_calls:  # RAM code runs with the flash off: no calls into XIP
    errors.append(f".ram_text contains calls: {rt_calls[:3]}")
  fn = "?"
  for ln in dis.splitlines():  # nothing may call or load an address in the chip ROM
    h = re.match(r"^[0-9a-f]+ <([^>]+)>:", ln)
    if h:
      fn = h.group(1)
      continue
    mm = LINE.match(ln)
    if not mm:
      continue
    for v in re.findall(r"= (-?\d+) <|call -?\d+ <[^:>]*: ([0-9a-f]+) >", mm.group(3)):
      val = (int(v[0]) & 0xFFFFFFFF) if v[0] else int(v[1], 16) & 0xFFFFFFFF
      if 0xFFC00000 <= val < 0xFFD00000:
        errors.append(f"{fn}: a constant {val:#010x} lies in the chip ROM (a call there bricks the FM-1): change the arithmetic so it never appears")
  if len(img) > slot:
    errors.append(f"image {len(img)} B exceeds the app slot ({slot} B)")

  def sym(name):
    mm = re.search(r"^([0-9a-f]+) .*\s" + name + r"$", syms, re.M)
    return int(mm.group(1), 16) if mm else 0

  ram = sym("_bss_end") - 0x01C08000
  pool = sym("_pool_end") - 0x01C20000
  notes.append(f"code {len(img) // 1024} of {slot // 1024} KiB, RAM {ram // 1024} of 96 KiB, pool {pool // 1024} of 336 KiB")
  if ram > 96 * 1024:
    errors.append(f"RAM (.data + .bss) {ram} B > 96 KiB: make big arrays POOL or const")
  if pool > 0x54000:
    errors.append(f"pool {pool} B > 336 KiB")
  return errors, notes


# Every hardware register access lives in platform/hal/. Elsewhere (comments stripped):
#  - no volatile pointer cast, except of a C object's address (`*(volatile T *)&x`);
#  - no literal in a register or reserved window (core SFRs 0x10000-0x13FFF, SFC 0x40000-0x43FFF,
#    GPIO/IOMAP 0x50000-0x51FFF, CPU 0x1EE0000-0x1EEFFFF, RAM top 0x01C7F000-, XIP 0x02000000-0x020FFFFF);
#  - no inline asm, except the empty compiler barrier RING_PUBLISH();
#  - no HAL register macro (a hal/ #define that is, or expands to, a volatile access).
MMIO_LIT = re.compile(r"\b0x0*(1[0-3][0-9a-f]{3}|4[0-3][0-9a-f]{3}|5[01][0-9a-f]{3}|1ee[0-9a-f]{4}|1c7f[0-9a-f]{3}|20[0-9a-f]{5})u?l?\b", re.I)
MMIO_CAST = re.compile(r"\(\s*(?:const\s+)?volatile\b[^()]*\*\s*\)(?!\s*&)")
MMIO_ASM = re.compile(r"\b(?:__asm__|asm)\b(?!\s+volatile\s*\(\s*\"\"\s*:::\s*\"memory\"\s*\))")


def mmio_check():
  def strip(s):
    s = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)

  defs = {}
  for h in sorted((PLAT / "hal").glob("*.h")):
    for m in re.finditer(r"^#define\s+(\w+)(?:\([^)]*\))?\s+(.*)$", strip(h.read_text()), re.M):
      defs[m.group(1)] = m.group(2)
  regs = {n for n, b in defs.items() if re.search(r"\bvolatile\b", b)}
  while True:
    more = {n for n, b in defs.items() if n not in regs and set(re.findall(r"\w+", b)) & regs}
    if not more:
      break
    regs |= more
  errors = []
  files = [*SRC.glob("**/*.[ch]"), *(PLAT / "src").glob("*.[ch]"), *(PLAT / "loader").glob("*.c"), PLAT / "fm1.c", PLAT / "common.c"]
  for f in sorted(files):
    for no, ln in enumerate(strip(f.read_text()).splitlines(), 1):
      where = f"{f.relative_to(ROOT)}:{no}"
      for rx, what in ((MMIO_LIT, "register/window address"), (MMIO_CAST, "volatile pointer cast"), (MMIO_ASM, "inline asm")):
        m = rx.search(ln)
        if m:
          errors.append(f"{where}: {what} outside platform/hal ({m.group(0).strip()})")
      used = set(re.findall(r"\b[A-Z_][A-Z0-9_]*\b", ln)) & regs
      if used:
        errors.append(f"{where}: HAL register macro {sorted(used)[0]} outside platform/hal")
  return errors


def build_fm1(cfg, t):
  ota = build_loader(t)
  img, syms, dis, rt = build_app(cfg, t)
  errors, notes = check(img, syms, dis, rt, fm1pkg_make.BOARDS[t["board"]][1])
  errors += mmio_check()
  if errors:
    raise BuildError(f"the {t['model']} build is refused (it could crash or brick the {t['model']}):\n" + "\n".join("  " + e for e in errors))
  try:
    pkg = fm1pkg_make.ufw(fm1pkg_make.flash_image(img, fm1pkg_make.KEY, t["board"]), ota, t["product"])
  except SystemExit as e:
    raise BuildError(str(e)) from None
  (SITE / t["site"]).write_bytes(pkg)
  (OUT / t["pkg"]).write_bytes(pkg)
  return notes


# ---- the site


def build_site():
  SITE.mkdir(parents=True, exist_ok=True)
  for f in WEB.iterdir():
    if f.is_file():
      shutil.copy(f, SITE / f.name)


def build(dev_mode=False):
  t0 = time.time()
  info = {"id": str(time.time_ns()), "wasm": False, "fwsc": False, "log": "", "packages": {}}
  log = []
  try:
    cfg = config()
    info.update(name=cfg["name"], version=cfg["version"], identity=cfg["product"])
    build_site()
    try:
      n, size = samples.generate(SAMPLES, OUT / "gen" / "samples.h")
    except ValueError as e:
      raise BuildError(str(e))
    log.append(f"samples   {n} from samples/, {size // 1024} KiB")
    fe = OUT / "gen" / "fe_tables.h"  # the Felucca engines' tables (vendor/felucca-engines)
    if not fe.exists():
      run([sys.executable, "vendor/felucca-engines/tools/gen_engine_tables.py", rel(fe)])
    names, size = roms.generate(ROOT / "roms", OUT / "gen" / "roms.h")
    log.append(f"roms      {' '.join(names) or 'none (offline?)'}, {size // 1024} KiB")
    if "fm1bt" in cfg["boards"]:
      names, size = roms.generate(ROOT / "roms", GEN_BT / "roms.h", BT_ROM_BUDGET, BT_ROM_SECONDS)
      log.append(f"roms (BT) {' '.join(names) or 'none (offline?)'}, {size // 1024} KiB")
    try:
      n = build_wasm(cfg)
      info["wasm"] = True
      log.append(f"emulator  build/web/firmware.wasm  {n // 1024} KiB")
    except BuildError as e:
      log.append(f"emulator: compile failed\n{e}")
    if info["wasm"]:  # (a compile error is reported once)
      ok = True
      for t in targets(cfg):
        if dev_mode and t["board"] != "fm1":
          log.append(f"{t['model']:<12} skipped in dev (./fm1 build makes it)")
          continue
        try:
          notes = build_fm1(cfg, t)
          info["packages"][t["model"]] = t["site"]
          log.append(f"{t['model']:<12} build/{t['pkg']}  identity {t['product']}  ({'; '.join(notes)})")
        except BuildError as e:
          ok = False
          log.append(f"{t['model']}: {e}")
          break  # (the same error for every model)
      info["fwsc"] = ok and "FM-1" in info["packages"]
  except BuildError as e:
    log.append(str(e))
  info["ok"] = info["wasm"] and info["fwsc"]
  info["log"] = "\n".join(log)
  SITE.mkdir(parents=True, exist_ok=True)
  (SITE / "build.json").write_text(json.dumps(info))
  print(info["log"])
  print(f"{'ok' if info['ok'] else 'FAILED'} in {time.time() - t0:.1f} s")
  return info["ok"]


# ---- dev: serve the site, rebuild on change


class Quiet(SimpleHTTPRequestHandler):
  def end_headers(self):
    self.send_header("Cache-Control", "no-store")
    super().end_headers()

  def log_message(self, *a):
    pass


def snapshot():
  files = [ROOT / "firmware.json", *SRC.rglob("*"), *PLAT.rglob("*"), *WEB.rglob("*"), *SAMPLES.rglob("*"), *(ROOT / "vendor").rglob("*")]
  return {str(f): f.stat().st_mtime for f in files if f.is_file()}


def dev(port):
  build(True)
  httpd = ThreadingHTTPServer(("0.0.0.0", port), partial(Quiet, directory=str(SITE)))
  threading.Thread(target=httpd.serve_forever, daemon=True).start()
  print(f"\nemulator, installer and rescue: http://localhost:{port}  (rebuilds when you save; ctrl-c stops)\n")
  seen = snapshot()
  try:
    while True:
      time.sleep(0.4)
      now = snapshot()
      if now != seen:
        seen = now
        print("\nchanged, rebuilding ...")
        build(True)
  except KeyboardInterrupt:
    pass


def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--dev", type=int, metavar="PORT", help="serve build/web on PORT and rebuild on change")
  a = ap.parse_args()
  if not (TC / "pi32v2/bin/clang").exists():
    raise SystemExit("no JieLi toolchain here: run this through ./fm1 (it uses the Docker image)")
  if a.dev:
    dev(a.dev)
    return 0
  return 0 if build() else 1


if __name__ == "__main__":
  sys.exit(main())
