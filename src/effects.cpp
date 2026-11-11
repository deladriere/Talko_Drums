// SPDX-License-Identifier: GPL-3.0-only
// Reverb, ring mod, phaser and tape follow Talko Monk's effects, at 44.1 kHz.
#include "effects.h"

#include <cmath>
#include <cstring>

namespace {

constexpr int32_t Unity = 32768;
constexpr uint32_t PhasePerHz = UINT32_MAX / FxSampleRate;

int32_t scale(int32_t sample, int32_t gain) {
  return (sample * gain) / Unity;
}

int16_t limit(int32_t sample) {
  return sample < -32768 ? -32768 : sample > 32767 ? 32767 : int16_t(sample);
}

template <unsigned Length> struct DelayLine {
  int16_t samples[Length];
  unsigned index;

  int32_t read() const { return samples[index]; }

  void write(int32_t sample) {
    samples[index] = limit(sample);
    if (++index == Length) {
      index = 0;
    }
  }
};

template <unsigned Length> struct Comb {
  DelayLine<Length> delay;
  int32_t damped;

  int32_t tick(int32_t input, int32_t feedback) {
    const int32_t tail = delay.read();
    damped += (tail - damped) / 4;
    delay.write(input + scale(damped, feedback));
    return tail;
  }
};

template <unsigned Length> struct Diffuser {
  DelayLine<Length> delay;

  int32_t tick(int32_t input) {
    const int32_t delayed = delay.read();
    const int32_t next = limit(input + delayed / 2);
    delay.write(next);
    return delayed - next / 2;
  }
};

// Monk's room, with its delays doubled to keep their times at 44.1 kHz.
struct Space {
  Comb<1306> combA;
  Comb<1594> combB;
  Comb<1966> combC;
  Comb<2342> combD;
  Diffuser<226> diffuseA;
  Diffuser<94> diffuseB;

  int32_t tick(int32_t input, int32_t amount) {
    const int32_t feedback = 16384 + scale(amount, 12500);
    const int32_t early = (combA.tick(input, feedback) + combB.tick(input, feedback) +
                           combC.tick(input, feedback) + combD.tick(input, feedback)) / 4;
    return diffuseB.tick(diffuseA.tick(early));
  }
};

struct Echo {
  static constexpr unsigned Frames = FxSampleRate * 3 / 10;
  int16_t samples[Frames];
  unsigned index;
  int32_t damped;

  int32_t tick(int32_t input, int32_t amount) {
    const int32_t tail = samples[index];
    // Darken each repeat so the echoes sit behind the next hit.
    damped += (tail - damped) / 3;
    const int32_t feedback = 9830 + scale(amount, 14745);
    samples[index] = limit(input + scale(damped, feedback));
    if (++index == Frames) {
      index = 0;
    }
    return tail;
  }
};

struct Crush {
  int32_t held;
  unsigned count;

  int16_t tick(int32_t dry, int32_t amount) {
    if (count == 0) {
      const int32_t bits = 12 - amount * 8 / Unity;
      const int32_t step = 1 << (16 - bits);
      held = dry / step * step;
      count = 1 + unsigned(amount) * 15 / Unity;
    }
    --count;
    const int32_t mix = amount < Unity / 8 ? amount * 8 : Unity;
    return limit(dry + scale(held - dry, mix));
  }
};

uint32_t phase = 0;
uint32_t phaseStep = 0;
int16_t sine[256];

struct Tape {
  static constexpr unsigned BufferFrames = 4096;
  static constexpr unsigned BufferMask = BufferFrames - 1;
  static constexpr unsigned Fraction = 256;
  static constexpr unsigned MinDelay = 32;
  static constexpr int32_t FullMixAt = Unity / 4;
  static constexpr unsigned FadeFrames = 256;
  // About 18 ms between the two heads, as Monk aims for.
  static constexpr unsigned HalfWindow = 800;
  static constexpr unsigned WindowReciprocal = (1u << 20) / HalfWindow;
  static constexpr unsigned Warmup = 2 * HalfWindow + MinDelay;
  static_assert(Warmup + FadeFrames < BufferFrames);

  int16_t samples[BufferFrames];
  unsigned write, filled, delayPhase;
  uint32_t wowPhase, flutterPhase;
  int32_t tone, motor, target, mix;

  void setControls(uint16_t knob) {
    mix = knob < FullMixAt ? knob * (Unity / FullMixAt) : Unity;
    // Fade in at half speed, then sweep through unity at noon to double speed.
    const int32_t speedKnob = knob < FullMixAt ? FullMixAt : knob;
    const int32_t centered = speedKnob - Unity / 2;
    const int32_t speedChange = centered > -384 && centered < 384 ? 0 : centered / 64;
    target = -speedChange * int32_t(Fraction);
  }

  int32_t read(unsigned delay) const {
    const unsigned position = write * Fraction - (MinDelay * Fraction + delay);
    const unsigned index = position / Fraction;
    const int32_t fraction = position % Fraction;
    const int32_t first = samples[index & BufferMask];
    const int32_t next = samples[(index + 1) & BufferMask];
    return first + (next - first) * fraction / int32_t(Fraction);
  }

  int16_t tick(int32_t dry) {
    tone += (dry - tone) * 3 / 8;
    samples[write] = limit(tone);
    motor += (target - motor) / 256;
    wowPhase += PhasePerHz * 13 / 10;
    flutterPhase += PhasePerHz * 76 / 10;
    const int32_t wow = sine[wowPhase >> 24] / 2048;
    const int32_t flutter = sine[flutterPhase >> 24] / 8192;
    // Crossfade between two moving heads as their delays wrap.
    constexpr unsigned headOffset = HalfWindow * Fraction;
    constexpr unsigned sweep = 2 * headOffset;
    const unsigned other = delayPhase < headOffset ? delayPhase + headOffset : delayPhase - headOffset;
    const unsigned triangle = delayPhase < headOffset ? delayPhase : sweep - delayPhase;
    const int32_t weight = (triangle * WindowReciprocal) >> 13;
    const int32_t wet = (read(delayPhase) * weight + read(other) * (Unity - weight)) / Unity;
    int32_t nextPhase = int32_t(delayPhase) + (motor >> 8) + wow + flutter;
    if (nextPhase < 0) {
      nextPhase += int32_t(sweep);
    } else if (unsigned(nextPhase) >= sweep) {
      nextPhase -= int32_t(sweep);
    }
    delayPhase = nextPhase;
    write = (write + 1) & BufferMask;

    if (filled < Warmup + FadeFrames) {
      ++filled;
    }
    if (mix == 0 || filled <= Warmup) {
      return limit(dry);
    }
    const unsigned ready = filled - Warmup;
    const int32_t wetMix = ready < FadeFrames ? (mix * int32_t(ready)) / FadeFrames : mix;
    return limit(dry + scale(wet - dry, wetMix));
  }
};

Space space{};
Echo echo{};
Crush crush{};
Tape tape{};
Fx selected = Fx::Off;
int32_t wetAmount = 0;
int32_t phaserState[4];

int16_t phaser(int32_t dry) {
  const int32_t coefficient = 17300 + scale(sine[phase >> 24], 12900);
  phase += phaseStep;
  int32_t wet = dry;
  for (int32_t &state : phaserState) {
    const int32_t stage = limit(state - scale(wet, coefficient));
    state = wet + scale(stage, coefficient);
    wet = stage;
  }
  return limit(dry + scale(wet - dry, wetAmount / 2));
}

} // namespace

uint16_t fxAmount(uint16_t adc) {
  if (adc <= FxAdcFloor) {
    return 0;
  }
  return (uint32_t(adc - FxAdcFloor) * Unity) / (FxAdcMax - FxAdcFloor);
}

void setupEffects() {
  constexpr float Tau = 6.28318530718f;
  for (unsigned i = 0; i < 256; ++i) {
    sine[i] = int16_t(std::sin(i * (Tau / 256)) * 32767);
  }
}

void setEffect(Fx mode, uint16_t amount) {
  if (mode != selected) {
    if (mode == Fx::Reverb) {
      std::memset(&space, 0, sizeof(space));
    }
    if (mode == Fx::Delay) {
      std::memset(&echo, 0, sizeof(echo));
    }
    if (mode == Fx::Tape) {
      std::memset(&tape, 0, sizeof(tape));
    }
    crush = {};
    std::memset(phaserState, 0, sizeof(phaserState));
    phase = 0;
    selected = mode;
  }
  wetAmount = amount;
  if (mode == Fx::Tape) {
    tape.setControls(amount);
  } else if (mode == Fx::RingMod) {
    phaseStep = (30u + (uint32_t(amount) * 1170u) / Unity) * PhasePerHz;
  } else if (mode == Fx::Phaser) {
    phaseStep = PhasePerHz * 55 / 100;
  }
}

// Keep the per-sample path in SRAM so UI work on the other core does not
// evict its instructions from the shared flash cache.
#if defined(ARDUINO_ARCH_RP2040)
__attribute__((section(".time_critical.drums_effect")))
#endif
int16_t processEffect(int32_t sample) {
  if (wetAmount == 0) {
    // Keep space and echo tails decaying and Tape history current while FX is down.
    if (selected != Fx::Reverb && selected != Fx::Delay && selected != Fx::Tape) {
      return limit(sample);
    }
  }
  switch (selected) {
  case Fx::Reverb:
    return limit(sample + scale(space.tick(sample / 2, wetAmount), wetAmount));
  case Fx::Delay:
    return limit(sample + scale(echo.tick(sample, wetAmount), wetAmount * 3 / 4));
  case Fx::Crush:
    return crush.tick(sample, wetAmount);
  case Fx::RingMod: {
    const int32_t modulated = scale(sample, sine[phase >> 24]);
    phase += phaseStep;
    return limit(sample + scale(modulated - sample, wetAmount));
  }
  case Fx::Phaser:
    return phaser(sample);
  case Fx::Tape:
    return tape.tick(sample);
  default:
    return limit(sample);
  }
}
