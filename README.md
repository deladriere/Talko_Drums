# Talko 2 Drums 0.1

A drum machine voice for the Talko 2 RP2040. The encoder selects one of ten
drum kits. SOUND selects one of the kit's eight hits, and SPEAK or GATE plays it.
The hits are Felucca's DRUM voices by Hügelton Instruments, used unchanged at their
designed 44.1 kHz rate (see [THIRD_PARTY.md](THIRD_PARTY.md)).

At startup, the front display shows `0`, the dot, then `1`. The rear LED
flashes blue once for the dot and green once for the minor version; the zero
major version has no red flashes.

## Play

Start with bank **0**, **PITCH** and **SPEED** at noon, and **FX** fully
counterclockwise. Turn **SOUND** and press **SPEAK** at each position.

| Control | Action |
|---|---|
| Encoder | Turn to choose a kit, 0 to 9. Press to switch between the kit and FX pages. |
| SOUND | Choose one of the kit's eight hits, left to right (table below). |
| SPEAK button or GATE input | Play the selected hit. Hits overlap: each rings on while the next one plays. |
| PITCH | An octave down at the left, the designed pitch at noon, an octave up at the right. |
| SPEED | Decay: short at the left, the designed decay at noon, long at the right. |
| FX | Turn fully left for dry sound, then clockwise to bring in the selected effect. |
| ERROR | Hold for a drum roll on the selected hit (below). |

PITCH and SPEED snap to the designed sound in a small zone around noon. They
also reshape hits that are still ringing.

The encoder stops at bank 0 and bank 9. Turn back to select another bank.

| Switch position | SPEAK and GATE behavior |
|---|---|
| Left | One-shot. Every hit rings out its full decay. |
| Middle | Gate. The hit rings while the button or gate is held and fades on release. |
| Right | Choke. Each hit cuts the ones still ringing, as on a single-voice drum box. |

In every position a closed hat cuts a ringing open hat.

## Kits

| Bank | Kit | SOUND, left to right |
|---|---|---|
| `0` | STD | KICK, SNARE, CLAP, HATCL, HATOP, TOM, RIM, BELL |
| `1` | 80 | KICK, SNARE, CLAP, HATCL, HATOP, TOM, RIM, BELL |
| `2` | 10 | KICK, SNARE, CLAP, HATCL, HATOP, TOM, RIM, CYM |
| `3` | 66 | KICK, SNARE, CLAP, HATCL, HATOP, CONGA, RIM, BELL |
| `4` | 55 | KICK, SNARE, CLAP, HATCL, HATOP, TOM, RIM, BELL |
| `5` | 77 | KICK, SNARE, CLAP, HATCL, HATOP, TOM, CLAVE, CYM |
| `6` | DEEP | KICK, SNARE, CLAP, HATCL, HATOP, TOM, RIM, CYM |
| `7` | TIGHT | KICK, SNARE, CLAP, HATCL, HATOP, TOM, RIM, BELL |
| `8` | PERC | KICK, SNARE, CLAP, HATCL, HATOP, CONGA, CLAVE, BELL |
| `9` | H+CYM | KICK, SNARE, CLAP, HATCL, HATOP, CONGA, CLAVE, CYM |

The kit names are Felucca's. Kit 0 is Felucca's own STD kit, and kits 1 to 8
are its model kits: each voices every hit with that kit's own numbers. Kit 9
plays Felucca's own conga, claves and cymbal on the last three positions, with
its ROUND kick.

- **80**: a deep sine kick with a long decay, a two-shell snare, a 1 kHz clap.
- **10**: a small kit, its kick swept down from 90 Hz, three slow claps with a long tail.
- **66**: an early rhythm box, a kick that swells in, a bright noisy snare.
- **55**: a kick dropping from 100 Hz, hats of a high metal cluster.
- **77**: a long swelling kick, six close claps, pulse-cluster hats.
- **DEEP**, **TIGHT**, **PERC**: kits assembled from the pieces of the five above.

## FX page

Press the encoder. The dot stays on in FX mode and goes off on the kit page.
Turn to choose an effect, then use the FX knob to adjust it. Press again to
return to the kits. The effect stays active across kits. The module starts in
bank 0 with Reverb selected.

| Glyph | Effect | FX knob |
|---|---|---|
| `-` | Off | Dry. |
| `r` | Reverb | Brings up a room and lengthens its decay. |
| `d` | Delay | Raises the level and feedback of 300 ms repeats that darken as they fade. |
| `c` | Crush | Fades in over the first eighth, then lowers the bit depth from 12 to 4 bits and the sample rate down to 2.8 kHz. |
| `o` | Ring mod | Increases modulation depth and sweeps the carrier from 30 to 1,200 Hz. |
| `P` | Phaser | Increases the depth of a four-stage sweep. |
| `t` | Tape | Fades in at half speed over the first quarter of travel. Playback reaches normal speed near noon and double speed at the right, with tape wobble throughout. |

Every effect is dry in the small zone at the FX knob's counterclockwise end.
Turn fully left to switch effects while a tail rings, then turn clockwise to
bring in the new effect. Reverb, Delay and Tape keep running silently at zero,
so turning back up brings in fresh audio rather than an old tail.

## ERROR roll

Hold ERROR: the selected hit plays as a roll, starting at once. Every fourth
hit is accented. Release ERROR to stop; the last hits ring out.

| Control while ERROR is held | Effect on the roll |
|---|---|
| SPEED | Roll rate, from 4 hits a second at the left to 32 at the right. |
| SOUND | Which hit rolls; turn it to move the roll across the kit. |
| PITCH | Tunes the rolling hit live. |
| FX | Effect level, as usual. |
| Encoder | Changes the kit, or the effect on the FX page. |

The decay stays where SPEED had it when ERROR was pressed. The display uses
Talko 2's stock ERROR flicker on the current kit number or FX glyph while
ERROR is held.

## Sleeping drum

After five minutes without activity, the display dims and shows a slow beat:
a kick on the bottom segment and two hats on the top. Turn a knob or the
encoder, press a button, move the mode switch, or send a gate to bring back the
current kit or FX page. A ringing hit or held ERROR keeps the display awake.

## USB serial

The port appears as **Talko 2 Drums 0.1**. At 115200 baud it prints the name of
every hit as it plays, one per line: `KICK`, `SNARE`, `CLAP`, `HATCL`, `HATOP`,
`TOM`, `CONGA`, `RIM`, `CLAVE`, `BELL` or `CYM`. When hits come faster than the
USB link can print, only the latest is printed. Send `?` for a status report:

```text
Talko 2 Drums 0.1
BANK 1 80
SOUND 3 CLAP
EFFECT Reverb page=BANK
PANEL sound=1180 pitch=2050 fx=0 speed=2047 gate=0 error=0
MODE gate
ENCODER detents=4 invalid=0
AUDIO budget_us=2902 peak_us=... late_blocks=0 underrun_samples=0
```

Drum hits have no articulation to animate, so this firmware does not stream
Console lip-sync events.

## Building

PlatformIO, from this folder:

```sh
pio run                      # firmware: .pio/build/pico/firmware.uf2
sh tests/run_host.sh         # host checks with ASan and UBSan
sh tests/run_host.sh renders # also writes every hit and a groove per kit as WAV
```

The audio runs at 44.1 kHz with a 88.2 kHz PWM carrier on GP1. Core 1 renders
the hits and effects in 128-sample blocks; core 0 handles the panel and USB.

## Hardware checks still to do

Version 0.1 is built and checked on the host only. On a Talko, check:

- The first encoder click after boot, fast turns both ways, and a one-click
  reversal at banks 0 and 9.
- The FX dot with SPEAK and GATE active.
- The heaviest load: bank 2 or 6 (long cymbal and open hat), all eight hits
  struck in quick succession with ERROR rolling at full SPEED and Reverb at full,
  while moving the pots. Send `?` and compare `peak_us` with `budget_us`
  (2,902 µs); `late_blocks` and `underrun_samples` should stay 0, with USB open
  and closed.
- The serial port name after reconnecting USB.
