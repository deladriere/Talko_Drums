// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  DRUM_RATE = 44100,
  DRUM_BLOCK = 32,
  DRUM_BANKS = 16,
  DRUM_LANES = 8,
  DRUM_DESIGN = 64, // DECAY at the voice's designed sound
  DRUM_TUNE_RANGE = 192, // 1/16 semitone: one octave each way
  DRUM_COST_FLOOR = 2000, // the smallest voice budget drums_budget accepts
};

void drums_init(void);
// Microseconds per 128 samples the voices may take, in the units of hort_cost_us (133 MHz).
void drums_budget(uint32_t us133);
// PITCH (1/16 semitone) and DECAY (0..127). New hits, and the ringing hits of Felucca's kits.
void drums_shape(int32_t tune16, uint32_t decay);
void drums_hit(uint32_t bank, uint32_t lane, uint32_t accent);
// Fade the most recent hit, or every ringing hit, with time constant tau.
void drums_release(uint32_t tau_us);
void drums_choke_all(uint32_t tau_us);
// Stop the oldest ringing hit at once (declicked): the audio block ran late.
void drums_shed(void);
// n is a multiple of DRUM_BLOCK. Mono Q15, never above full scale.
void drums_render(int32_t *out, uint32_t n);
uint32_t drums_live(void);
const char *drums_bank_name(uint32_t bank);
const char *drums_lane_name(uint32_t bank, uint32_t lane);
// 1 for Felucca's kits, 2 for Hortator's.
uint32_t drums_bank_engine(uint32_t bank);

#ifdef __cplusplus
}
#endif
