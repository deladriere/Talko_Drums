# Release notes

## 0.2 - 11 November 2026

Sixteen kits, `0` to `F`, from two drum engines.

- Kits `6` to `F` are new: 808, 909, TRX, EFM, DIRT, ZAP, HAND, DEEP, TIGHT
  and FM, eighty sounds from Hortator's drum models and factory presets,
  level-matched to Felucca's kits. Kits `0` to `5` are Felucca's STD, 80, 10,
  66, 55 and 77; 0.1's DEEP, TIGHT, PERC and H+CYM, which reused those kits'
  sounds, are gone.
- Hits from GATE or SPEAK wait up to 5 ms for the CV inputs to settle and read
  them directly. A sequencer moving SOUND with each gate now plays each step's
  hit; 0.1 played the previous step's.
- The RP2040 runs at 200 MHz, and both engines run from RAM. Each new hit
  first stops the oldest ones if the audio budget would be exceeded; a block
  that still runs long stops the oldest hit at once (`sheds` in the status).
- Status: the bank as the display shows it, `GATE settle_us`, `sheds`, and a
  `CPU` line. `!` runs an unattended self-test of every kit.
- USB serial name `Talko 2 Drums 0.2`.

Validation: host checks of all 128 sounds, the voice budget, the gate settling
and the effects passed with ASan and UBSan. On the Talko, the self-test of all
16 kits ran with no late blocks or underruns; worst block 2.5 ms of 2.9 ms. The
sequencer fix is not yet checked with a sequencer. The firmware builds to a
226,720-byte image using 166,416 bytes of RAM.

## 0.1 - 11 November 2026

First Talko 2 Drums build for Talko 2 (RP2040). Not yet tested on hardware.

- Ten kits of eight hits from Felucca's DRUM voices: its STD kit, its eight
  model kits and an H+CYM kit, at Felucca's 44.1 kHz design rate.
- Encoder selects the kit, SOUND the hit, PITCH tunes ±1 octave and SPEED sets
  the decay, both snapping to the design at noon.
- Up to eight hits ring together; a closed hat chokes the open hat.
- Mode switch: one-shot, gate (fades on release) and choke.
- Held ERROR rolls the selected hit at 4 to 32 hits a second, with the stock
  ERROR display flicker.
- Seven effects, all dry with FX fully counterclockwise: Reverb, Delay,
  Crush, Ring mod, Phaser and Tape.
- USB serial name `Talko 2 Drums 0.1`, one hit label per line, `?` status.

Validation: host checks of all 80 hits (level, ring-out, knob extremes), voice
handling, panel mappings and effects passed with ASan and UBSan. The firmware
builds to a 118 KB image using 66 KB of RAM.
