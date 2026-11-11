# Release notes

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
