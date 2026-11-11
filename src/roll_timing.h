// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "panel_logic.h"

namespace RollTiming {

// Measure accepted gate edges, before the CV settling delay. A stopped clock
// expires so an isolated SPEAK press is not mistaken for a running sequencer.
class GateClock {
 public:
  void pulse(uint32_t atUs) {
    if (seen_) {
      const uint32_t interval = atUs - lastUs_;
      periodUs_ = interval >= 10000 && interval <= 4000000 ? interval : 0;
    }
    lastUs_ = atUs;
    seen_ = true;
  }

  uint32_t periodUs(uint32_t nowUs) {
    const uint32_t timeout = periodUs_ ? periodUs_ * 2 : 4000000;
    if (seen_ && nowUs - lastUs_ > timeout) {
      periodUs_ = 0;
      seen_ = false;
    }
    return periodUs_;
  }

 private:
  bool seen_ = false;
  uint32_t lastUs_ = 0;
  uint32_t periodUs_ = 0;
};

// Powers of two per gate, with generated repeats limited to the existing
// 32-hit/second ceiling. Fast incoming gates themselves are never discarded.
inline uint8_t divisions(uint16_t speed, uint32_t gateUs) {
  uint32_t index = uint32_t(speed) * 5 / (PanelLogic::AdcMax + 1);
  if (index > 4) index = 4;
  uint8_t count = 1u << index;
  while (count > 1 && uint64_t(gateUs) * 32 < uint64_t(count) * 1000000) {
    count /= 2;
  }
  return count;
}

// The audio thread advances this on its actual 32-sample render grid. Gates
// anchor both the repeats and accents; no predicted hit is emitted at the
// next gate boundary, where it could double the real gate.
class Roll {
 public:
  bool held() const { return held_; }

  void update(bool held, uint16_t speed, uint32_t gateUs) {
    const bool starting = held && !held_;
    const bool changed = speed != speed_ || gateUs != gateUs_;
    const bool wasSynced = synced_;
    bool gridChanged = false;
    if (changed || starting) {
      freePeriod_ = PanelLogic::rollPeriod(speed);
      const uint32_t frames = uint32_t((uint64_t(gateUs) * DRUM_RATE + 500000) / 1000000);
      const uint8_t count = gateUs ? divisions(speed, gateUs) : 1;
      gridChanged = frames != gateFrames_ || count != divisions_;
      gateFrames_ = frames;
      divisions_ = count;
    }
    speed_ = speed;
    gateUs_ = gateUs;
    held_ = held;
    synced_ = gateUs != 0 && gateSeen_;
    if (!held_) return;

    if (synced_ && (starting || gridChanged || !wasSynced)) {
      // Join the current gate's subdivision grid, even when ERROR is pressed
      // between beats or SPEED changes while a roll is already sounding.
      const uint32_t elapsed = frame_ - anchor_;
      nextStep_ = elapsed >= gateFrames_
                      ? divisions_
                      : (elapsed * divisions_ + gateFrames_ - 1) / gateFrames_;
    } else if (!synced_ && (starting || wasSynced)) {
      countdown_ = 0;
      hits_ = 0;
    }
  }

  // Called exactly once for a real gate/SPEAK hit, which the caller renders.
  // The following tick must not produce a second hit on the same boundary.
  uint8_t gate() {
    gateSeen_ = true;
    anchor_ = frame_;
    nextStep_ = 1;
    hits_ = 1;
    countdown_ = int32_t(freePeriod_);
    synced_ = gateUs_ != 0;
    return held_ ? PanelLogic::rollAccent(0) : 0;
  }

  bool tick(uint8_t &accent) {
    bool play = false;
    if (held_) {
      if (synced_) {
        if (nextStep_ < divisions_) {
          const uint32_t due = (gateFrames_ * nextStep_ + divisions_ / 2) / divisions_;
          if (frame_ - anchor_ >= due) {
            accent = PanelLogic::rollAccent(nextStep_++);
            play = true;
          }
        }
      } else {
        if (countdown_ <= 0) {
          accent = PanelLogic::rollAccent(hits_++);
          countdown_ += int32_t(freePeriod_);
          play = true;
        }
        countdown_ -= DRUM_BLOCK;
      }
    }
    frame_ += DRUM_BLOCK;
    return play;
  }

 private:
  bool held_ = false;
  bool synced_ = false;
  bool gateSeen_ = false;
  uint16_t speed_ = 0;
  uint8_t divisions_ = 1;
  uint32_t gateUs_ = 0;
  uint32_t frame_ = 0;
  uint32_t anchor_ = 0;
  uint32_t gateFrames_ = 0;
  uint32_t nextStep_ = 0;
  uint32_t hits_ = 0;
  uint32_t freePeriod_ = DRUM_RATE / 4;
  int32_t countdown_ = 0;
};

} // namespace RollTiming
