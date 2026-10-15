# vendor

Synthesis engines from [Felucca](https://github.com/hugelton/Felucca) (Leo Kuroshita, Hügelton Instruments), which
`src/synths.c` hosts. Copied unchanged except where a file says `fomajam:` (`eng_analog.c`: OSC 2 is not computed at MIX 0, the same
output; `eng_phys.c`: the host may set PHYS_POLY).

| Folder             | From                                                                                                                                                   | Licence                                                                                        |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------- |
| `felucca-engines/` | [Felucca-Engines](https://github.com/hugelton/Felucca-Engines) at `7511c0a`: ANALOG LOFI TRIO WHEEL NOISE, `dsp.c`, the interface, the table generator | MIT (`felucca-engines/LICENSE`)                                                                |
| `felucca/`         | [Felucca](https://github.com/hugelton/Felucca) `firmware/src/` at `a53ecf4`: PHASE, VOICE (`eng_formant.c`), PHYS                                      | GPL-3.0-only; `phys_dsp.c` (DaisySP) and `phys_symp.c` (Rings) MIT, their licences beside them |
