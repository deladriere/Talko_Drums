// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <stdint.h>

enum class Fx : uint8_t { Off, Reverb, Delay, Crush, RingMod, Phaser, Tape, Count };

struct FxLabel {
  const char *name;
  uint8_t glyph;
};

constexpr FxLabel FxLabels[] = {
    {"Off", 0x40},      // -
    {"Reverb", 0x50},   // r
    {"Delay", 0x5e},    // d
    {"Crush", 0x58},    // c
    {"Ring mod", 0x5c}, // o
    {"Phaser", 0x73},   // P
    {"Tape", 0x78},     // t
};
static_assert(sizeof(FxLabels) / sizeof(FxLabels[0]) == unsigned(Fx::Count));

constexpr uint16_t FxAdcMax = 4095;
constexpr uint16_t FxAdcFloor = 48;
constexpr uint32_t FxSampleRate = 44100;

uint16_t fxAmount(uint16_t adc); // Q15 wet amount
void setupEffects();
void setEffect(Fx mode, uint16_t amount);
int16_t processEffect(int32_t sample);
