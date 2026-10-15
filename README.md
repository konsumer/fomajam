# FOMAJAM

A groovebox firmware for the M-VAVE FM-1, made with [foma](https://github.com/konsumer/foma): 9 tracks, each its own
machine: a drum machine (808, 909, SDS-V, CR-78 modelled; 707, 606, DMX, LinnDrum, 505, DR-55, SK-1, KPR-77, Rhythm Ace
from their ROMs; or your own samples) with a voice per step, or a synth with a note per step: one of
[Felucca](https://github.com/hugelton/Felucca)'s engines (ANALOG, LOFI, TRIO, WHEEL, NOISE, PHASE, VOICE, PHYS), STRING
(Karplus-Strong) or SUPERSAW (the JP-8000's), with their presets. 8 patterns of 16 steps, a delay, and colour themes.

Try it in the browser, and install it on the FM-1 from Chrome or Edge: <https://konsumer.github.io/fomajam/>

## Playing it

Every page looks the same: the **header** (page, track, pattern, tempo, play/stop), the **body**, and the **knob
bar**. KNOB1..4 always sit under the screen in the same four colours, named for what they do on that page.

| Control     | Does                                                                                                        |
| ----------- | ----------------------------------------------------------------------------------------------------------- |
| `SELECT`    | the track, 1..9                                                                                             |
| `PRESETS`   | the pattern, 1..8 (while playing it changes at the bar)                                                     |
| `ALGORITHM` | the tempo                                                                                                   |
| `PLAY`      | start / stop                                                                                                |
| `REC`       | record what you play into the pattern                                                                       |
| `SAVE`      | keep the song (waits until you stop: saving pauses sound)                                                   |
| `SEL`       | the next page of knobs; on PATTERN with a synth: slide the cursor's step into the next (lit when it slides) |
| `ENV`       | clear the pattern: asks first, `OCT+` clears, `OCT-` keeps it                                               |
| `ARP`       | on PATTERN: the cursor's step's ratchet, x1 x2 x3 x4 x6 x8 (lit when it has one)                            |
| `OCT-/OCT+` | the keyboard's octave; when something asks (lit both): `OCT+` yes, `OCT-` no                                |

| Button | Page    | Keys                                     | Knobs                                                            |
| ------ | ------- | ---------------------------------------- | ---------------------------------------------------------------- |
| `SEQ`  | PATTERN | white = the 16 steps, black = tracks     | drum: STEP VOICE ACCENT MACHINE; synth: STEP NOTE ACCENT MACHINE |
| `EDIT` | SOUND   | play                                     | drum: TUNE DECAY TONE LEVEL / MACHINE VOICE EXTRA SEND           |
|        |         |                                          | synth: its two pages / ENV / PATCH (below)                       |
| `HOME` | MIX     | white = mute tracks 1..9, black = tracks | LEVEL SEND MUTE DRIVE                                            |
| `FX`   | FX      | play                                     | SEND TIME FDBK DRIVE                                             |
| `GLO`  | GLOBAL  | play                                     | TEMPO: BPM SWING LENGTH THEME / MIDI: CLOCK; the CPU load (FM-1) |

"Play" means: on a synth track the keys are a keyboard; on a drum track they are its machine's kit
(white keys BD SD CH OH CP RS LT MT HT CY RD CB TB MA CL TI, black keys BD2 SD2 CB2 and the congas; on SMP a sample
a key), and the voice or note you play is the one new steps get. With `REC` on, what you play while it runs goes into
the step that is playing; stopped, it goes into the step at the cursor, which then moves on. `REC` makes the keys play on the PATTERN page
too, so you watch the grid fill in as you play; switch it off to edit steps again.

On the PATTERN page a white key turns a step on (with the track's voice, or a synth's last note) or off, and puts the
cursor there; KNOB2 changes the voice or note at the cursor. A drum step is coloured by its voice, so kicks, snares and
hats on one track look different.

**Ratchets** (trap and witch house rolls): ARP on the PATTERN page gives the cursor's step a ratchet, round x1 x2 x3
x4 x6 x8: the step plays that many times, evenly across its length (swung with it), each as loud as the step (an
accent: all of them). It works on every track: a drum rolls, a synth retriggers its note, MIDI sends them. A step
with a ratchet does not slide. On the grid its cell is cut in its hits (x6 and x8: four, and underlined).

## Machines

MACHINE on any track picks what it plays: one of the drum machines below, or a synth engine. Two tracks on one
machine are two independent instruments. For a 303 line: ANALOG's ACID preset (MONO), accents and slides on the
steps.

### Synth engines

Felucca's engines and STRING and SUPERSAW (ours, in their form), each with its eight parameters and its presets (picking the engine
loads its first):

| Engine   | What it is                                                                                                                                                                                                                                                |
| -------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| ANALOG   | two band-limited oscillators (saw, square, triangle, sine, PWM, sync, sub), noise, drive, a resonant filter                                                                                                                                               |
| LOFI     | a chip voice: pulse, triangle, saw, shift-register noise, 4-bit wave memory, bit depth, arpeggio                                                                                                                                                          |
| TRIO     | three oscillators with ring modulation, hard sync and AND waves into a multimode filter                                                                                                                                                                   |
| WHEEL    | a tonewheel organ: nine drawbars, percussion, key click, drive, rotary                                                                                                                                                                                    |
| NOISE    | white / pink / brown noise, impulses, shift-register noise through a resonant filter, a bit crusher                                                                                                                                                       |
| PHASE    | phase distortion (CZ-style)                                                                                                                                                                                                                               |
| VOICE    | formants: vowels, a choir, a talking bass                                                                                                                                                                                                                 |
| PHYS     | physical models: modal bars and bells, strings, membranes, sympathetic strings                                                                                                                                                                            |
| STRING   | a plucked string (Karplus-Strong): noise, pick or soft burst, pluck position, damping, decay, bow                                                                                                                                                         |
| SUPERSAW | the JP-8000's super saw after Adam Szabo's measurements: seven free-running saws, his detune and mix curves, the high-pass at the fundamental; a resonant low-pass, DRFT (tape wow) and CRSH (bits, rate) for the dark and worn: WITCH, DRAG CHOIR, GHOST |

On a synth engine the SOUND page has five pages of knobs, SEL moves along the tabs: the engine's two (its own names:
OSC / FLT on ANALOG), ENV (ATTACK DECAY SUSTN RELEASE), PATCH (PRESET, ENV>FLT: the envelope's sweep of the filter,
LEVEL, SEND) and PLAY (GATE, MONO, GLIDE, TUNE). GATE is how much of its step a sequenced note is held, or TIE: held
until the track's next note, which takes over from it (a pad line; with MONO on it glides on, no new attack). Picking
a preset sets it: TIE for a slow attack (a pad), else half the step. An accented step opens the filter, a step with
slide glides into the next. Each track has two voices; at most eight synth voices sound at once, the oldest giving
way.

### Drum machines

On a drum machine each step plays one of its voices: BD BD2 SD SD2 RS CP LT MT HT CH OH CY RD
CB CB2 TB, the congas, TI MA CL. A machine plays the voices it had; one it lacks is played by its nearest neighbour,
retuned (a DR-55 has no open hat, so its closed one plays it), and VOICE skips what it cannot play at all. A closed hat
chokes an open one on the same track. Combine machines by giving tracks different ones: a 909 kick, 808 claps, 707
tambourine.

| Machine                                         | How                                                                  |
| ----------------------------------------------- | -------------------------------------------------------------------- |
| 808, 909, SDS (SDS-V), CR78                     | modelled from how the circuits work, as in trommelsynthesizer        |
| 707, 606, DMX, LINN, 505, DR55, SK1, KPR77, ACE | recordings of their own ROM samples, 4-bit ADPCM, tails cut at 0.8 s |
| SMP                                             | your `samples/` (when there are any)                                 |

The knobs mean the same on every machine: TUNE (+-12 semitones), DECAY (64 = the machine's own), TONE (64 = neutral:
the voice's own tone control; on a recording lower is darker, higher brighter), LEVEL, and EXTRA: ATTACK on a kick,
SNAPPY on a snare.

The ROM machines are in [tools/roms.py](tools/roms.py) (`MACHINES`): the build downloads their recordings from
[tidal-drum-machines](https://github.com/geikha/tidal-drum-machines) (pinned to a commit) into `roms/` once, and goes
on without them when it can't. They go in in the order there until `BUDGET` (430 KiB) is spent, and the build says
which were left out (LM-1, Space Drum, RZ-1, DR-110, Drumulator, RX5, HR-16, SDS-5, Drumtraks and MPC60 are listed and
wait): move one up to have it. See [LICENSING.md](LICENSING.md) about these recordings.

### MIDI

MIDI in: each track has its own channel: channels 1..9 play tracks 1..9 (whatever machine they are: a keyboard plays
any of them), channel 10 the track chosen. A drum machine takes General MIDI drum keys, a synth notes.

**MIDI out** is a machine (MIDI, `MID` on the grid): its steps are sent, not heard. A step is a note on the track's
channel, at its VELO (an accent: 127), let go after GATE % of the step; a step with slide is sent legato (the next
note before this one's note-off), for the synth at the other end to glide. Its SOUND page has three pages of knobs:
NOTE (CHAN, PROG: a program change, sent as you turn it and as the song starts; GATE, or TIE: held to the next
note; VELO), CC (four controllers,
sent as you turn them: CC74 CC71 CC1 CC10 unless you change them) and CC# (which controllers they are). Keys and MIDI
in on a MIDI track go out on its channel too (a keyboard through the FM-1 to another synth).

GLOBAL's second page (SEL) has CLOCK: MIDI clock out (24 a beat, with start and stop) while it plays.

## Samples

Put `.wav` files in `samples/`: any PCM wav (8/16/24-bit, mono or stereo, any rate). The build makes them 16-bit mono
at 22050 Hz, up to 2 seconds each, and they become the SMP machine: a voice per file, named by it (`01-boom.wav` is
`BOOM`). They share the flash with the ROM machines: drop a ROM machine for room.

## Themes

[src/themes.h](src/themes.h): one per FM-1 finish (BLACK, GREY, BLUE, GREEN, FOREST, PURPLE, RED, ORANGE, TEAL) and two
spares (WHITE, a light screen, and NEON). THEME on the GLOBAL page recolours the screen as you turn it. A theme is a dozen colours;
add one there and it is on the GLOBAL page. The eight track colours also colour the drum voices (kicks the first,
snares the second, claps the third, closed hats the fourth ...).

## Develop

Only Docker is needed:

```sh
./fm1 dev          # http://localhost:8000: the emulator, the installer, the rescue; rebuilds when you save
./fm1 build        # build/fomajam-VERSION.fwsc
```

| File                                     |                                                                  |
| ---------------------------------------- | ---------------------------------------------------------------- |
| [src/main.c](src/main.c)                 | the pages, the knobs, the keys, the song to start with           |
| [src/drums.c](src/drums.c)               | the drum machines: the voice kinds, the recordings, choke        |
| [src/engine.c](src/engine.c)             | the clock, the delay, the output                                 |
| [src/synths.c](src/synths.c)             | the host of Felucca's synth engines (`vendor/`)                  |
| [src/eng_string.h](src/eng_string.h)     | STRING, an engine in Felucca's form                              |
| [src/eng_supersaw.h](src/eng_supersaw.h) | SUPERSAW, an engine in Felucca's form                            |
| [src/groove.h](src/groove.h)             | the song (it is also what is saved: 4 KiB at most)               |
| [src/drums.h](src/drums.h)               | the modelled machines as integers, made by `tools/drums.py`      |
| [src/gains.h](src/gains.h)               | each modelled voice's level, made by `test/render.c --calibrate` |
| [src/themes.h](src/themes.h)             | the colours                                                      |
| [src/tables.h](src/tables.h)             | curves, made by `python3 tools/tables.py`                        |
| [tools/samples.py](tools/samples.py)     | `samples/*.wav` to `build/gen/samples.h` (the build runs it)     |
| [tools/roms.py](tools/roms.py)           | the ROM machines to `build/gen/roms.h` (the build runs it)       |
| [test/render.c](test/render.c)           | the sound on the computer: checks, calibration, .wav renders     |

To hear and check the sound without the FM-1 (after a build, which makes `build/gen/`):

```sh
cc -O2 -Isrc -Iplatform -Ibuild/gen -Ivendor/felucca-engines/src -Ivendor/felucca -D__wasm__ \
  -o build/render test/render.c src/{drums,engine,main,synths}.c -lm
build/render --check          # every voice of every machine sounds, stays finite, dies away
build/render --wav /tmp/kit   # a .wav per machine and voice
build/render --synths         # every preset of every synth engine sounds, stays finite, dies away
build/render --song song.wav  # 8 s of the song it starts with (add N for pattern N)
build/render --midi           # a MIDI track and the clock through a bar: the messages and when
```

What the firmware can call is in [platform/fm1.h](platform/fm1.h). Name, version and package identity:
[firmware.json](firmware.json). The rules of the chip (no floats, keep `audio()` short, ...) are in
[foma's README](https://github.com/konsumer/foma#the-rules-of-the-chip).

Every push to `main` publishes the emulator and the installer to GitHub Pages (Settings → Pages → Source: GitHub
Actions); a tag (`v0.2`) also makes a release with the `.fwsc`.

Installing firmware is at your own risk. If the FM-1 does not start, hold OCT- while switching it on (the USB rescue),
then install from the page.

## License

GPL-3.0-only. Credits and third-party parts: [LICENSING.md](LICENSING.md).
