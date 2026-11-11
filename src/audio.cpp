// SPDX-License-Identifier: GPL-3.0-only
#include "audio.h"
#include "drum_engine.h"
#include "panel_logic.h"
#include "roll_timing.h"

#include <Arduino.h>
#include <atomic>
#include <hardware/clocks.h>
#include <hardware/pwm.h>
#include <pico/critical_section.h>

namespace {

constexpr uint AudioPin = 1;
constexpr uint SampleRate = FxSampleRate;
constexpr uint CarrierCycles = 2;
constexpr uint BlockFrames = 128;
constexpr uint BlockBudgetUs = (BlockFrames * 1000000u + SampleRate / 2) / SampleRate;
constexpr uint RingFrames = BlockFrames * 2;
constexpr uint RingMask = RingFrames - 1;
constexpr uint32_t GateReleaseUs = 30000;
constexpr uint32_t ChokeUs = 4000;
// validation/hardware-0.1-serial.txt: a block with Reverb and no voice took 740 us at 133 MHz.
constexpr uint32_t FxReserveUs133 = 740;
constexpr uint32_t ShedUs = BlockBudgetUs * 85 / 100;
static_assert((RingFrames & RingMask) == 0);
static_assert(SampleRate == DRUM_RATE && BlockFrames % DRUM_BLOCK == 0);

critical_section_t controlsLock;
PanelControls latestControls{};
std::atomic<bool> ready{false};
std::atomic<uint32_t> readIndex{0};
std::atomic<uint32_t> writeIndex{0};
std::atomic<uint32_t> peakUs{0};
std::atomic<uint32_t> lateBlocks{0};
std::atomic<uint32_t> underrunSamples{0};
std::atomic<uint32_t> sheds{0};
uint32_t voiceBudget = 0;
std::atomic<uint32_t> hitCount{0};
std::atomic<uint32_t> hitState{0};
uint16_t duties[RingFrames];

uint audioSlice;
uint audioChannel;
uint pwmPeriod;
uint carrierPhase = 0;
bool outputStarted = false;

uint __not_in_flash_func(nextDuty)() {
  const uint32_t read = readIndex.load(std::memory_order_relaxed);
  const uint32_t available = writeIndex.load(std::memory_order_acquire) - read;
  if (!outputStarted) {
    outputStarted = available == RingFrames;
    if (!outputStarted) {
      return pwmPeriod / 2;
    }
  }

  if (available == 0) {
    underrunSamples.store(underrunSamples.load(std::memory_order_relaxed) + 1,
                          std::memory_order_relaxed);
    return pwmPeriod / 2;
  }
  const uint duty = duties[read & RingMask];
  readIndex.store(read + 1, std::memory_order_release);
  return duty;
}

void __isr __not_in_flash_func(audioInterrupt)() {
  pwm_clear_irq(audioSlice);
  if (++carrierPhase == CarrierCycles) {
    carrierPhase = 0;
    pwm_set_chan_level(audioSlice, audioChannel, nextDuty());
  }
}

PanelControls readControls() {
  critical_section_enter_blocking(&controlsLock);
  PanelControls controls = latestControls;
  critical_section_exit(&controlsLock);
  return controls;
}

uint32_t lastHit = 0;

void publishHit(bool audible) {
  hitState.store(lastHit | (audible ? 0x10000u : 0), std::memory_order_relaxed);
}

void hit(const PanelControls &panel, uint8_t accent) {
  if (panel.mode == PlayMode::Choke) {
    drums_choke_all(ChokeUs);
  }
  drums_hit(panel.bank, panel.lane, accent);
  lastHit = (uint32_t(panel.bank) << 8) | panel.lane;
  publishHit(true);
  hitCount.fetch_add(1, std::memory_order_release);
}

struct Roll {
  uint8_t decay = DRUM_DESIGN;
  RollTiming::Roll timing;
};
Roll roll;

void applyControls(const PanelControls &next) {
  static PanelControls previous{};
  static bool firstBlock = true;

  uint8_t decay = PanelLogic::speedDecay(next.speed);
  if (next.error && !roll.timing.held()) {
    // Keep the decay SPEED had before it became the roll rate.
    roll.decay = decay;
  }
  roll.timing.update(next.error, next.speed, next.rollGateUs);
  if (roll.timing.held()) {
    decay = roll.decay;
  }
  drums_shape(PanelLogic::pitchTune(next.pitch), decay);

  if (firstBlock || next.effect != previous.effect || next.fx != previous.fx) {
    setEffect(next.effect, fxAmount(next.fx));
  }
  if (!firstBlock && next.trigger != previous.trigger) {
    hit(next, roll.timing.gate());
  }
  if (!firstBlock && previous.gate && !next.gate && next.mode == PlayMode::Gate &&
      !roll.timing.held()) {
    drums_release(GateReleaseUs);
  }
  previous = next;
  firstBlock = false;
}

void renderBlock(const PanelControls &panel, int32_t *mix) {
  for (uint offset = 0; offset < BlockFrames; offset += DRUM_BLOCK) {
    uint8_t accent = 0;
    if (roll.timing.tick(accent)) {
      hit(panel, accent);
    }
    drums_render(mix + offset, DRUM_BLOCK);
  }
}

} // namespace

void setupAudio(const PanelControls &controls) {
  critical_section_init(&controlsLock);
  latestControls = controls;
  audioSlice = pwm_gpio_to_slice_num(AudioPin);
  audioChannel = pwm_gpio_to_channel(AudioPin);
  constexpr uint carrierRate = SampleRate * CarrierCycles;
  pwmPeriod = (clock_get_hz(clk_sys) + carrierRate / 2) / carrierRate;

  pwm_config config = pwm_get_default_config();
  pwm_config_set_wrap(&config, pwmPeriod - 1);
  pwm_init(audioSlice, &config, false);
  pwm_set_chan_level(audioSlice, audioChannel, pwmPeriod / 2);
  gpio_set_function(AudioPin, GPIO_FUNC_PWM);
  pwm_clear_irq(audioSlice);
  irq_set_exclusive_handler(PWM_IRQ_WRAP, audioInterrupt);
  // On the panel core, let PWM updates preempt USB serial work.
  irq_set_priority(PWM_IRQ_WRAP, PICO_HIGHEST_IRQ_PRIORITY);
  pwm_set_irq_enabled(audioSlice, true);
  irq_set_enabled(PWM_IRQ_WRAP, true);
  pwm_set_enabled(audioSlice, true);

  ready.store(true, std::memory_order_release);
}

void updateAudioControls(const PanelControls &controls) {
  critical_section_enter_blocking(&controlsLock);
  latestControls = controls;
  critical_section_exit(&controlsLock);
}

AudioStats audioStats() {
  return {BlockBudgetUs, peakUs.load(std::memory_order_relaxed),
          lateBlocks.load(std::memory_order_relaxed),
          underrunSamples.load(std::memory_order_relaxed),
          sheds.load(std::memory_order_relaxed), voiceBudget};
}

void audioResetPeak() { peakUs.store(0, std::memory_order_relaxed); }

AudioHit audioHit() {
  const uint32_t count = hitCount.load(std::memory_order_acquire);
  const uint32_t state = hitState.load(std::memory_order_relaxed);
  return {count, uint8_t(state >> 8), uint8_t(state), bool(state & 0x10000u)};
}

bool core1_separate_stack = true;

void setup1() {
  while (!ready.load(std::memory_order_acquire)) {
    tight_loop_contents();
  }
  drums_init();
  // Voices get 90% of a block less the effects' share, both scaled to the clock.
  const uint32_t mhz = clock_get_hz(clk_sys) / 1000000;
  // Plan the voices to finish below the shed threshold, so shedding stays a last resort.
  const uint32_t usable = ShedUs - FxReserveUs133 * 133 / mhz;
  voiceBudget = usable * mhz / 133;
  drums_budget(voiceBudget);
  setupEffects();
}

void loop1() {
  const uint32_t write = writeIndex.load(std::memory_order_relaxed);
  const uint32_t read = readIndex.load(std::memory_order_acquire);
  if (write - read > RingFrames - BlockFrames) {
    tight_loop_contents();
    return;
  }

  const uint32_t start = micros();
  const PanelControls panel = readControls();
  applyControls(panel);
  static int32_t mix[BlockFrames];
  renderBlock(panel, mix);
  for (uint i = 0; i < BlockFrames; ++i) {
    const int32_t processed = processEffect(mix[i]);
    duties[(write + i) & RingMask] = ((processed + 32768) * pwmPeriod + 32768u) >> 16;
  }
  publishHit(drums_live() != 0);
  writeIndex.store(write + BlockFrames, std::memory_order_release);

  const uint32_t elapsed = micros() - start;
  if (elapsed > peakUs.load(std::memory_order_relaxed)) {
    peakUs.store(elapsed, std::memory_order_relaxed);
  }
  if (elapsed > BlockBudgetUs) {
    lateBlocks.store(lateBlocks.load(std::memory_order_relaxed) + 1,
                     std::memory_order_relaxed);
  }
  if (elapsed > ShedUs && drums_live()) {
    drums_shed();
    sheds.store(sheds.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
  }
}
