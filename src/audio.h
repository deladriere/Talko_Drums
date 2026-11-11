// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "effects.h"

#include <stdint.h>

enum class PlayMode : uint8_t { OneShot, Gate, Choke };

struct PanelControls {
  uint16_t pitch;
  uint16_t fx;
  uint16_t speed;
  Fx effect = Fx::Reverb;
  PlayMode mode = PlayMode::Gate;
  uint8_t bank;
  uint8_t lane;
  uint8_t trigger; // counts SPEAK and GATE hits
  bool gate;
  bool error;
};

struct AudioStats {
  uint32_t budgetUs;
  uint32_t peakUs;
  uint32_t lateBlocks;
  uint32_t underrunSamples;
};

struct AudioHit {
  uint32_t count; // every hit played, the ERROR roll's too
  uint8_t bank;
  uint8_t lane;
  bool audible;
};

void setupAudio(const PanelControls &controls);
void updateAudioControls(const PanelControls &controls);
AudioStats audioStats();
AudioHit audioHit();
