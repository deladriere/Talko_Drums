// SPDX-License-Identifier: GPL-3.0-only
// Host checks of the drum engine, the panel mappings and the effects.
// With an output folder, also writes each hit and a short groove per bank as WAV.
#include "../src/drum_engine.h"
#include "../src/effects.h"
#include "../src/hortator_engine.h"
#include "../src/panel_logic.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int failures = 0;
// drums_budget on the device at 200 MHz (audio.cpp setup1).
constexpr uint32_t DeviceBudget = 2968;

void check(bool ok, const std::string &what) {
  if (!ok) {
    std::printf("FAIL %s\n", what.c_str());
    ++failures;
  }
}

void writeWav(const std::string &path, const std::vector<int16_t> &samples) {
  FILE *file = std::fopen(path.c_str(), "wb");
  if (!file) {
    check(false, "open " + path);
    return;
  }
  auto word = [&](uint32_t v, int bytes) { std::fwrite(&v, bytes, 1, file); };
  const uint32_t data = uint32_t(samples.size() * 2);
  std::fwrite("RIFF", 1, 4, file);
  word(36 + data, 4);
  std::fwrite("WAVEfmt ", 1, 8, file);
  word(16, 4);
  word(1, 2);
  word(1, 2);
  word(DRUM_RATE, 4);
  word(DRUM_RATE * 2, 4);
  word(2, 2);
  word(16, 2);
  std::fwrite("data", 1, 4, file);
  word(data, 4);
  std::fwrite(samples.data(), 2, samples.size(), file);
  std::fclose(file);
}

struct Hit {
  int32_t peak = 0;
  double rms = 0;
  double seconds = 0;
  std::vector<int16_t> samples;
};

Hit renderHit(uint32_t bank, uint32_t lane, int32_t tune, uint32_t decay, double maxSeconds) {
  drums_init();
  drums_budget(DeviceBudget);
  drums_shape(tune, decay);
  drums_hit(bank, lane, 0);
  Hit hit;
  int32_t block[DRUM_BLOCK];
  double energy = 0;
  const size_t limit = size_t(maxSeconds * DRUM_RATE);
  while (hit.samples.size() < limit && drums_live()) {
    drums_render(block, DRUM_BLOCK);
    for (int32_t s : block) {
      hit.peak = std::max(hit.peak, std::abs(s));
      energy += double(s) * s;
      hit.samples.push_back(int16_t(std::clamp(s, -32768, 32767)));
    }
  }
  hit.seconds = double(hit.samples.size()) / DRUM_RATE;
  hit.rms = std::sqrt(energy / std::max<size_t>(1, hit.samples.size()));
  return hit;
}

void checkHits(const char *folder) {
  std::printf("%-6s %-9s %7s %7s %7s\n", "bank", "sound", "peak", "rms", "sec");
  for (uint32_t bank = 0; bank < DRUM_BANKS; ++bank) {
    for (uint32_t lane = 0; lane < DRUM_LANES; ++lane) {
      const std::string name = std::to_string(bank) + " " + drums_bank_name(bank) + " " +
                               std::to_string(lane + 1) + " " + drums_lane_name(bank, lane);
      check(*drums_lane_name(bank, lane) != 0, name + " has a name");
      const Hit hit = renderHit(bank, lane, 0, DRUM_DESIGN, 20.0);
      std::printf("%-6s %-9s %7d %7.0f %7.2f\n", drums_bank_name(bank),
                  drums_lane_name(bank, lane), hit.peak, hit.rms, hit.seconds);
      check(hit.peak > 2000, name + " is audible");
      check(hit.peak <= 32767, name + " stays under full scale");
      check(hit.seconds < 20.0, name + " rings out");
      if (folder) {
        char file[128];
        std::snprintf(file, sizeof(file), "%s/bank%u-%u-%s.wav", folder, unsigned(bank),
                      unsigned(lane + 1), drums_lane_name(bank, lane));
        writeWav(file, hit.samples);
      }
      // The knobs' extremes must not overflow or go silent.
      for (int32_t tune : {-int32_t(DRUM_TUNE_RANGE), int32_t(DRUM_TUNE_RANGE)}) {
        for (uint32_t decay : {0u, 127u}) {
          const Hit edge = renderHit(bank, lane, tune, decay, 30.0);
          check(edge.peak > 500 && edge.peak <= 32767,
                name + " at tune " + std::to_string(tune) + " decay " + std::to_string(decay));
        }
      }
    }
  }
}

void checkVoices() {
  int32_t block[DRUM_BLOCK];
  for (uint32_t bank : {0u, 6u}) {
    const std::string kit = std::string(drums_bank_name(bank)) + ": ";
    // A lane is mono: restriking it keeps one voice.
    drums_init();
    drums_budget(DeviceBudget);
    for (int i = 0; i < 5; ++i) {
      drums_hit(bank, 0, 0);
      drums_render(block, DRUM_BLOCK);
    }
    check(drums_live() == 1, kit + "restruck kick keeps one voice");

    // A closed hat chokes the open one within a few milliseconds.
    drums_init();
    drums_budget(DeviceBudget);
    drums_hit(bank, 4, 0);
    for (int i = 0; i < 10; ++i) drums_render(block, DRUM_BLOCK);
    drums_hit(bank, 3, 0);
    for (int i = 0; i < 100; ++i) drums_render(block, DRUM_BLOCK);
    check(drums_live() <= 1, kit + "closed hat chokes the open hat");

    // Gate release fades the last hit.
    drums_init();
    drums_budget(DeviceBudget);
    drums_hit(bank, 7, 0);
    drums_render(block, DRUM_BLOCK);
    drums_release(30000);
    for (int i = 0; i < 2000 && drums_live(); ++i) drums_render(block, DRUM_BLOCK);
    check(drums_live() == 0, kit + "gate release ends the last hit");
  }

  // Every hit of a bank rings together without a budget, under full scale; choke silences them.
  for (uint32_t bank = 0; bank < DRUM_BANKS; ++bank) {
    const std::string kit = std::string(drums_bank_name(bank)) + ": ";
    drums_init();
    drums_budget(1000000);
    for (uint32_t lane = 0; lane < DRUM_LANES; ++lane) drums_hit(bank, lane, 127);
    check(drums_live() == DRUM_LANES, kit + "a full kit rings together");
    int32_t peak = 0;
    for (int i = 0; i < 40; ++i) {
      drums_render(block, DRUM_BLOCK);
      for (int32_t s : block) peak = std::max(peak, std::abs(s));
    }
    check(peak <= 32767, kit + "a full kit at once stays under full scale");
    drums_choke_all(4000);
    for (int i = 0; i < 60; ++i) drums_render(block, DRUM_BLOCK);
    check(drums_live() == 0, kit + "choke silences every hit");
  }

  // The device budget: Felucca's and the classic kits play all eight, the heavy kits fewer.
  for (uint32_t bank = 0; bank < DRUM_BANKS; ++bank) {
    drums_init();
    drums_budget(DeviceBudget);
    for (uint32_t lane = 0; lane < DRUM_LANES; ++lane) drums_hit(bank, lane, 0);
    const uint32_t live = drums_live();
    std::printf("%s: %u of %d hits ring in the device budget\n", drums_bank_name(bank),
                unsigned(live), DRUM_LANES);
    const bool heavy = (bank >= 8 && bank <= 11) || bank == 15;
    check(heavy ? live >= 3 && live < DRUM_LANES : live >= DRUM_LANES - 1,
          std::string(drums_bank_name(bank)) + ": " + std::to_string(live) + " voices in the budget");
  }

  // A late block sheds the oldest hit at once.
  drums_init();
  drums_budget(DeviceBudget);
  drums_hit(0, 0, 0);
  drums_hit(0, 1, 0);
  drums_render(block, DRUM_BLOCK);
  drums_shed();
  check(drums_live() == 1, "shed stops one hit");
  drums_shed();
  drums_shed();
  check(drums_live() == 0, "shed with nothing left is harmless");
  for (int i = 0; i < 10; ++i) drums_render(block, DRUM_BLOCK);
  check(std::abs(block[DRUM_BLOCK - 1]) < 64, "a shed hit is declicked to silence");
  check(hort_q_overflows() == 0, "Hortator's Q24 results fit 32 bits");
}

void checkPanel() {
  using namespace PanelLogic;
  uint8_t lane = DRUM_LANES;
  for (int adc = 0; adc <= 4095; ++adc) {
    lane = soundLane(uint16_t(adc), lane);
  }
  check(lane == DRUM_LANES - 1, "SOUND reaches the eighth hit");
  for (int adc = 4095; adc >= 0; --adc) {
    lane = soundLane(uint16_t(adc), lane);
  }
  check(lane == 0, "SOUND returns to the first hit");
  lane = soundLane(500, 0);
  check(lane == 0 && soundLane(530, 0) == 0 && soundLane(600, 0) == 1, "SOUND hysteresis");

  check(pitchTune(2048) == 0 && pitchTune(2000) == 0, "PITCH noon is the design");
  check(pitchTune(0) == -DRUM_TUNE_RANGE && pitchTune(4095) == DRUM_TUNE_RANGE, "PITCH ends");
  check(speedDecay(2048) == DRUM_DESIGN && speedDecay(0) == 0 && speedDecay(4095) == 127,
        "SPEED decay");
  check(rollPeriod(0) == DRUM_RATE / 4 && rollPeriod(4095) == (DRUM_RATE + 16) / 32,
        "roll from 4 to 32 hits a second");
  check(rollAccent(0) == 127 && rollAccent(1) == 0 && rollAccent(4) == 127, "roll accents");

  // A sequencer step: SOUND CV falls from the snare (lane 1) to the kick (lane 0) as the gate
  // rises. The gate debounce passes at 1.5 ms; main.cpp samples every GateSampleUs after it.
  auto settle = [](auto sound, uint32_t &firedUs) {
    GateSettle gate;
    for (uint32_t us = 1500;; us += GateSampleUs) {
      const uint16_t adc[PotCount] = {sound(us), 2048, 0, 2048};
      if (gate.sample(us, adc)) {
        firedUs = us;
        return soundLane(adc[0], DRUM_LANES);
      }
    }
  };
  uint32_t firedUs = 0;
  for (const double tauUs : {0.0, 300.0, 1000.0}) {
    const uint8_t hit = settle(
        [tauUs](uint32_t us) {
          return uint16_t(250 + (750 - 250) * (tauUs > 0 ? std::exp(-double(us) / tauUs) : 0));
        },
        firedUs);
    check(hit == 0 && firedUs <= GateSettleMaxUs,
          "a sequencer step plays its own hit (CV time constant " +
              std::to_string(int(tauUs)) + " us)");
  }
  settle([](uint32_t) { return uint16_t(250); }, firedUs);
  check(firedUs == GateSettleMinUs, "a still SOUND plays after GateSettleMinUs");
  settle([](uint32_t us) { return uint16_t(us / 4 % 4000); }, firedUs);
  check(firedUs >= GateSettleMaxUs && firedUs < GateSettleMaxUs + GateSampleUs,
        "a moving SOUND still plays by GateSettleMaxUs");
  check(soundLane(470, DRUM_LANES) == 0 && soundLane(470, 1) == 1,
        "a gate's hit ignores the knob hysteresis");
}

void checkEffects() {
  setupEffects();
  for (unsigned mode = 0; mode < unsigned(Fx::Count); ++mode) {
    const Fx fx = Fx(mode);
    setEffect(fx, 32768 - 1);
    uint32_t noise = 12345;
    // Build up a tail at full FX, then turn FX fully down.
    for (int i = 0; i < DRUM_RATE / 2; ++i) {
      noise = noise * 1664525u + 1013904223u;
      processEffect(i < 4000 ? int32_t(noise >> 17) - 16384 : 0);
    }
    setEffect(fx, fxAmount(0));
    bool dry = true;
    for (int i = 0; i < 2000; ++i) {
      const int32_t in = (i * 37) % 20000 - 10000;
      dry = dry && processEffect(in) == in;
    }
    check(dry, std::string(FxLabels[mode].name) + " is dry at zero after a tail");
  }
  // Switching effects at zero stays dry.
  for (unsigned mode = 0; mode < unsigned(Fx::Count); ++mode) {
    setEffect(Fx(mode), 0);
    check(processEffect(1234) == 1234, std::string("switch to ") + FxLabels[mode].name);
  }
  check(fxAmount(48) == 0 && fxAmount(4095) == 32768, "FX dead zone and full scale");
}

void writeGrooves(const char *folder) {
  // Two bars at 120 BPM of kick, snare, closed and open hat, clap.
  constexpr int Steps = 32;
  constexpr int StepFrames = DRUM_RATE * 60 / 120 / 4 / DRUM_BLOCK * DRUM_BLOCK;
  const char *pattern[5] = {"x...x...x...x..x", "....x.......x...", "x.x.x.x.x.x.x.x.",
                            "...............x", "............x..."};
  const uint32_t lanes[5] = {0, 1, 3, 4, 2};
  for (uint32_t bank = 0; bank < DRUM_BANKS; ++bank) {
    drums_init();
    drums_budget(DeviceBudget);
    std::vector<int16_t> out;
    int32_t block[DRUM_BLOCK];
    for (int step = 0; step < Steps + 8; ++step) {
      for (int part = 0; step < Steps && part < 5; ++part) {
        if (pattern[part][step % 16] == 'x') drums_hit(bank, lanes[part], step % 4 ? 0 : 90);
      }
      for (int f = 0; f < StepFrames; f += DRUM_BLOCK) {
        drums_render(block, DRUM_BLOCK);
        for (int32_t s : block) out.push_back(int16_t(s));
      }
    }
    char file[128];
    std::snprintf(file, sizeof(file), "%s/groove-bank%u-%s.wav", folder, unsigned(bank),
                  drums_bank_name(bank));
    writeWav(file, out);
  }
}

} // namespace

int main(int argc, char **argv) {
  const char *folder = argc > 1 ? argv[1] : nullptr;
  checkHits(folder);
  checkVoices();
  checkPanel();
  checkEffects();
  if (folder) {
    writeGrooves(folder);
  }
  std::printf(failures ? "%d checks failed\n" : "all checks passed\n", failures);
  return failures ? 1 : 0;
}
