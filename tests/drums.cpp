// SPDX-License-Identifier: GPL-3.0-only
// Host checks of the drum engine, the panel mappings and the effects.
// With an output folder, also writes each hit and a short groove per bank as WAV.
#include "../src/drum_engine.h"
#include "../src/effects.h"
#include "../src/panel_logic.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int failures = 0;

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
  std::printf("%-6s %-6s %7s %7s %7s\n", "bank", "sound", "peak", "rms", "sec");
  for (uint32_t bank = 0; bank < DRUM_BANKS; ++bank) {
    for (uint32_t lane = 0; lane < DRUM_LANES; ++lane) {
      const std::string name = std::to_string(bank) + " " + drums_bank_name(bank) + " " +
                               std::to_string(lane + 1) + " " + drums_lane_name(bank, lane);
      const Hit hit = renderHit(bank, lane, 0, DRUM_DESIGN, 20.0);
      std::printf("%-6s %-6s %7d %7.0f %7.2f\n", drums_bank_name(bank),
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
  // A lane is mono: restriking it keeps one voice.
  drums_init();
  int32_t block[DRUM_BLOCK];
  for (int i = 0; i < 5; ++i) {
    drums_hit(0, 0, 0);
    drums_render(block, DRUM_BLOCK);
  }
  check(drums_live() == 1, "restruck kick keeps one voice");

  // A closed hat chokes the open one within a few milliseconds.
  drums_init();
  drums_hit(0, 4, 0);
  drums_render(block, DRUM_BLOCK);
  drums_hit(0, 3, 0);
  for (int i = 0; i < 100; ++i) drums_render(block, DRUM_BLOCK);
  check(drums_live() == 1, "closed hat chokes the open hat");

  // Every hit of a bank rings together, and choke silences them.
  drums_init();
  for (uint32_t lane = 0; lane < DRUM_LANES; ++lane) drums_hit(1, lane, 127);
  int32_t peak = 0;
  for (int i = 0; i < 40; ++i) {
    drums_render(block, DRUM_BLOCK);
    for (int32_t s : block) peak = std::max(peak, std::abs(s));
  }
  check(peak <= 32767, "a full kit at once stays under full scale");
  drums_choke_all(4000);
  for (int i = 0; i < 60; ++i) drums_render(block, DRUM_BLOCK);
  check(drums_live() == 0, "choke silences every hit");

  // Gate release fades the last hit.
  drums_init();
  drums_hit(0, 7, 0);
  drums_render(block, DRUM_BLOCK);
  drums_release(30000);
  for (int i = 0; i < 2000 && drums_live(); ++i) drums_render(block, DRUM_BLOCK);
  check(drums_live() == 0, "gate release ends the bell");
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
