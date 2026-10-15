# Licensing

GPL-3.0-only (`LICENSE`), as Felucca, SLOOP and Dinghy. Whoever gets a firmware built from this gets its source too.

| What                                                                                                                                                                                                        | Who                                                                                                                                                                                                                                                                                                                                         |
| ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `platform/hal/`, `platform/loader/`, `platform/src/`, `platform/crt0.S`, `platform/app.ld`, `tools/fm1pkg_make.py`, `tools/lz4blk.py`, `web/fm1ota.js`, `web/fm1pkg.js`, the FM-1 parts of `tools/build.py` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89), as [Dinghy](https://github.com/zvenson/dinghy) (zvenson) and [X0X](https://github.com/charlesvestal/fm1-x0x) (Charles Vestal) carry them |
| `platform/fm1.c`                                                                                                                                                                                            | Dinghy's `platform.c` (zvenson), rewired to `fm1.h`                                                                                                                                                                                                                                                                                         |
| `platform/fm1.h`, `platform/common.c`, `platform/web.c`, `web/` (the page, the emulator), `fm1`, `Dockerfile`, `src/main.c`                                                                                 | foma                                                                                                                                                                                                                                                                                                                                        |

FOMAJAM: `src/`, `firmware.json`, and its changes to the rest, by its authors, on [foma](https://github.com/konsumer/foma).

The modelled drum machines (`tools/drums.py`, from it `src/drums.h`, and `src/drums.c`) are ported from trommelsynthesizer
in [poketrack](https://github.com/konsumer/poketrack) (zlib), by the same author.

The synth engines in `vendor/` are [Felucca](https://github.com/hugelton/Felucca)'s (Copyright (C) 2026 Leo Kuroshita
(@kurogedelic), Hügelton Instruments; see `vendor/README.md`): ANALOG, LOFI, TRIO, WHEEL, NOISE, `dsp.c`, the engine
interface and the table generator from Felucca-Engines (MIT, `vendor/felucca-engines/LICENSE`); PHASE, VOICE and
PHYS from Felucca (GPL-3.0-only), with `phys_dsp.c` from DaisySP and `phys_symp.c` from Rings (MIT,
`vendor/felucca/MIT-DaisySP.txt`, `MIT-Rings.txt`). `src/synths.c` hosts them.

## Third-party material

| What                                                                                      | Licence     | Where                                           |
| ----------------------------------------------------------------------------------------- | ----------- | ----------------------------------------------- |
| Terminus font (ter-u16n), rendered into `platform/gen/felucca_font.h`                     | SIL OFL 1.1 | `platform/gen/Terminus-LICENSE.txt`             |
| `platform/sdk/uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` from JieLi's AC79NN_SDK_V1.2.1 | Apache-2.0  | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |
| JieLi's toolchain, downloaded into the Docker image                                       | JieLi's     | <https://pkgman.jieliapp.com/s/linux-toolchain> |

**The ROM drum machines are not ours.** `tools/roms.py` downloads recordings of the TR-707, TR-606, DMX, LinnDrum,
TR-505, DR-55, SK-1, KPR-77 and Rhythm Ace's own sounds from [tidal-drum-machines](https://github.com/geikha/tidal-drum-machines), which states no license
for them, and the build puts them in the emulator and the `.fwsc` (and so on GitHub Pages and in releases). Make sure
you are comfortable distributing them; remove them from `MACHINES` in `tools/roms.py` (or delete `roms/` and build
offline) to build without.

"M-VAVE" and "FM-1" belong to their owners; "Felucca" and "Hügelton Instruments" to Hügelton Instruments. This project is not made or endorsed by them.
