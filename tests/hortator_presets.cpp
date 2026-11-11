// SPDX-License-Identifier: GPL-3.0-only
// Every Hortator preset this firmware can play: level, length and host render time, optional WAVs.
#include "../src/hortator_engine.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

void writeWav(const std::string &path, const std::vector<int16_t> &samples) {
  FILE *file = std::fopen(path.c_str(), "wb");
  if (!file) return;
  const uint32_t bytes = uint32_t(samples.size() * 2);
  auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, file); };
  auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, file); };
  std::fwrite("RIFF", 1, 4, file); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, file);
  u32(16); u16(1); u16(1); u32(44100); u32(88200); u16(2); u16(16);
  std::fwrite("data", 1, 4, file); u32(bytes);
  std::fwrite(samples.data(), 2, samples.size(), file);
  std::fclose(file);
}

}  // namespace

int main(int argc, char **argv) {
  const char *wavDir = argc > 1 ? argv[1] : nullptr;
  hort_init();
  int failures = 0;
  std::printf("%-9s %-6s %6s %7s %8s\n", "PRESET", "MODEL", "PEAK", "LEN_MS", "NS/SMP");
  for (uint32_t preset = 0; preset < 200; ++preset) {
    const char *name = hort_preset_name(preset);
    if (!*name) break;
    if (hort_preset(name) != preset) continue;  // SAMPLE and ACID presets are not built
    hort_start(0, preset, 0, 64, 96);
    std::vector<int16_t> wav;
    int32_t block[128];
    int32_t peak = 0;
    uint32_t last = 0, n = 0;
    double seconds = 0;
    while (n < 44100u * 7u) {
      for (int32_t &s : block) s = 0;
      const auto start = std::chrono::steady_clock::now();
      const uint32_t live = hort_render(0, block, 128);
      seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
      hort_block();
      for (int32_t s : block) {
        const int32_t a = std::abs(s);
        peak = a > peak ? a : peak;
        if (a > 32) last = n;
        wav.push_back(int16_t(s > 32767 ? 32767 : s < -32768 ? -32768 : s));
        ++n;
      }
      if (!live) break;
    }
    // Raw levels: the kits' gains even them out (tests/drums.cpp checks the result).
    const bool ok = peak > 500 && peak < 65536 && n < 44100u * 7u;
    failures += !ok;
    std::printf("%-9s %-6s %6d %7u %8.1f%s\n", name, hort_model_name(preset), peak, last * 1000 / 44100,
                seconds * 1e9 / n, ok ? "" : "  <-- check");
    if (wavDir) writeWav(std::string(wavDir) + "/" + name + ".wav", wav);
  }
  std::printf(failures ? "%d presets to check\n" : "all presets play\n", failures);
  return failures ? 1 : 0;
}
