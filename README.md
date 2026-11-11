# Talko 2 Drums 0.3

A drum machine voice for the Talko 2 RP2040. The encoder selects one of sixteen
drum kits, `0` to `F`. SOUND selects one of the kit's eight hits, and SPEAK or
GATE plays it. Kits `0` to `5` are Felucca's DRUM voices by Hügelton Instruments;
kits `6` to `F` are built from the drum models and factory presets of Hortator
by DEADACTIVE. Both are used unchanged at their designed 44.1 kHz rate (see
[THIRD_PARTY.md](THIRD_PARTY.md)).

At startup, the front display shows `0`, the dot, then `3`. The rear LED
flashes blue once for the dot and green three times for the minor version; the zero
major version has no red flashes.

## User manual and downloads

For installation, panel controls, kits and playing examples, see the
[Talko Drums User Manual](https://www.polaxis.be/talko-drums-user-manual/).
Ready-to-install UF2 files are on [GitHub Releases](https://github.com/deladriere/Talko_Drums/releases).
This README covers development, implementation and verification.

For a future version, publish a GitHub Release at the matching source commit
and attach one `Talko_Drums-<version>.uf2` plus its checksum. The Polaxis browser
installer discovers published releases automatically; draft releases and
prereleases are excluded.

## Building

Requirements: PlatformIO Core with the Arduino-Pico core selected in
`platformio.ini`. PlatformIO installs the embedded toolchain and framework.
Host checks also need a POSIX shell and C11/C++17 compilers (`cc` and `c++`)
with AddressSanitizer and UndefinedBehaviorSanitizer support.

The project has one build/upload environment: `pico`.

Run these commands from this folder:

```sh
pio run -e pico              # firmware: .pio/build/pico/firmware.uf2
sh tests/run_host.sh         # host checks with ASan and UBSan
sh tests/run_host.sh renders # also writes every hit, a groove per kit, and every Hortator preset as WAV
```

To flash a connected Talko and read it, with PlatformIO's Python (it has
pyserial). Close any Console page holding the port first:

```sh
~/.platformio/penv/bin/python tools/flash_local.py --serial <USB serial number> --image Talko_Drums-0.3.uf2 --product "Talko 2 Drums 0.3"
~/.platformio/penv/bin/python tools/status.py --seconds 30
~/.platformio/penv/bin/python tools/status.py --seconds 150 --every 0 --send '!' --until "TEST done"
```

`flash_local.py` first saves the complete flash to `rollback/` unless given
`--no-backup`.

The RP2040 runs at 200 MHz with the core regulator set to 1.15 V by the
Arduino core. Raspberry Pi documents this operating point in the
[RP2040 datasheet, section 2.15.3](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf);
133 MHz is the limit specified across all process, voltage and temperature
variations. The audio runs at 44.1 kHz with an
88.2 kHz PWM carrier on GP1. Core 1 renders the hits and effects in 128-sample
blocks; core 0 handles the panel and USB. Both drum engines run from RAM
(`tools/engines_in_ram.py`), which shortened the worst whole-kit blocks by 10 to
20% over running them from flash.

### Voice costs

`validation/bench-200.txt` holds, from the bench firmware at 200 MHz with the
engines in RAM: each effect's cost, each kit with all eight hits struck at once,
each voice's worst 128-sample block alone, and each Hortator trigger.
`src/hortator_engine.c` budgets each Hortator model by its worst block, and
`src/drum_engine.c` adds 60 (in 133 MHz microseconds) per Hortator voice for the
cost of mixing it, measured with the self-test. Hortator's Plaits-style models
(BOOM, PUNCH, SNAP, CRACK, METAL, HISS) take 0.8 to 2 ms each and SKIN 2.1 ms,
so no kit uses them. Reverb is the heaviest effect, 334 µs per block.

### Source layout

- `src/main.cpp`: panel inputs, display, USB serial and on-device self-test.
- `src/audio.cpp`: core 1 rendering, PWM output, timing counters and voice budget.
- `src/drum_engine.c` and `src/hortator_engine.c`: voice management and engine adapters.
- `src/effects.cpp`: effects; `src/panel_logic.h`: control mappings and gate settling.
- `src/roll_timing.h`: gate-clock tracking and the ERROR roll's subdivision grid.
- `third_party/`: bundled upstream engines, licences and source hashes.
- `tests/`, `tools/` and `validation/`: host checks, development helpers and hardware logs.

## Play

Start with bank **0**, **PITCH** and **SPEED** at noon, and **FX** fully
counterclockwise. Turn **SOUND** and press **SPEAK** at each position.

| Control | Action |
|---|---|
| Encoder | Turn to choose a kit, `0` to `F`. Press to switch between the kit and FX pages. |
| SOUND | Choose one of the kit's eight hits, left to right (table below). |
| SPEAK button or GATE input | Play the selected hit. Hits overlap: each rings on while the next one plays. |
| PITCH | An octave down at the left, the designed pitch at noon, an octave up at the right. |
| SPEED | Decay: short at the left, the designed decay at noon, long at the right. |
| FX | Turn fully left for dry sound, then clockwise to bring in the selected effect. |
| ERROR | Hold for a drum roll on the selected hit (below). |

A hit from SPEAK or GATE waits until SOUND, PITCH, SPEED and FX hold still,
5 ms at most after the gate edge, and then reads them directly. A sequencer
that moves its SOUND CV with each gate therefore plays each step's own hit.
Set each step's CV inside its hit's zone: the gate reads SOUND without the
knob's small hysteresis.

PITCH and SPEED snap to the designed sound in a small zone around noon. On
kits `0` to `5` they also reshape hits that are still ringing; kits `6` to `F`
take them when the hit starts, and PITCH moves those in semitone steps.

The encoder stops at bank `0` and bank `F`. Turn back to select another bank.

| Switch position | SPEAK and GATE behavior |
|---|---|
| Left | One-shot. Every hit rings out its full decay. |
| Middle | Gate. The hit rings while the button or gate is held and fades on release. |
| Right | Choke. Each hit cuts the ones still ringing, as on a single-voice drum box. |

In every position a closed hat cuts its kit's ringing open hat (not in HAND and
FM, which have no hat pair).

## Kits

| Bank | Kit | SOUND, left to right |
|---|---|---|
| `0` | STD | KICK, SNARE, CLAP, HATCL, HATOP, TOM, RIM, BELL |
| `1` | 80 | KICK, SNARE, CLAP, HATCL, HATOP, TOM, RIM, BELL |
| `2` | 10 | KICK, SNARE, CLAP, HATCL, HATOP, TOM, RIM, CYM |
| `3` | 66 | KICK, SNARE, CLAP, HATCL, HATOP, CONGA, RIM, BELL |
| `4` | 55 | KICK, SNARE, CLAP, HATCL, HATOP, TOM, RIM, BELL |
| `5` | 77 | KICK, SNARE, CLAP, HATCL, HATOP, TOM, CLAVE, CYM |
| `6` | 808 | ROUNDBD, WARMSN, HANDCLAP, CLOSEDHH, OPENHAT, MIDTOM, RIMSHOT, COWBELL |
| `7` | 909 | HOUSEBD, CRISPSN, SNAPCLAP, LOOSEHAT, WASHHAT, HITOM, HOTRIM, CRASH |
| `8` | TRX | SUBTHUMP, HARDSNAP, ROOMCLAP, NOISEHAT, AIROPEN, SWOOPTOM, KNOCK, WASHCYM |
| `9` | EFM | FMSUB, TONESNR, BUZZCLAP, FMHAT, TRASHHAT, LONGTOM, FMRIM, FMCRASH |
| `A` | DIRT | BIGBOOM, POPSNARE, WIDECLAP, METALHAT, DARKOPEN, FUZZTOM, GRITRIM, THINCRSH |
| `b` | ZAP | NOISEZAP, RIMSNARE, FMZAP, SPITHAT, TREMHAT, DROPTOM, DEEPBLIP, GONG |
| `C` | HAND | TUMBA, CONGA, SLAPCONG, BONGO, CLAVE, WOODBLK, LOWBELL, CLINK |
| `d` | DEEP | SUBBOOM, TONALSN, DARKCLAP, DARKHAT, DUSTYHAT, FLOORTOM, WOODRIM, DARKCRSH |
| `E` | TIGHT | TIGHTBD, TIGHTSN, DRYCLAP, TICK, HALFHAT, PIP, STICK, SPLASH |
| `F` | FM | RISEKICK, FMNOISE, RATTLE, FMPERC, SIZZRIDE, FMTOM, BLIP, FMBELL |

Kits `0` to `5` use Felucca's kit names: its own STD kit and its five model kits.

- **80**: a deep sine kick with a long decay, a two-shell snare, a 1 kHz clap.
- **10**: a small kit, its kick swept down from 90 Hz, three slow claps with a long tail.
- **66**: an early rhythm box, a kick that swells in, a bright noisy snare.
- **55**: a kick dropping from 100 Hz, hats of a high metal cluster.
- **77**: a long swelling kick, six close claps, pulse-cluster hats.

Kits `6` to `F` use Hortator's preset names. Each kit draws on one family of
Hortator's models:

- **808**, **909**, **DEEP**, **TIGHT**: its analog-style classic models, at their
  standard, dark and long, and short settings.
- **TRX**, **DIRT**: its measured digital Machinedrum-style models.
- **EFM**, **ZAP**: its measured FM models; ZAP takes the short, glitchy presets.
- **HAND**: congas, bongo, claves, woodblock and bells.
- **FM**: its two-operator FM percussion, with an FM kick, clap and ride.

Each Hortator sound is level-matched to the Felucca kits' hit at the same SOUND
position.

### Voices and CPU

Eight hits can ring at once. Kits `0` to `7` and `C` to `E` play all eight.
TRX, EFM, DIRT, ZAP and FM cost more CPU per voice: when a new hit would not
fit the audio budget, the oldest ringing hits stop (with a short declick) to
make room. With every hit struck at once, three or four of a TRX, EFM, DIRT or
ZAP kit's hits ring together, and seven of FM's. If a block
still runs long, the oldest hit stops at once; the status report counts these as
`sheds`.

## FX page

Press the encoder. The dot stays on in FX mode and goes off on the kit page.
Turn to choose an effect, then use the FX knob to adjust it. Press again to
return to the kits. The effect stays active across kits. The module starts in
bank `0` with Reverb selected. On the FX page, the dot tells the Delay `d` from
kit `d`.

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

Hold ERROR for a roll on the selected hit. Without a gate clock it starts at
once, with every fourth hit accented. Release ERROR to stop; the last hits
ring out.

With regular incoming gates, the roll follows their rhythm after two pulses.
SPEED selects 1, 2, 4, 8 or 16 hits per gate interval, capped at 32 hits a second
(fast incoming gates themselves are still played). Each gate restarts the
subdivisions and accents. Pressing ERROR between beats joins the next
subdivision; moving SPEED keeps the roll on the gate's grid. The roll waits
for each new gate instead of doubling it with a predicted hit. After two
gate intervals without another pulse, it returns to the standalone roll.

| Control while ERROR is held | Effect on the roll |
|---|---|
| SPEED | Clocked: 1, 2, 4, 8 or 16 hits per gate, subject to the rate limit. Standalone: 4 to 32 hits a second. |
| SOUND | Which hit rolls; turn it to move the roll across the kit. |
| PITCH | Tunes the rolling hit live. |
| FX | Effect level, as usual. |
| Encoder | Changes the kit, or the effect on the FX page. |

The decay stays where SPEED had it when ERROR was pressed. The display uses
Talko 2's stock ERROR flicker on the current kit or FX glyph while ERROR is
held.

## Sleeping drum

After five minutes without activity, the display dims and shows a slow beat:
a kick on the bottom segment and two hats on the top. Turn a knob or the
encoder, press a button, move the mode switch, or send a gate to bring back the
current kit or FX page. A ringing hit or held ERROR keeps the display awake.

## USB serial

Serial-port selectors that support device names show **Talko 2 Drums 0.3**,
so you can identify the installed firmware and version before connecting.

At 115200 baud, the firmware prints the name of every hit as it plays, one per
line: Felucca's `KICK`, `SNARE`, `CLAP`, `HATCL`,
`HATOP`, `TOM`, `CONGA`, `RIM`, `CLAVE`, `BELL` or `CYM` on kits `0` to `5`, and
the Hortator preset name (`SUBTHUMP`, `FMHAT` and so on) on kits `6` to `F`. When
hits come faster than the USB link can print, only the latest is printed. Send
`?` for a status report:

```text
Talko 2 Drums 0.3
BANK 8 TRX
SOUND 3 ROOMCLAP
EFFECT Reverb page=BANK
PANEL sound=1180 pitch=2050 fx=0 speed=2047 gate=0 error=0
MODE gate
GATE settle_us=2000
ROLL gate_us=0
ENCODER detents=4 invalid=0
AUDIO budget_us=2902 peak_us=... late_blocks=0 underrun_samples=0 sheds=0
CPU mhz=200 voices=2 voice_budget_us133=2968
```

`GATE settle_us` is how long the last gate's hit waited for its CV to settle.
`ROLL gate_us` is the measured interval between incoming gates, before CV
settling; zero means the roll uses its standalone rate.

Send `!` for the self-test, about two minutes, which needs no hands. It takes
over the panel and plays every kit in turn: each hit twice, then a 32-hit-a-second
ERROR roll on the last hit dry, then with Reverb up while SOUND sweeps across the
kit. Each kit reports the worst block of the three phases and its sheds, then
`TEST done`, and the panel is restored:

```text
TEST 9 EFM peak_us=2180/1446/2302 sheds=0/0/0 late_blocks=0 underrun_samples=0
```

Drum hits have no articulation to animate, so this firmware does not stream
Console lip-sync events.

## Hardware checks

### 0.3

On 11 November 2026, 0.3 was flashed and sound was confirmed on the Talko.
A serial status snapshot during playback reported a peak block time of
982 us out of 2,902 us, with no late blocks, underruns or sheds.

Gate-synchronized ERROR rolls are not yet checked on hardware. The host timing
checks simulate 200 beats at each subdivision, off-beat ERROR entry, SPEED
movement, tempo changes, clock loss and timer wrap. Keep `Talko_Drums-0.2.uf2`
for rollback.

On hardware, check a sequencer with ERROR held, different SPEED settings,
tempo changes, and pressing ERROR between beats. Listen for drift or doubled
hits at gate boundaries, including with the heavy kits and Reverb active.

### 0.2

On 11 November 2026, 0.2 was flashed with `tools/flash_local.py`:

- USB showed `Talko 2 Drums 0.2` after each flash; the status report gave
  `CPU mhz=200`.
- Played by hand for two minutes (`validation/hardware-0.2-serial.txt`): the
  encoder reached the new kits, whose kicks played (kits 6, 9, A, b, C, d, E, F).
  34 detents, 0 invalid; `peak_us` at most 1,037, with no late blocks,
  underruns or sheds. A Chrome Console page reconnected to the port during that
  log and took part of the serial stream, so some report lines are cut.
- Self-test (`!`, `validation/hardware-0.2-selftest.txt`), all 16 kits:
  `late_blocks` and `underrun_samples` 0 throughout. Worst blocks 0.9 to
  1.3 ms in kits 0 to 7, d and E; 1.8 ms in HAND; 2.1 to 2.5 ms in TRX, EFM,
  DIRT, ZAP and FM, out of 2,902 µs. One shed, in FM, during the roll with
  Reverb that sweeps all eight hits.

Still to check:

- A sequencer driving GATE and SOUND CV together: each step's own hit
  should play (`GATE settle_us` shows the wait). The earlier build played the
  previous step's hit.
- The first encoder click after boot, fast turns both ways, and a one-click
  reversal at banks `0` and `F`.
- The FX dot with SPEAK and GATE active; the ERROR roll from the panel.
- The serial port name after reconnecting USB, and the audio with USB closed.

### 0.1

On 11 November 2026, 0.1 was flashed with `tools/flash_local.py` and played
for 90 seconds while `tools/status.py` logged the serial output
(`validation/hardware-0.1-serial.txt`):

- USB showed `Talko 2 Drums 0.1`. The status report answered every `?`.
- 139 hit labels came through, covering all eight SOUND positions, with
  banks 0, 1, 8 and 9 seen in the reports. Encoder: 23 detents, 0 invalid. The
  FX page switched between Reverb, Crush and Ring mod.
- Audio: `peak_us` 740 at rest with Reverb, 2,142 at the busiest moment, out of a
  2,902 µs budget. `late_blocks` and `underrun_samples` stayed 0.
