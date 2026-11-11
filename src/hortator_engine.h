// SPDX-License-Identifier: GPL-3.0-only
// Hortator's drum models (third_party/hortator, unmodified), one model voice per slot.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { HORT_SLOTS = 8, HORT_NO_PRESET = 0xFFFF };

void hort_init(void);
// Index of a Hortator factory preset by its name, HORT_NO_PRESET if none.
uint32_t hort_preset(const char *name);
const char *hort_preset_name(uint32_t preset);
const char *hort_model_name(uint32_t preset);
// The preset's model's worst block of 128 samples on the device at 133 MHz, in microseconds.
uint32_t hort_cost_us(uint32_t preset);
// Starts preset on slot, transposed by semitones, decay 0..127 (64 = the preset's), velocity 1..127.
void hort_start(uint32_t slot, uint32_t preset, int32_t semitones, uint32_t decay, uint32_t velocity);
void hort_stop(uint32_t slot);
// Adds n samples (a multiple of 32) of slot's voice to out; returns 0 once the voice has ended.
uint32_t hort_render(uint32_t slot, int32_t *out, uint32_t n);
void hort_block(void);
// Host builds with DM_QCHECK: Q24 results that did not fit 32 bits (Hortator's tests require 0).
uint32_t hort_q_overflows(void);

#ifdef __cplusplus
}
#endif
