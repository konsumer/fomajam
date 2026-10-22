# FOMAJAM

A groovebox firmware for the M-VAVE FM-1 (and the [M-VAVE SMK-37](#m-vave-smk-37)), made with [foma](https://github.com/konsumer/foma).

- 9 tracks, each playing its own machine:
  - **drum machines:** modeled 808, 909, SDS-V and CR-78; the 707, 606, DMX, LinnDrum, 505, DR-55, SK-1, KPR-77 and
    Rhythm Ace from recordings of their ROMs; or your own samples
  - **synths:** [Felucca](https://github.com/hugelton/Felucca)'s engines (ANALOG, LOFI, TRIO, WHEEL, NOISE, PHASE,
    VOICE, PHYS), plus STRING (Karplus-Strong) and SUPERSAW (JP-8000 style), all with presets
  - **MIDI out**, to sequence other gear
- 40 patterns of 16 steps, with accents, slides and ratchets
- effects: delay, reverb and chorus sends, kick ducking, a master filter and 11 kinds of distortion
- MIDI in over USB and the TRS jack, MIDI clock in and out, and color themes

Try it in your browser, and install it on the FM-1 from Chrome or Edge: <https://konsumer.github.io/fomajam/>

## Playing it

Every page has the same layout: a **header** (page, track, pattern, tempo, play/stop), the **body**, and the **knob
bar**. The knob bar shows what KNOB1–4 do on the current page, always in the same four colors.

| Control     | What it does                                                                                   |
| ----------- | ---------------------------------------------------------------------------------------------- |
| `SELECT`    | Choose the track (1–9).                                                                        |
| `PRESETS`   | Choose the pattern (1–40). While playing, the one you stop on plays when the current one ends. |
| `ALGORITHM` | Set the tempo.                                                                                 |
| `PLAY`      | Start or stop.                                                                                 |
| `REC`       | Record what you play into the pattern.                                                         |
| `SAVE`      | Save the song. If it's playing, it saves when you stop (saving pauses the sound).              |
| `SEL`       | Show the next page of knobs. On PATTERN with a synth track: toggle slide on a step.            |
| `ARP`       | On PATTERN: set the ratchet of the step under the cursor.                                      |
| `ENV`       | Clear the pattern. It asks first: `OCT+` clears, `OCT-` cancels.                               |
| `OCT-/OCT+` | Change the keyboard's octave. When a question is shown, `OCT+` is yes, `OCT-` is no.           |

| Button       | Page                | Keys                                            | Knobs                                                                                                                                    |
| ------------ | ------------------- | ----------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `SEQ`        | PATTERN             | white keys: the 16 steps; black keys: tracks    | STEP, VOICE (or NOTE), ACCENT, MACHINE                                                                                                   |
| `EDIT`       | SOUND               | play the track                                  | the track's sound (see [Machines](#machines))                                                                                            |
| `HOME`       | MIX                 | white keys: mute tracks 1–9; black keys: tracks | LEVEL, DELAY, REVERB, MUTE                                                                                                               |
| `HOME` again | SCOPE, then MIDI IN | play the track                                  | the MIX knobs. SCOPE shows the output; MIDI IN lists the last messages that came in. `HOME` once more goes back to MIX.                  |
| `FX`         | FX                  | play the track                                  | 4 pages (press `SEL`): see [Effects](#effects)                                                                                           |
| `GLO`        | GLOBAL              | play the track                                  | TEMPO page: BPM, SWING, LENGTH, THEME; SETUP page: CLOCK, CC OUT, LIGHTS; MIDI page: MIDI IN, SYNC, SEL CH, TRK CH. Also shows CPU load. |

**Playing the keys:** on a synth track, the keys are a keyboard. On a drum track, they play the machine's kit: the
white keys are BD SD CH OH CP RS LT MT HT CY RD CB TB MA CL TI, and the black keys are BD2 SD2 CB2 and the congas. (On
the SMP machine, each key plays one sample.) The last voice or note you play is what new steps use.

**Recording:** with `REC` on, what you play goes into the step that is playing. When stopped, it goes into the step
under the cursor, and the cursor moves to the next step. `REC` also makes the keys play on the PATTERN page, so you can
watch the grid fill in. Turn it off to go back to editing steps.

**Editing steps:** on the PATTERN page, a white key turns a step on or off and moves the cursor to it. A new step uses
the track's current voice (or a synth's last note); KNOB2 changes it. Drum steps are colored by voice, so kicks,
snares and hats on the same track look different.

**Ratchets** (the rolls in trap and witch house): press `ARP` on the PATTERN page to cycle the step under the cursor
through ×1, ×2, ×3, ×4, ×6 and ×8. The step then plays that many times, evenly spaced across the step (and swung with
it). Every repeat is as loud as the step, so an accented step accents the whole roll. Ratchets work on every track:
drums roll, synths retrigger their note, and MIDI tracks send the repeats. A ratcheted step doesn't slide. On the grid,
the cell is split into its repeats (×6 and ×8 show four segments with a line underneath).

## Effects

Each track sends to a delay, a reverb and a chorus. The mix then goes through the master filter and the distortion.
The FX page has four pages of knobs (press `SEL` to move on), and each page draws what its knobs do:

| Page   | Knobs                       | What they do                                                                                                       |
| ------ | --------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| SENDS  | DELAY, REVERB, CHORUS, DUCK | the selected track's sends, and how much track 1 ducks it (track 1 shows `KEY`: it does the ducking)               |
| SPACE  | TIME, FDBK, SIZE, DAMP      | the delay's time (in 16ths) and feedback; the reverb's size and how dark it gets                                   |
| MOTION | RATE, DEPTH, PUMP           | the chorus's speed and depth; how fast a ducked track comes back                                                   |
| MASTER | FILTER, RESO, DRIVE, TYPE   | one-knob filter (left of center low-pass, right high-pass, center off), its resonance, the distortion and its kind |

**Ducking** gives the pumping sound of trap and witch house: put the kick on track 1 and turn up DUCK on the bass and
pads. **Distortion types:** SOFT (a gentle saturation, the default), HARD (clipping), TUBE (warm, uneven), DIODE (soft
above, hard below), FUZZ, FOLD (wavefolding), WAVE (rounder folding), RECT (rectified: an octave up), BITS (fewer
bits), RATE (lower sample rate) and CRUSH (both).

## Machines

`MACHINE` sets what a track plays: a drum machine, a synth engine or MIDI out. Two tracks on the same machine are two
separate instruments. For a 303-style bassline, use ANALOG's ACID preset with MONO on, and add accents and slides to
the steps.

### Synth engines

Each engine has eight parameters and a set of presets. Choosing an engine loads its first preset.

| Engine   | What it is                                                                                                                                                                                                                                                                                          |
| -------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| ANALOG   | Two band-limited oscillators (saw, square, triangle, sine, PWM, sync, sub), noise, drive and a resonant filter.                                                                                                                                                                                     |
| LOFI     | A chip-style voice: pulse, triangle, saw, shift-register noise, 4-bit wavetables, bit depth and arpeggio.                                                                                                                                                                                           |
| TRIO     | Three oscillators with ring modulation, hard sync and AND waves, into a multimode filter.                                                                                                                                                                                                           |
| WHEEL    | A tonewheel organ: nine drawbars, percussion, key click, drive and rotary speaker.                                                                                                                                                                                                                  |
| NOISE    | White, pink and brown noise, impulses and shift-register noise through a resonant filter, with a bit crusher.                                                                                                                                                                                       |
| PHASE    | Phase distortion, in the style of Casio's CZ synths.                                                                                                                                                                                                                                                |
| VOICE    | Formant synthesis: vowels, choirs and a talking bass.                                                                                                                                                                                                                                               |
| PHYS     | Physical models: struck bars and bells, strings, membranes and sympathetic strings.                                                                                                                                                                                                                 |
| STRING   | A plucked string (Karplus-Strong): pick and pluck position, damping, decay and a bowing option.                                                                                                                                                                                                     |
| SUPERSAW | The JP-8000 super saw, based on Adam Szabo's measurements: seven free-running saws, his detune and mix curves, and a high-pass at the note's fundamental. A resonant low-pass, DRFT (tape wobble) and CRSH (lower bit depth and sample rate) make it dark and worn. Try WITCH, DRAG CHOIR or GHOST. |

STRING and SUPERSAW are written for this firmware in the same form as Felucca's engines.

On a synth track, the SOUND page has five pages of knobs (`SEL` steps through the tabs):

| Page                 | Knobs                                                                  |
| -------------------- | ---------------------------------------------------------------------- |
| the engine's 2 pages | its own parameters (for ANALOG: OSC and FLT)                           |
| ENV                  | ATTACK, DECAY, SUSTN (sustain), RELEASE                                |
| PATCH                | PRESET, ENV>FLT (how far the envelope sweeps the filter), LEVEL, DELAY |
| PLAY                 | GATE, MONO, GLIDE, TUNE                                                |

`GATE` sets how much of its step a sequenced note is held. At `TIE`, a note is held until the track's next note, which
takes over from it; use this for pad lines. With MONO on, a tied note glides into the next one without a new attack.
Choosing a preset sets GATE for you: TIE for slow-attack presets (pads), otherwise half a step.

An accented step opens the filter, and a step with slide (`SEL` on the PATTERN page) glides into the next one, even
with MONO off. Each track has two voices, and at
most eight synth voices play at once (the oldest gives way).

### Drum machines

On a drum machine, each step plays one voice: BD BD2 SD SD2 RS CP LT MT HT CH OH CY RD CB CB2 TB, the congas, TI MA
CL. If a machine doesn't have a voice, its closest voice plays it instead, retuned (for example, the DR-55 has no open
hi-hat, so its closed hi-hat stands in). `VOICE` skips voices a machine can't play at all. A closed hi-hat cuts off
an open one on the same track. To mix machines, give tracks different ones: a 909 kick, 808 claps, a 707 tambourine.

| Machine                                         | How it's made                                                            |
| ----------------------------------------------- | ------------------------------------------------------------------------ |
| 808, 909, SDS (SDS-V), CR78                     | modeled on how the circuits work, ported from trommelsynthesizer         |
| 707, 606, DMX, LINN, 505, DR55, SK1, KPR77, ACE | recordings of their own ROM samples, as 4-bit ADPCM, cut at 0.8 s        |
| SMP                                             | your own samples from `samples/` (only shown when there are any)         |
| your kits (after MIDI)                          | up to four of your own, loaded without a new firmware: see [Kits](#kits) |

The knobs work the same on every drum machine:

- **TUNE:** ±12 semitones.
- **DECAY:** 64 is the machine's own decay.
- **TONE:** 64 is neutral. On modeled machines it is the voice's tone control; on recordings, lower is darker and
  higher is brighter.
- **LEVEL**
- **EXTRA:** ATTACK on a kick, SNAPPY on a snare.

The ROM machines are listed in [tools/roms.py](tools/roms.py) (`MACHINES`). The first build downloads their
recordings from [tidal-drum-machines](https://github.com/geikha/tidal-drum-machines) (pinned to one commit) into
`roms/`. Without a network connection, the build continues without them.

Machines are added in the order they're listed until `BUDGET` (430 KiB of flash) is used up, and the build tells you
which ones didn't fit. LM-1, Space Drum, RZ-1, DR-110, Drumulator, RX5, HR-16, SDS-5, Drumtraks and MPC60 are listed
but don't fit; to include one, move it higher in the list. See [LICENSING.md](LICENSING.md) about these recordings.

### MIDI

**MIDI in:** MIDI comes in over USB and the 3.5 mm TRS jack (you need a TRS-to-DIN adapter for the jack). Each
track has its own channel. Channels 1–9 play tracks 1–9, whatever machine they use, so a keyboard can play any of
them. Channel 10 plays the selected track. Drum machines respond to General MIDI drum notes; synths respond to any
note.

The GLOBAL page's third page (press `SEL` twice) sets up MIDI in:

| Knob    | What it does                                                                                     |
| ------- | ------------------------------------------------------------------------------------------------ |
| MIDI IN | where MIDI comes from: `BOTH`, `USB` or `TRS`. Everything else is ignored, the clock too.        |
| SYNC    | `INT`: the FM-1 keeps its own tempo. `EXT`: it follows the MIDI clock coming in (see below).     |
| SEL CH  | the channel that plays the selected track (10 at first), or `OFF`                                |
| TRK CH  | the channel the selected track listens on (its number at first), or `OFF`. Tracks can share one. |

**MIDI controllers:** every parameter also has a fixed MIDI CC, on the same channels (channel 16 is the song and
effects), so a controller with lots of knobs reaches everything without menus. See [CC.md](CC.md) for the map. With
`CC OUT` on (GLOBAL page, SETUP page), changes made on the FM-1 are sent back out so a controller's lights follow.

**MIDI out** is a machine (MIDI, shown as `MID` on the grid). Its steps are sent over USB MIDI instead of making a
sound:

- Each step sends a note on the track's channel at its velocity (127 if accented).
- The note-off comes after GATE % of the step, or at the next note with `TIE`.
- A step with slide is sent legato (the next note-on comes before this note-off), so the receiving synth can glide.
- Keys and MIDI in on a MIDI track are sent out on its channel too, so you can play another synth through the FM-1.

Its SOUND page has three pages of knobs:

| Page | Knobs                                                                                          |
| ---- | ---------------------------------------------------------------------------------------------- |
| NOTE | CHAN, PROG (a program change, sent when you turn it and when the song starts), GATE, VELO      |
| CC   | four MIDI controllers, sent as you turn them (CC74, CC71, CC1 and CC10 unless you change them) |
| CC#  | which controller numbers the CC page sends                                                     |

**MIDI clock:** the GLOBAL page's second page (press `SEL`) has `CLOCK`. When it's on, the FM-1 sends MIDI clock
(24 ticks per beat) with start and stop messages.

With `SYNC` on `EXT`, the FM-1 follows the clock from `MIDI IN` instead: a step every 6 ticks, and start, stop and
continue from the other device. `PLAY` still works, but nothing moves until the clock does. The header shows the
clock's tempo (`--` until one comes), and the delay time and `SWING` follow it; `BPM` waits until you go back to
`INT`. With `CLOCK` on too, the ticks are passed on over USB, so a computer can follow a drum machine on the TRS jack.
Pick one source in `MIDI IN` when both send clock.

## Samples

Put `.wav` files in `samples/`. Any PCM WAV works (8, 16 or 24-bit, mono or stereo, any sample rate). The build
converts them to 16-bit mono at 22050 Hz, up to 2 seconds each, and they become the SMP machine: one voice per file,
named after it (`01-boom.wav` becomes `BOOM`). Samples share flash with the ROM machines, so you may need to remove a
ROM machine to make room. To change samples without building a firmware, use [kits](#kits).

## Kits

Up to four drum machines of your own, loaded onto the FM-1 from `.wav` files (by the page's Songs tab, or
fm1buddy) without installing a new firmware. They come after MIDI on the MACHINE knob, each with its own name.

A kit is a folder of `.wav` files, and the folder's name is the machine's (`Boom Kit` becomes `BOOMKIT`). Name the
files after drum voices (`bd.wav`, `bd2.wav`, `sd.wav`, `rs.wav`, `cp.wav`, `lt.wav`, `mt.wav`, `ht.wav`, `ch.wav`,
`oh.wav`, `cy.wav`, `rd.wav`, `cb.wav`, `tb.wav`, ...) and it's a drum kit, played like the others: the voices it
lacks are played by the ones it has, as with the ROM machines. Otherwise it plays one file a key, like SMP.

They're kept in 288 KiB of the FM-1's flash (100 KiB on the SMK-37), compressed as the ROM machines are: about 25
seconds in all. Loading them pauses the sound for a few seconds, and waits until the FM-1 is stopped. Installing
another firmware may lose them: keep them in a setup (below) to load them again.

## Songs

Keep a setup for each song you play, and load it before you play it: the song (everything `SAVE` keeps: the
patterns, the tracks' sounds, the effects and the settings) and its [kits](#kits). Loading one replaces them on the
FM-1 and saves them; it waits until the FM-1 is stopped.

A setup is a folder: `fomajam.bin` (the song), and its kits as folders of `.wav` files in `kits/`, or as they were
saved from the FM-1 (`fomajam-kits.bin`).

- On a computer: the **Songs** tab of [the page](https://konsumer.github.io/fomajam/#songs), with the FM-1 on USB or
  with the emulator. Choose a folder to keep the setups in, such as an SD card's `setups`.
- Without one: [fm1buddy](https://github.com/konsumer/fm1buddy) keeps them on its SD card, in `/setups`, the same way.

For other gear, **Export** (on the page, or fm1buddy) writes a setup's patterns as MIDI files in `midi/` (one for each
pattern with steps: drum tracks on channel 10 with General MIDI drum notes, the others on channels 1-9, with the
song's tempo, swing, accents, ratchets and slides) and its kits as `.wav` files in `kits/` (which load back as the same
kits). [web/fm1song.js](web/fm1song.js) and [web/fm1bank.js](web/fm1bank.js) say how.

How they talk is in [platform/src/share.c](platform/src/share.c) (and [web/fm1share.js](web/fm1share.js) for the other
side), so another helper can do the same.

## Themes

The themes are in [src/themes.h](src/themes.h): one for each FM-1 color (BLACK, GREY, BLUE, GREEN, FOREST, PURPLE,
RED, ORANGE, TEAL), plus WHITE (a light screen) and NEON. `THEME` on the GLOBAL page changes the colors as you turn it.
A theme is about a dozen colors; add one to the file and it appears on the GLOBAL page. The eight track colors also
color the drum voices: kicks use the first, snares the second, claps the third, closed hi-hats the fourth, and so on.

## USB audio

Over one USB cable, the FM-1 is a class-compliant audio and MIDI device. It needs no driver on a computer, an iPad
or a phone.

- **Audio to the FM-1:** a 44.1 kHz stereo output. Pick the FM-1 as the output on your computer or iPad, and its
  sound plays through the FM-1 mixed with the FM-1's own. The MASTER knob sets the volume of both.
- **MIDI in and out:** see above.

## Key lights

`LIGHTS` on the GLOBAL page's SETUP page (press `SEL`) dimly lights every key and button so the labels are easy to
find in the dark, or on an FM-1 with dark keys: `OFF`, `LOW` or `FULL`. Lit keys and buttons still stand out above it.
It's saved with the song.

## M-VAVE SMK-37

The SMK-37 (Pro, Elite, and the MKE-P37, which is the same keyboard) has the FM-1's chip, so FOMAJAM runs on it too.
Each model has its own package, because M-VAVE's updater only takes a keyboard's own model:
`fomajam-smk37-VERSION.fwsc` (SMK-37 Pro, also the Donner Starrykey 37 Play), `fomajam-smk37-elite-VERSION.fwsc` and
`fomajam-smk37-mke-VERSION.fwsc`. The install page picks the right one for the connected keyboard.

The FM-1's controls are on the SMK-37's panel, so everything above applies:

| FM-1           | SMK-37                                                                   |
| -------------- | ------------------------------------------------------------------------ |
| MASTER         | K1                                                                       |
| `SELECT`       | K2                                                                       |
| `ALGORITHM`    | K3                                                                       |
| `PRESETS`      | K4                                                                       |
| KNOB1–4        | FADER1–4 (each sets the control's whole range), and K5–K8 (step by step) |
| `SEQ`          | SEQ (PATTERN)                                                            |
| `EDIT`         | PATCH (SOUND)                                                            |
| `HOME`         | PARA (MIX, SCOPE, MIDI IN)                                               |
| `FX`           | FX                                                                       |
| `GLO`          | GLOBE (GLOBAL)                                                           |
| `SEL`          | SCALE, or KNOB BANK (KNOB BANK never toggles a slide)                    |
| `ARP`          | NOTE REPEAT (the ratchet)                                                |
| `ENV`          | CHORD (clear the pattern)                                                |
| `SAVE`         | BT                                                                       |
| `PLAY` / `REC` | SEQ PLAY / SEQ REC                                                       |
| `OCT-/OCT+`    | OCTAVE- / OCTAVE+                                                        |
| the 27 keys    | the middle 27 keys (F3–G5)                                               |
| the white keys | also the 16 pads, top row first: on PATTERN, the steps                   |

A fader takes over once you move it (also after a page, track or bank change), so nothing jumps. And what only the
SMK-37 has:

| SMK-37        | What it does                                                                                                     |
| ------------- | ---------------------------------------------------------------------------------------------------------------- |
| keys and pads | play with velocity (the keys use the keyboard's own factory calibration)                                         |
| PITCH wheel   | bends the chosen track ±2 semitones; on a MIDI track it sends pitch bend                                         |
| MOD wheel     | vibrato on the chosen track; on a MIDI track it sends CC 1                                                       |
| FADER BANK    | makes the faders a mixer: the levels of tracks 1–4, then 5–8, then 9, then back to the screen (lit while mixing) |
| `<` / `>`     | the pattern before or after (playing, it plays when the current one ends)                                        |
| pad colors    | the screen's: steps in their track or drum-voice color, the playhead white, the MIX page in track colors         |

The black keys' lights (the tracks on the PATTERN page) have no place on the SMK-37. Not used yet: ARP (there is no
arpeggiator), the outer 10 keys and the pedal. The SMK-37 has no MIDI in on its TRS jack, so MIDI in is USB only.

The SMK-37 build has not been tested on a keyboard yet. It uses the hardware map and board code of the
[SMK-37 port kit](https://github.com/genkigenki/fm1-to-smk37) (genkigenki), which other FM-1 firmwares run on, and it
never writes the SMK-37's factory calibration or M-VAVE's own data. Hold OCTAVE- while switching on for the USB
rescue. To go back to the official firmware, use M-VAVE's updater for your model.

## Develop

You only need Docker:

```sh
./fm1 dev          # http://localhost:8000: the emulator, installer and rescue; rebuilds when you save
./fm1 build        # build/fomajam-VERSION.fwsc, and the SMK-37 packages (firmware.json "boards")
```

| File                                     | What it is                                                            |
| ---------------------------------------- | --------------------------------------------------------------------- |
| [src/main.c](src/main.c)                 | the pages, knobs and keys, and the song it starts with                |
| [src/drums.c](src/drums.c)               | the drum machines: voice models, recordings, hi-hat choke             |
| [src/engine.c](src/engine.c)             | the sequencer clock, MIDI out, the mix, the delay and the output      |
| [src/fx.c](src/fx.c)                     | the reverb, chorus, master filter and distortion                      |
| [src/synths.c](src/synths.c)             | the host for the synth engines (Felucca's are in `vendor/`)           |
| [src/eng_string.h](src/eng_string.h)     | the STRING engine                                                     |
| [src/eng_supersaw.h](src/eng_supersaw.h) | the SUPERSAW engine                                                   |
| [src/groove.h](src/groove.h)             | the song format (also what gets saved, 4 KiB at most)                 |
| [src/drums.h](src/drums.h)               | the modeled drum machines as integer tables, made by `tools/drums.py` |
| [src/gains.h](src/gains.h)               | each modeled voice's level, made by `test/render.c --calibrate`       |
| [src/themes.h](src/themes.h)             | the color themes                                                      |
| [src/tables.h](src/tables.h)             | lookup tables, made by `python3 tools/tables.py`                      |
| [tools/samples.py](tools/samples.py)     | converts `samples/*.wav` to `build/gen/samples.h` (run by the build)  |
| [tools/roms.py](tools/roms.py)           | builds the ROM machines into `build/gen/roms.h` (run by the build)    |
| [test/render.c](test/render.c)           | renders and checks the sound on your computer                         |

To hear and check the sound without an FM-1 (after a build, which creates `build/gen/`):

```sh
cc -O2 -Isrc -Iplatform -Ibuild/gen -Ivendor/felucca-engines/src -Ivendor/felucca -D__wasm__ \
  -o build/render test/render.c src/{drums,engine,fx,main,synths}.c -lm
build/render --check          # every drum voice sounds, stays in range and dies away
build/render --wav /tmp/kit   # a .wav for every machine and voice
build/render --synths         # the same checks for every synth preset
build/render --song song.wav  # 8 seconds of the starting song (add N for pattern N)
build/render --midi           # the MIDI messages a MIDI track and the clock send in one bar, with their times
build/render --share          # the song and kits read and written as fm1buddy does it
build/render --bank FILE      # a kits file (fomajam-kits.bin) played
```

`node test/fm1ota_test.mjs` checks that the installer never writes a package to another model (an FM-1 package to an
SMK-37, or the other way).

`node test/share_test.mjs` reads and writes the song and kits in the emulator's firmware the way the Songs tab does,
losing some messages on the way (and, with `build/render`, checks that the firmware plays the kits it made).

The functions the firmware can call are in [platform/fm1.h](platform/fm1.h): it's [foma](https://github.com/konsumer/foma)'s
platform, with its USB audio, TRS MIDI and MIDI clock options on. The name, version, package identity and those options
are in [firmware.json](firmware.json). The rules of the chip (no floating point, keep `audio()` short, and so on) are
in [foma's README](https://github.com/konsumer/foma#the-rules-of-the-chip).

Every push to `main` publishes the emulator and installer to GitHub Pages (Settings → Pages → Source: GitHub Actions).
Pushing a tag (like `v0.2`) also creates a release with the `.fwsc` files (the FM-1's and the SMK-37's).

Installing firmware is at your own risk. If the FM-1 doesn't start, hold OCT- while switching it on to enter USB
rescue mode, then install from the page.

## License

GPL-3.0-only. For credits and third-party parts, see [LICENSING.md](LICENSING.md).
