// SPDX-License-Identifier: GPL-3.0-only
#include "../src/roll_timing.h"

#include <cstdio>
#include <vector>

namespace {
int failures = 0;

void check(bool ok, const char *what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

struct Hit {
  uint32_t frame;
  uint8_t accent;
  bool gate;
};

struct Render {
  RollTiming::Roll roll;
  uint32_t frame = 0;
  std::vector<Hit> hits;

  void tick(bool held, uint16_t speed, uint32_t gateUs = 0, bool gate = false) {
    roll.update(held, speed, gateUs);
    if (gate) hits.push_back({frame, roll.gate(), true});
    uint8_t accent = 0;
    if (roll.tick(accent)) hits.push_back({frame, accent, false});
    frame += DRUM_BLOCK;
  }
};

void checkClock() {
  RollTiming::GateClock clock;
  check(clock.periodUs(0) == 0, "no gate means standalone roll");
  // The microsecond timer wraps every 71 minutes.
  const uint32_t start = 0xffff0000u;
  clock.pulse(start);
  check(clock.periodUs(start) == 0, "one gate does not establish a tempo");
  clock.pulse(start + 500000u);
  check(clock.periodUs(start + 500100u) == 500000, "tempo survives microsecond wrap");
  clock.pulse(start + 833333u);
  check(clock.periodUs(start + 833400u) == 333333, "next interval follows a tempo change");
  check(clock.periodUs(start + 1500000u) == 0, "missing two gates releases the clock");
  clock.pulse(start + 4000000u);
  check(clock.periodUs(start + 4000000u) == 0, "first gate after a stop reacquires cleanly");
  clock.pulse(start + 4250000u);
  check(clock.periodUs(start + 4250000u) == 250000, "second gate reacquires tempo");
}

void checkSteadyClock(uint16_t speed, bool noisyPot = false) {
  // 200 beats at 120 BPM. 22,050 frames is deliberately not divisible by the
  // 32-sample render quantum; repeating rounded timers must not accumulate drift.
  constexpr uint32_t GateFrames = DRUM_RATE / 2;
  constexpr uint32_t Gates = 200;
  const uint32_t count = RollTiming::divisions(speed, 500000);
  Render render;
  uint32_t nextGate = 0;
  std::vector<uint32_t> anchors;
  while (render.frame < Gates * GateFrames) {
    const bool gate = render.frame >= nextGate && anchors.size() < Gates;
    if (gate) {
      anchors.push_back(render.frame);
      nextGate += GateFrames;
    }
    const uint16_t pot = noisyPot ? uint16_t(speed + int(render.frame / DRUM_BLOCK % 7) - 3)
                                 : speed;
    render.tick(true, pot, 500000, gate);
  }
  check(render.hits.size() == Gates * count, "exactly one gate plus its subdivisions per beat");
  bool aligned = render.hits.size() == Gates * count;
  bool accents = aligned;
  if (aligned) {
    for (uint32_t beat = 0; beat < Gates; ++beat) {
      for (uint32_t step = 0; step < count; ++step) {
        const Hit &hit = render.hits[beat * count + step];
        const uint32_t due = anchors[beat] + (GateFrames * step + count / 2) / count;
        aligned = aligned && hit.frame >= due && hit.frame - due < DRUM_BLOCK &&
                  hit.gate == (step == 0);
        accents = accents && hit.accent == PanelLogic::rollAccent(step);
      }
    }
  }
  check(aligned, "clocked roll stays on the grid over 200 beats, with no boundary doubles");
  check(accents, "each gate resets the roll accents");
}

void checkEntryAndChanges() {
  Render render;
  // Learn gates while ERROR is up, then press between two subdivision points.
  while (render.frame < 26000) {
    const bool gate = render.frame == 0 ||
                      (render.frame >= 22050 && render.frame - DRUM_BLOCK < 22050);
    render.tick(false, 2048, 500000, gate);
  }
  const size_t before = render.hits.size();
  const uint32_t pressed = render.frame;
  render.tick(true, 2048, 500000);
  check(render.hits.size() == before, "pressing ERROR between beats does not start off-grid");
  while (render.frame < 30000) render.tick(true, 2048, 500000);
  check(render.hits.size() == before + 1 && render.hits.back().frame > pressed &&
            render.hits.back().frame >= 22080 + 5513 &&
            render.hits.back().frame < 22080 + 5513 + DRUM_BLOCK,
        "ERROR joins the next subdivision of the already running gate");

  // Change to x16 midway through a beat: join its new grid rather than
  // restarting a separate timer at the knob movement.
  const size_t changed = render.hits.size();
  while (render.frame < 43000) render.tick(true, 4095, 500000);
  bool aligned = render.hits.size() > changed;
  for (size_t i = changed; i < render.hits.size(); ++i) {
    const uint32_t elapsed = render.hits[i].frame - 22080;
    const uint32_t step = (elapsed * 16 + 11025) / 22050;
    const uint32_t due = (22050 * step + 8) / 16;
    aligned = aligned && elapsed >= due && elapsed - due < DRUM_BLOCK;
  }
  check(aligned, "SPEED changes keep the new subdivisions anchored to the gate");

  const size_t released = render.hits.size();
  for (int i = 0; i < 100; ++i) render.tick(false, 4095, 500000);
  check(render.hits.size() == released, "releasing ERROR stops generated repeats");
  render.tick(true, 4095, 500000, true);
  check(render.hits.size() == released + 1 && render.hits.back().gate,
        "a gate and ERROR press together produce one hit");

  // New tempo: one 250 ms interval, x4 = 16 Hz. The first real gate is
  // rendered once, and the subsequent repeats use its new interval.
  const uint32_t anchor = render.frame;
  const size_t tempoStart = render.hits.size();
  render.tick(true, 2048, 250000, true);
  while (render.frame < anchor + 11025) render.tick(true, 2048, 250000);
  check(render.hits.size() == tempoStart + 4, "tempo change produces the new gate's four hits");

  // Losing the clock restores the standalone rate, without waiting for a
  // stale clock deadline to expire in the audio thread.
  const size_t stopped = render.hits.size();
  render.tick(true, 2048);
  check(render.hits.size() == stopped + 1, "clock loss falls back to a standalone roll");
}

void checkStandaloneAndRateLimit() {
  Render render;
  constexpr uint16_t Speed = 1234;
  const uint32_t period = PanelLogic::rollPeriod(Speed);
  while (render.frame < DRUM_RATE * 20u) render.tick(true, Speed);
  bool aligned = !render.hits.empty() && render.hits.front().frame == 0;
  for (size_t i = 0; i < render.hits.size(); ++i) {
    const uint32_t due = uint32_t(i) * period;
    aligned = aligned && render.hits[i].frame >= due &&
              render.hits[i].frame - due < DRUM_BLOCK;
  }
  check(aligned, "standalone roll keeps its immediate start and stable 4-32 Hz timing");
  check(RollTiming::divisions(4095, 125000) == 4 &&
            RollTiming::divisions(4095, 250000) == 8 &&
            RollTiming::divisions(4095, 500000) == 16 &&
            RollTiming::divisions(4095, 10000) == 1,
        "clocked repeats are capped at 32 Hz without discarding incoming gates");
}
} // namespace

int main() {
  checkClock();
  for (uint16_t speed : {0, 1024, 2048, 3072, 4095}) checkSteadyClock(speed);
  checkSteadyClock(2048, true);
  checkEntryAndChanges();
  checkStandaloneAndRateLimit();
  std::printf(failures ? "%d roll timing checks failed\n" : "roll timing checks passed\n", failures);
  return failures ? 1 : 0;
}
