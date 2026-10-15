# MIDI CC map

Every parameter you can change on the FM-1 also has a MIDI CC, so a controller with lots of knobs can reach them
without paging through menus. The numbers are fixed: set your controller's knobs to send the CCs below. This works
the same on the FM-1 (over USB MIDI) and in the web emulator (pick your controller under "MIDI in").

**Channels:**

| Channel | Controls                                                                             |
| ------- | ------------------------------------------------------------------------------------ |
| 1–9     | tracks 1–9                                                                           |
| 10      | the selected track (whichever one is on screen), so one set of knobs edits any track |
| 16      | the song: tempo, effects, transport                                                  |

**Values:** each CC's 0–127 covers the parameter's whole range. On/off parameters turn on at 64 and above. A list
(MACHINE, TYPE, THEME, ...) is spread across 0–127. VOICE picks the nearest voice the machine has. The FM-1 shows the
parameter and its new value for a moment when a CC arrives.

A CC for a parameter the track's machine doesn't have is ignored. For example, a drum track has no ATTACK, and only
MIDI tracks have CHAN.

## Track parameters (channels 1–10)

| CC  | Parameter | Machines    | Notes                                                                      |
| --- | --------- | ----------- | -------------------------------------------------------------------------- |
| 3   | PRESET    | synth       | loads the preset                                                           |
| 5   | GLIDE     | synth       | how fast slides glide                                                      |
| 7   | LEVEL     | all         |                                                                            |
| 9   | MUTE      | all         |                                                                            |
| 14  | MACHINE   | all         | a synth engine starts on its first preset                                  |
| 15  | VOICE     | drum, synth | a drum track's voice; a synth's note for new steps                         |
| 20  | engine 1  | synth       | the engine's 8 parameters: see [below](#synth-engine-parameters-cc-2027)   |
| 21  | engine 2  | synth       |                                                                            |
| 22  | engine 3  | synth       |                                                                            |
| 23  | engine 4  | synth       |                                                                            |
| 24  | engine 5  | synth       |                                                                            |
| 25  | engine 6  | synth       |                                                                            |
| 26  | engine 7  | synth       |                                                                            |
| 27  | engine 8  | synth       |                                                                            |
| 28  | TUNE      | drum, synth |                                                                            |
| 29  | DECAY     | drum        |                                                                            |
| 30  | TONE      | drum        |                                                                            |
| 31  | EXTRA     | drum        | ATTACK on kicks, SNAPPY on snares                                          |
| 70  | SUSTAIN   | synth       |                                                                            |
| 72  | RELEASE   | synth       |                                                                            |
| 73  | ATTACK    | synth       |                                                                            |
| 75  | DECAY     | synth       | the envelope's                                                             |
| 85  | ENV>FLT   | synth       | 64 is none, below sweeps down, above sweeps up                             |
| 86  | GATE      | synth, MIDI | the top of the range is TIE                                                |
| 87  | MONO      | synth       |                                                                            |
| 89  | DUCK      | all         | how much track 1 ducks this track                                          |
| 91  | REVERB    | all         | send                                                                       |
| 93  | CHORUS    | all         | send                                                                       |
| 94  | DELAY     | all         | send                                                                       |
| 102 | CHAN      | MIDI        | the channel it sends on                                                    |
| 103 | PROG      | MIDI        | 0 is none                                                                  |
| 104 | VELO      | MIDI        |                                                                            |
| 105 | CC 1      | MIDI        | the values of the four controllers it sends (also sent on, as they change) |
| 106 | CC 2      | MIDI        |                                                                            |
| 107 | CC 3      | MIDI        |                                                                            |
| 108 | CC 4      | MIDI        |                                                                            |
| 109 | CC 1 #    | MIDI        | which controller numbers those are                                         |
| 110 | CC 2 #    | MIDI        |                                                                            |
| 111 | CC 3 #    | MIDI        |                                                                            |
| 112 | CC 4 #    | MIDI        |                                                                            |

Where General MIDI has a convention, the map follows it: 7 volume, 5 portamento time, 72/73/75 release/attack/decay,
91 reverb, 93 chorus, 94 for the delay. The rest use CCs that MIDI leaves undefined.

### Synth engine parameters (CC 20–27)

What CC 20–27 do depends on the track's engine, as the knobs on its SOUND page do:

| Engine   | 20    | 21    | 22   | 23    | 24   | 25    | 26   | 27   |
| -------- | ----- | ----- | ---- | ----- | ---- | ----- | ---- | ---- |
| ANALOG   | WAVE  | DTN   | MIX  | NOIS  | CUT  | RES   | DRV  | KTR  |
| LOFI     | CHIP  | WAVE  | DUTY | CRSH  | SWP  | VIB   | ARP  | TONE |
| TRIO     | WAVE  | INT2  | INT3 | DTN   | MODE | CUT   | RES  | PW   |
| WHEEL    | REG   | SUB   | BODY | TOP   | PERC | CLICK | DRV  | ROTR |
| NOISE    | MODE  | COLR  | FREQ | RES   | TRK  | DENS  | DRFT | CRSH |
| PHASE    | WAVE  | WAVE2 | DCW  | ENV   | DTN  | LINE  | SUB  | FOLD |
| VOICE    | VOWL  | VOWL2 | TALK | SHIFT | BUZZ | BRTH  | Q    | RAND |
| PHYS     | MODEL | STRC  | BRIT | DAMP  | POS  | ACC   | BOW  | EXC  |
| STRING   | SRC   | BRIT  | POS  | BOW   | DAMP | DCAY  | KTR  | DRV  |
| SUPERSAW | VOIC  | DETN  | MIX  | DRFT  | CUT  | RES   | HPF  | CRSH |

## Song parameters (channel 16)

| CC  | Parameter | Notes                                                                      |
| --- | --------- | -------------------------------------------------------------------------- |
| 20  | BPM       | 40–250                                                                     |
| 21  | SWING     |                                                                            |
| 22  | LENGTH    | steps in the pattern, 1–16                                                 |
| 23  | THEME     |                                                                            |
| 24  | TIME      | delay time, 1–16 sixteenths                                                |
| 25  | FDBK      | delay feedback                                                             |
| 26  | SIZE      | reverb size                                                                |
| 27  | DAMP      | reverb darkness                                                            |
| 28  | RATE      | chorus speed                                                               |
| 29  | DEPTH     | chorus depth                                                               |
| 30  | PUMP      | how fast a ducked track comes back                                         |
| 31  | FILTER    | master filter: 0–63 low-pass, 64 off, 65–127 high-pass                     |
| 85  | RESO      | master filter resonance                                                    |
| 86  | DRIVE     | distortion amount                                                          |
| 87  | TYPE      | distortion kind (SOFT HARD TUBE DIODE FUZZ FOLD WAVE RECT BITS RATE CRUSH) |
| 88  | CLOCK     | send MIDI clock                                                            |
| 102 | PLAY      | 64 and above plays, below stops                                            |
| 103 | PATTERN   | patterns 1–8 spread across 0–127 (16 apart)                                |
| 104 | TRACK     | selects tracks 1–9 (about 14 apart)                                        |

## CC OUT: keeping a controller's lights in step

Turn on `CC OUT` on the GLOBAL page's MIDI page (press `SEL` on GLOBAL). Then:

- When you change a track parameter on the FM-1, its CC is sent on the track's channel and on channel 10.
- When you change a song parameter, its CC is sent on channel 16.
- When you select another track, or change a track's machine, all of the selected track's values are sent on channel 10. A controller with LED rings, or one that takes values in, then shows the right values and doesn't jump.

What comes in as a CC is not sent back. CC OUT is off by default. It goes out over the same USB MIDI as MIDI tracks'
notes, so a synth listening on channels 1–10 or 16 also receives those CCs.

## Plugging a USB controller straight in: a USB MIDI host box

The FM-1's USB port only connects to a computer: the FM-1 is a USB _device_, and so is a USB controller, and two
devices can't talk to each other. A small box that is a USB _host_ fixes that. Plug a USB hub into it, and the FM-1
and your controllers into the hub. Everything any of them sends goes to all of the others, so a controller plays and
edits the FM-1, the FM-1's CC OUT reaches the controller's lights, and a USB synth on the hub can play the FM-1's MIDI
tracks. There's nothing to set up: no routing, no commands.

Two ways to build one, both from ready-made parts and with no soldering:

|                      | A: Feather (recommended)                             | B: Raspberry Pi                                                                                         |
| -------------------- | ---------------------------------------------------- | ------------------------------------------------------------------------------------------------------- |
| Board                | Adafruit Feather RP2040 with USB Type A Host (#5723) | Raspberry Pi Zero 2 W (or any Raspberry Pi with USB-A)                                                  |
| Also needed          | a USB hub; a USB-C charger or power bank             | a USB OTG adapter (micro-USB to USB-A), a powered USB hub, a 5 V 2.5 A micro-USB supply, a microSD card |
| Setup                | drag one file onto it                                | install Raspberry Pi OS, run one command                                                                |
| Ready after power-on | under a second                                       | about 30 seconds                                                                                        |
| Devices              | up to 4 on the hub                                   | as many as the hub has ports                                                                            |

Either way you also need the cables you already have: one for the FM-1 (USB-A to USB-C) and one for each controller.

### A: Adafruit Feather RP2040 with USB Type A Host

The board has a USB-A socket for devices and powers it (up to 500 mA). Its USB-C socket is only for power and for
loading the firmware.

1. Get `fomajam-usbhost.uf2`: from the [latest release](https://github.com/konsumer/fomajam/releases/latest), from
   [the site](https://konsumer.js.org/fomajam/fomajam-usbhost.uf2), or build it with `tools/usbhost/build.sh` (needs
   Docker).
2. Hold the board's **BOOT** button while you plug its USB-C into a computer. A drive called `RPI-RP2` appears.
3. Drag `fomajam-usbhost.uf2` onto it. The drive disappears; the box is ready. (You only do this once.)
4. Plug the hub into the USB-A socket, and the FM-1 and your controllers into the hub. Power the board's USB-C from a
   charger or power bank (or a computer).

The red LED blinks when MIDI passes through. If a device keeps dropping out, it needs more power than the board gives:
use a hub with its own power supply. The code is in `tools/usbhost/` (`main.c`, about 60 lines).

### B: Raspberry Pi

1. Use Raspberry Pi Imager to put **Raspberry Pi OS Lite** on the microSD card. In its settings, set a user name and
   password, your Wi-Fi, and turn on SSH.
2. Start the Pi, connect to it with `ssh`, and run:

   ```sh
   curl -fsSL https://raw.githubusercontent.com/konsumer/fomajam/main/tools/usbhost/pi-setup.sh | sudo sh
   ```

3. Plug the OTG adapter into the Zero's **USB** port (not PWR), the powered hub into the adapter, and the FM-1 and
   your controllers into the hub. Power the Pi from its PWR port.

From then on it starts by itself whenever the Pi is powered. It checks every 2 seconds for devices that came or went
(`tools/usbhost/pi-setup.sh` is the whole thing). Wi-Fi is only needed for setup.

**Avoid loops:** if a controller sends back everything it receives ("MIDI thru" or "echo" in its settings), turn
that off, or messages go round in circles.
