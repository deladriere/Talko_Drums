// SPDX-License-Identifier: GPL-3.0-only
// On-device cost of each drum voice: the worst and mean microseconds per 128-sample block, one voice alone.
#include "../drum_engine.h"
#include "../effects.h"
#include "../hortator_engine.h"

#include <Arduino.h>

namespace {

constexpr uint32_t Block = 128;
constexpr uint32_t MaxBlocks = 44100 * 7 / Block;

void report(const char *engine, const char *name, uint32_t worst, uint32_t total, uint32_t blocks) {
  Serial.printf("%s %-9s worst_us=%4lu mean_us=%4lu blocks=%lu\n", engine, name, (unsigned long)worst,
                (unsigned long)(blocks ? total / blocks : 0), (unsigned long)blocks);
}

// Every lane of each kit struck in the same block, no budget: the kit's worst case.
void benchKits() {
  int32_t out[Block];
  for (uint32_t bank = 0; bank < DRUM_BANKS; ++bank) {
    drums_init();
    drums_budget(100000);
    for (uint32_t lane = 0; lane < DRUM_LANES; ++lane) {
      drums_hit(bank, lane, 0);
    }
    uint32_t worst = 0, total = 0, blocks = 0;
    while (drums_live() && blocks < MaxBlocks) {
      const uint32_t start = time_us_32();
      drums_render(out, Block);
      const uint32_t took = time_us_32() - start;
      worst = max(worst, took);
      total += took;
      ++blocks;
    }
    report("KIT", drums_bank_name(bank), worst, total, blocks);
  }
}

void benchFelucca() {
  int32_t out[Block];
  for (uint32_t bank = 0; bank < DRUM_BANKS; ++bank) {
    if (drums_bank_engine(bank) != 1) continue;
    for (uint32_t lane = 0; lane < DRUM_LANES; ++lane) {
      drums_init();
      drums_hit(bank, lane, 0);
      uint32_t worst = 0, total = 0, blocks = 0;
      while (drums_live() && blocks < MaxBlocks) {
        const uint32_t start = time_us_32();
        drums_render(out, Block);
        const uint32_t took = time_us_32() - start;
        worst = max(worst, took);
        total += took;
        ++blocks;
      }
      char name[16];
      snprintf(name, sizeof name, "%lu.%s", (unsigned long)bank, drums_lane_name(bank, lane));
      report("FEL", name, worst, total, blocks);
    }
  }
}

void benchHortator() {
  int32_t out[Block];
  for (uint32_t preset = 0; preset < 200; ++preset) {
    const char *name = hort_preset_name(preset);
    if (!*name) break;
    if (hort_preset(name) != preset) continue;
    hort_init();
    const uint32_t started = time_us_32();
    hort_start(0, preset, 0, 64, 96);
    const uint32_t triggerUs = time_us_32() - started;
    Serial.printf("TRIG %s %s trigger_us=%lu\n", hort_model_name(preset), name,
                  (unsigned long)triggerUs);
    uint32_t worst = 0, total = 0, blocks = 0;
    uint32_t live = 1;
    while (live && blocks < MaxBlocks) {
      for (int32_t &s : out) s = 0;
      const uint32_t start = time_us_32();
      live = hort_render(0, out, Block);
      hort_block();
      const uint32_t took = time_us_32() - start;
      worst = max(worst, took);
      total += took;
      ++blocks;
    }
    report(hort_model_name(preset), name, worst, total, blocks);
  }
}

// Each effect alone on a loud noise input, at half and full FX.
void benchEffects() {
  uint32_t noise = 1;
  for (unsigned mode = 0; mode < unsigned(Fx::Count); ++mode) {
    for (const uint16_t adc : {uint16_t(2048), FxAdcMax}) {
      setupEffects();
      setEffect(Fx(mode), fxAmount(adc));
      uint32_t worst = 0, total = 0, blocks = 0;
      for (; blocks < 44100 * 3 / Block; ++blocks) {
        const uint32_t start = time_us_32();
        for (uint32_t i = 0; i < Block; ++i) {
          noise = noise * 1664525u + 1013904223u;
          processEffect(int32_t(noise) >> 17);
        }
        const uint32_t took = time_us_32() - start;
        worst = max(worst, took);
        total += took;
      }
      char name[16];
      snprintf(name, sizeof name, "%s@%u", FxLabels[mode].name, unsigned(adc));
      report("FX", name, worst, total, blocks);
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  delay(500);
}

void loop() {
  if (Serial.read() != '?') return;
  Serial.printf("BENCH f_cpu=%lu budget_us=%lu\n", (unsigned long)F_CPU,
                (unsigned long)(Block * 1000000ull / 44100));
  benchEffects();
  benchKits();
  benchFelucca();
  benchHortator();
  Serial.println("BENCH done");
}
