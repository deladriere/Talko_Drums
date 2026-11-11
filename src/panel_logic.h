// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "drum_engine.h"

#include <math.h>
#include <stdint.h>

namespace PanelLogic {

constexpr int AdcMax = 4095;
constexpr int SoundMax = 4000; // Talko 2's SOUND pot stops short of 4095
constexpr int SoundHysteresis = 60;
constexpr int Detent = 56; // PITCH and SPEED snap to the design around noon
constexpr int Noon = 2048;
constexpr float RollSlowestHz = 4.0f;
constexpr float RollOctaves = 3.0f; // 4 Hz .. 32 Hz

// SOUND picks one of the bank's hits. A neighbouring hit is taken only once
// the knob is well inside its zone, so a resting knob does not flip between two.
inline uint8_t soundLane(uint16_t adc, uint8_t current) {
  const int value = adc > SoundMax ? SoundMax : adc;
  const int zone = (SoundMax + 1 + DRUM_LANES - 1) / DRUM_LANES;
  const int lane = value / zone;
  if (lane == current || current >= DRUM_LANES) {
    return uint8_t(lane);
  }
  const int low = current * zone - SoundHysteresis;
  const int high = (current + 1) * zone + SoundHysteresis;
  return value < low || value >= high ? uint8_t(lane) : current;
}

// Distance from noon past the detent, -1..1.
inline float fromNoon(uint16_t adc) {
  const int offset = int(adc) - Noon;
  if (offset >= -Detent && offset <= Detent) {
    return 0.0f;
  }
  return offset > 0 ? float(offset - Detent) / float(AdcMax - Noon - Detent)
                    : float(offset + Detent) / float(Noon - Detent);
}

// PITCH: an octave down at the left, the designed pitch at noon, an octave up at the right.
inline int32_t pitchTune(uint16_t adc) {
  return int32_t(lroundf(fromNoon(adc) * DRUM_TUNE_RANGE));
}

// SPEED: DECAY 0 at the left, the design (64) at noon, 127 at the right.
inline uint8_t speedDecay(uint16_t adc) {
  const float x = fromNoon(adc);
  return uint8_t(lroundf(DRUM_DESIGN + x * (x < 0.0f ? DRUM_DESIGN : 127 - DRUM_DESIGN)));
}

// While ERROR is held, SPEED sets the roll from 4 hits a second to 32.
inline uint32_t rollPeriod(uint16_t speed) {
  const float hz = RollSlowestHz * exp2f(RollOctaves * float(speed) / float(AdcMax));
  return uint32_t(lroundf(float(DRUM_RATE) / hz));
}

// Every fourth hit of a roll is accented.
inline uint8_t rollAccent(uint32_t hit) {
  return hit % 4 == 0 ? 127 : 0;
}

} // namespace PanelLogic
