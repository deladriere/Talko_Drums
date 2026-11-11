// SPDX-License-Identifier: GPL-3.0-only
#include "audio.h"
#include "drum_engine.h"
#include "panel_logic.h"
#include "talko_startup.h"

#include <Arduino.h>
#include <cstdio>
#include <hardware/gpio.h>
#include <hardware/pwm.h>

namespace {

constexpr uint ErrorPin = 0;
constexpr uint GatePin = 2;
constexpr uint EncoderA = 3;
constexpr uint EncoderB = 4;
constexpr uint SegmentA = 5;
constexpr uint DecimalPoint = 12;
constexpr uint SelectPin = 13;
constexpr uint ModeB = 14;
constexpr uint ModeA = 15;
constexpr uint8_t VersionMajor = 0;
constexpr uint8_t VersionMinor = 1;
constexpr uint SoundPin = 26;
constexpr uint FxPin = 27;
constexpr uint SpeedPin = 28;
constexpr uint PitchPin = 29;
constexpr uint DisplayBrightness = 100;
constexpr uint32_t IdleMs = 5 * 60 * 1000;
constexpr uint32_t SleepFrameMs = 25;
constexpr uint32_t FlickerFrameMs = 50;
constexpr int PotActivityThreshold = 32;
constexpr uint GateDebounceUs = 1500;
constexpr uint SelectDebounceMs = 10;
constexpr uint EncoderSettleMs = 25;
constexpr uint ModeDebounceMs = 10;

constexpr const char *ModeNames[] = {"one-shot", "gate", "choke"};

PanelControls controls{};
uint8_t lastEncoderState = 0;
int8_t encoderSteps = 0;
volatile uint8_t bankCursor = 0;
volatile uint8_t fxCursor = unsigned(Fx::Reverb);
volatile uint32_t encoderDetents = 0;
volatile uint32_t encoderInvalid = 0;
bool gateRaw = false;
bool gateHeld = false;
uint32_t gateChangedAt = 0;
volatile bool fxPage = false;
PlayMode rawMode = PlayMode::Gate;
uint32_t modeChangedAt = 0;

struct Button {
  uint pin;
  bool raw = false;
  bool held = false;
  uint32_t changedAt = 0;

  bool update(uint32_t now) {
    const bool pressed = !gpio_get(pin);
    if (pressed != raw) {
      raw = pressed;
      changedAt = now;
    }
    if (raw == held || now - changedAt < SelectDebounceMs) {
      return false;
    }
    held = raw;
    return true;
  }
};

Button selectButton{SelectPin};
Button errorButton{ErrorPin};

struct Pot {
  uint pin;
  uint32_t filtered;
  uint16_t lastActivity = 0;

  void begin() {
    lastActivity = analogRead(pin);
    filtered = lastActivity * 4;
  }

  bool update() {
    filtered += analogRead(pin) - int32_t(value());
    const uint16_t current = value();
    if (abs(int(current) - lastActivity) < PotActivityThreshold) {
      return false;
    }
    lastActivity = current;
    return true;
  }

  uint16_t value() const { return filtered / 4; }
};

Pot sound{SoundPin, 0};
Pot pitch{PitchPin, 0};
Pot fxPot{FxPin, 0};
Pot speed{SpeedPin, 0};

uint8_t encoderState() {
  const uint32_t pins = gpio_get_all();
  return (((pins >> EncoderA) & 1u) << 1) | ((pins >> EncoderB) & 1u);
}

void encoderInterrupt() {
  constexpr int8_t transitions[16] = {
       0, -1,  1,  0,
       1,  0,  0, -1,
      -1,  0,  0,  1,
       0,  1, -1,  0,
  };
  const uint8_t state = encoderState();
  const uint8_t previous = lastEncoderState;
  lastEncoderState = state;
  const int8_t step = transitions[(previous << 2) | state];
  if (step == 0) {
    if (state != previous) {
      ++encoderInvalid;
    }
    return;
  }

  encoderSteps += step;
  if (encoderSteps != 4 && encoderSteps != -4) {
    return;
  }

  const int movement = encoderSteps > 0 ? 1 : -1;
  encoderSteps = 0;
  ++encoderDetents;
  // Apply the limit on each click so a reversal at either end responds at once.
  if (fxPage) {
    const int effect = int(fxCursor) + movement;
    constexpr int count = int(Fx::Count);
    fxCursor = (effect + count) % count;
  } else {
    bankCursor = constrain(int(bankCursor) + movement, 0, DRUM_BANKS - 1);
  }
}

bool updateGate() {
  // The gate input is active low on the microcontroller.
  const bool raw = !gpio_get(GatePin);
  const uint32_t now = micros();
  if (raw != gateRaw) {
    gateRaw = raw;
    gateChangedAt = now;
  }
  if (raw == gateHeld || now - gateChangedAt < GateDebounceUs) {
    return false;
  }
  gateHeld = raw;
  return true;
}

PlayMode readMode() {
  constexpr uint32_t mask = (1u << ModeA) | (1u << ModeB);
  const uint32_t pins = gpio_get_all() & mask;
  if (pins == (1u << ModeB)) {
    return PlayMode::OneShot;
  }
  if (pins == (1u << ModeA)) {
    return PlayMode::Choke;
  }
  return PlayMode::Gate;
}

bool updateMode(uint32_t now) {
  const PlayMode next = readMode();
  if (next != rawMode) {
    rawMode = next;
    modeChangedAt = now;
  }
  if (next == controls.mode || now - modeChangedAt < ModeDebounceMs) {
    return false;
  }
  controls.mode = next;
  return true;
}

void readPots() {
  controls.lane = PanelLogic::soundLane(sound.value(), controls.lane);
  controls.pitch = pitch.value();
  controls.fx = fxPot.value();
  controls.speed = speed.value();
}

void drawDisplay(uint8_t segments, uint brightness) {
  for (uint bit = 0; bit < 8; ++bit) {
    pwm_set_gpio_level(SegmentA + bit, (segments & (1u << bit)) ? brightness : 0);
  }
}

void drawStartup(uint8_t segments) {
  drawDisplay(segments, DisplayBrightness);
}

uint8_t selectionGlyph() {
  return fxPage ? FxLabels[unsigned(controls.effect)].glyph | TalkoStartup::Dot
                : TalkoStartup::Digits[controls.bank];
}

// Talko 2's stock ERROR flicker: sharp dips and occasional blackouts.
uint flickerBrightness() {
  constexpr int Base = 80;
  int brightness = random(100) < 30 ? random(20, Base - 10) : Base + random(-10, 20);
  if (random(100) < 5) {
    brightness = 0;
  }
  return uint(brightness);
}

// A sleeping drum: a soft kick on the bottom segment, then two hats on the top.
void showSleepingDrum(uint32_t elapsed) {
  constexpr uint32_t CycleMs = 1600;
  constexpr uint32_t KickMs = 450;
  constexpr uint32_t HatMs = 140;
  constexpr uint32_t Hats[] = {800, 1200};
  constexpr uint Peak = 40;
  const uint32_t phase = elapsed % CycleMs;
  const uint kick = phase < KickMs ? Peak * (KickMs - phase) / KickMs : 0;
  uint hat = 0;
  for (uint32_t at : Hats) {
    if (phase >= at && phase < at + HatMs) {
      hat = Peak / 2 * (at + HatMs - phase) / HatMs;
    }
  }
  for (uint bit = 0; bit < 8; ++bit) {
    pwm_set_gpio_level(SegmentA + bit, bit == 3 ? kick : bit == 0 ? hat : 0);
  }
}

void updateDisplay(uint32_t now, bool activity, bool selectionChanged) {
  static uint32_t lastActivity = now;
  static uint32_t lastFrame = 0;
  static bool sleeping = false;
  static bool flickering = false;
  if (activity) {
    lastActivity = now;
  }

  const uint32_t idle = now - lastActivity;
  if (idle >= IdleMs) {
    const uint32_t frame = (idle - IdleMs) / SleepFrameMs;
    if (!sleeping || frame != lastFrame) {
      showSleepingDrum(frame * SleepFrameMs);
      lastFrame = frame;
    }
    sleeping = true;
    flickering = false;
    return;
  }
  if (controls.error) {
    const uint32_t frame = now / FlickerFrameMs;
    if (!flickering || sleeping || selectionChanged || frame != lastFrame) {
      drawDisplay(selectionGlyph(), flickerBrightness());
      lastFrame = frame;
    }
    flickering = true;
    sleeping = false;
    return;
  }
  if (sleeping || flickering || selectionChanged) {
    drawDisplay(selectionGlyph(), DisplayBrightness);
  }
  sleeping = flickering = false;
}

void serviceConsole() {
  static bool status = false;
  static uint32_t reportedHits = 0;
  static char buffer[512];
  static size_t length = 0;
  static size_t sent = 0;
  if (!Serial) {
    status = false;
    length = sent = 0;
    reportedHits = audioHit().count;
    return;
  }

  for (unsigned i = 0; i < 16 && Serial.available(); ++i) {
    if (Serial.read() == '?') status = true;
  }
  const AudioHit latest = audioHit();
  const bool newHit = latest.count != reportedHits;
  if (!status && !newHit && sent == length) {
    return;
  }
  const int available = Serial.availableForWrite();
  if (available <= 0) {
    return;
  }
  if (sent == length) {
    sent = length = 0;
    auto append = [&](const char *format, auto... args) {
      const size_t space = sizeof(buffer) - length;
      const int written = snprintf(buffer + length, space, format, args...);
      if (written > 0) {
        length += min(size_t(written), space - 1);
      }
    };

    if (newHit) {
      // One label per hit, the roll's included. Hits faster than USB keep only the latest.
      append("%s\n", drums_lane_name(latest.bank, latest.lane));
      reportedHits = latest.count;
    }
    if (status) {
      append("Talko 2 Drums %u.%u\n", unsigned(VersionMajor), unsigned(VersionMinor));
      append("BANK %u %s\n", unsigned(controls.bank), drums_bank_name(controls.bank));
      append("SOUND %u %s\n", unsigned(controls.lane) + 1,
             drums_lane_name(controls.bank, controls.lane));
      append("EFFECT %s page=%s\n", FxLabels[unsigned(controls.effect)].name,
             fxPage ? "FX" : "BANK");
      append("PANEL sound=%u pitch=%u fx=%u speed=%u gate=%u error=%u\n",
             unsigned(sound.value()), unsigned(controls.pitch), unsigned(controls.fx),
             unsigned(controls.speed), unsigned(gateHeld), unsigned(controls.error));
      append("MODE %s\n", ModeNames[unsigned(controls.mode)]);
      append("ENCODER detents=%lu invalid=%lu\n",
             (unsigned long)encoderDetents, (unsigned long)encoderInvalid);
      const AudioStats stats = audioStats();
      append("AUDIO budget_us=%lu peak_us=%lu late_blocks=%lu underrun_samples=%lu\n",
             (unsigned long)stats.budgetUs, (unsigned long)stats.peakUs,
             (unsigned long)stats.lateBlocks, (unsigned long)stats.underrunSamples);
      status = false;
    }
  }
  // Only write bytes the USB transport can take immediately.
  sent += Serial.write(reinterpret_cast<const uint8_t *>(buffer) + sent,
                       min(length - sent, size_t(available)));
}

} // namespace

void setup() {
  TalkoStartup::prepareLed();
  pinMode(GatePin, INPUT_PULLUP);
  pinMode(ErrorPin, INPUT_PULLUP);
  pinMode(EncoderA, INPUT_PULLUP);
  pinMode(EncoderB, INPUT_PULLUP);
  pinMode(SelectPin, INPUT_PULLUP);
  pinMode(ModeA, INPUT_PULLUP);
  pinMode(ModeB, INPUT_PULLUP);
  controls.mode = rawMode = readMode();
  for (uint pin = SegmentA; pin <= DecimalPoint; ++pin) {
    const uint slice = pwm_gpio_to_slice_num(pin);
    pwm_set_wrap(slice, 255);
    pwm_set_gpio_level(pin, 0);
    gpio_set_function(pin, GPIO_FUNC_PWM);
    pwm_set_enabled(slice, true);
  }

  analogReadResolution(12);
  sound.begin();
  pitch.begin();
  fxPot.begin();
  speed.begin();
  controls.lane = DRUM_LANES;
  readPots();
  controls.effect = Fx(fxCursor);
  delay(EncoderSettleMs);
  TalkoStartup::showVersion(VersionMajor, VersionMinor, drawStartup);
  lastEncoderState = encoderState();
  attachInterrupt(digitalPinToInterrupt(EncoderA), encoderInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(EncoderB), encoderInterrupt, CHANGE);
  drawDisplay(selectionGlyph(), DisplayBrightness);

  Serial.begin(115200);
  setupAudio(controls);
}

void loop() {
  const uint32_t now = millis();
  const bool pageChanged = selectButton.update(now) && selectButton.held;
  if (pageChanged) {
    fxPage = !fxPage;
  }
  const bool errorChanged = errorButton.update(now);
  controls.error = errorButton.held;
  const uint8_t bank = bankCursor;
  const Fx effect = Fx(fxCursor);
  const bool bankChanged = bank != controls.bank;
  const bool effectChanged = effect != controls.effect;
  controls.bank = bank;
  controls.effect = effect;
  const bool gateChanged = updateGate();
  const bool modeChanged = updateMode(now);
  const bool rising = gateChanged && gateHeld;
  if (rising) {
    ++controls.trigger;
  }
  controls.gate = gateHeld;
  static uint32_t lastPotRead = 0;
  const bool potsSampled = now - lastPotRead >= 1;
  bool potMoved = false;
  if (potsSampled) {
    lastPotRead = now;
    const bool soundMoved = sound.update();
    const bool pitchMoved = pitch.update();
    const bool fxMoved = fxPot.update();
    const bool speedMoved = speed.update();
    potMoved = soundMoved || pitchMoved || fxMoved || speedMoved;
    readPots();
  }

  static uint32_t lastDetents = 0;
  const uint32_t detents = encoderDetents;
  const bool encoderMoved = detents != lastDetents;
  lastDetents = detents;
  const bool selectionChanged = pageChanged || bankChanged || effectChanged;
  const bool activity = encoderMoved || selectionChanged || selectButton.held ||
                        errorChanged || controls.error || modeChanged || gateChanged ||
                        gateHeld || potMoved || audioHit().audible;
  updateDisplay(now, activity, selectionChanged);
  if (bankChanged || effectChanged || gateChanged || modeChanged || errorChanged ||
      potsSampled) {
    updateAudioControls(controls);
  }
  serviceConsole();
}
