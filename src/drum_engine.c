// SPDX-License-Identifier: GPL-3.0-only
// Talko banks of Felucca's DRUM voices (third_party/felucca, unmodified).
#include "drum_engine.h"

#define NVOICE 8
#define NPART 1
#include "../third_party/felucca/fe_tables.h"
#include "../third_party/felucca/felucca_engine.h"
#include "../third_party/felucca/dsp.c"
#include "../third_party/felucca/drum_voice.c"

_Static_assert(FS == DRUM_RATE && CTL == DRUM_BLOCK, "drum_engine.h does not match the Felucca tables");

enum { Voices = 8 };

// Bank 0 is Felucca's own kit and bank 9 its own CONGA, CLAVE and CYM on
// lanes 6..8 with the ROUND kick. Banks 1..8 are its model kits.
#define KIT(k)                                                               \
  { DV_KTYPE(k, 0), DV_KTYPE(k, 1), DV_KTYPE(k, 2), DV_KTYPE(k, 3),          \
    DV_KTYPE(k, 4), DV_KTYPE(k, 5), DV_KTYPE(k, 6), DV_KTYPE(k, 7) }
static const uint8_t Types[DRUM_BANKS][DRUM_LANES] = {
    {DVT_PUNCH, DVT_SNARE, DVT_CLAP, DVT_HATC, DVT_HATO, DVT_TOM, DVT_RIM, DVT_BELL},
    KIT(1), KIT(2), KIT(3), KIT(4), KIT(5), KIT(6), KIT(7), KIT(8),
    {DVT_ROUND, DVT_SNARE, DVT_CLAP, DVT_HATC, DVT_HATO, DVT_CONGA, DVT_CLAVE, DVT_CYM},
};
static const char *const BankNames[DRUM_BANKS] = {
    "STD", "80", "10", "66", "55", "77", "DEEP", "TIGHT", "PERC", "H+CYM",
};
// Felucca's lane names, with its swaps: 1 TOM -> CONGA, 2 RIM -> CLAVE, 4 BELL -> CYM.
static const uint8_t Swaps[DRUM_BANKS] = {0, 0, 4, 1, 0, 2 | 4, 4, 0, 1 | 2, 1 | 2 | 4};

typedef struct {
  dv_param_t p;
  dv_coef_t c;
  dv_voice_t v;
  dv_metal_t m;
  uint32_t age;
  uint32_t fade; // Q16
  uint32_t fadek; // Q15 per sample, 0 = not fading
  uint8_t bank, lane, on;
} drum_t;

static drum_t drums[Voices];
static drum_t *last;
static uint32_t ages;
static int32_t shapeTune;
static uint32_t shapeDecay = DRUM_DESIGN;

static void setup(drum_t *d) {
  d->p.tune = (int16_t)clamp(shapeTune, -DRUM_TUNE_RANGE, DRUM_TUNE_RANGE);
  d->p.decay = (uint8_t)shapeDecay;
  dv_setup(&d->c, &d->p);
  if (dv_uses_metal(d->c.type)) {
    dv_metal_tune(&d->m, &d->c);
  }
}

void drums_init(void) {
  for (uint32_t i = 0; i < Voices; ++i) {
    drums[i] = (drum_t){0};
  }
  last = 0;
  ages = 0;
  shapeTune = 0;
  shapeDecay = DRUM_DESIGN;
}

void drums_shape(int32_t tune16, uint32_t decay) {
  decay = decay > 127u ? 127u : decay;
  if (tune16 == shapeTune && decay == shapeDecay) {
    return;
  }
  shapeTune = tune16;
  shapeDecay = decay;
  for (uint32_t i = 0; i < Voices; ++i) {
    if (drums[i].on) {
      setup(&drums[i]);
    }
  }
}

static drum_t *voiceFor(uint32_t bank, uint32_t lane) {
  drum_t *oldest = &drums[0];
  // A lane is mono, as in Felucca: its next hit restrikes the same voice.
  for (uint32_t i = 0; i < Voices; ++i) {
    if (drums[i].on && drums[i].bank == bank && drums[i].lane == lane) {
      return &drums[i];
    }
  }
  for (uint32_t i = 0; i < Voices; ++i) {
    if (!drums[i].on) {
      return &drums[i];
    }
    if (drums[i].age < oldest->age) {
      oldest = &drums[i];
    }
  }
  return oldest;
}

void drums_hit(uint32_t bank, uint32_t lane, uint32_t accent) {
  if (bank >= DRUM_BANKS || lane >= DRUM_LANES) {
    return;
  }
  if (lane == DV_HATC) {
    for (uint32_t i = 0; i < Voices; ++i) {
      if (drums[i].on && drums[i].lane == DV_HATO) {
        dv_choke(&drums[i].v);
      }
    }
  }
  drum_t *d = voiceFor(bank, lane);
  dv_default(&d->p, Types[bank][lane]);
  d->p.accent = (uint8_t)(accent > 127u ? 127u : accent);
  setup(d);
  dv_trigger(&d->v);
  d->bank = (uint8_t)bank;
  d->lane = (uint8_t)lane;
  d->on = 1;
  d->fade = 65536u;
  d->fadek = 0;
  d->age = ++ages;
  last = d;
}

static void fadeOut(drum_t *d, uint32_t tau_us) {
  if (d->on && !d->fadek) {
    d->fadek = dv_k(tau_us);
  }
}

void drums_release(uint32_t tau_us) {
  if (last) {
    fadeOut(last, tau_us);
  }
}

void drums_choke_all(uint32_t tau_us) {
  for (uint32_t i = 0; i < Voices; ++i) {
    fadeOut(&drums[i], tau_us);
  }
}

void drums_render(int32_t *out, uint32_t n) {
  int32_t y[CTL], metal[CTL];
  for (uint32_t offset = 0; offset + CTL <= n; offset += CTL) {
    int32_t *block = out + offset;
    for (uint32_t i = 0; i < CTL; ++i) {
      block[i] = 0;
    }
    for (uint32_t k = 0; k < Voices; ++k) {
      drum_t *d = &drums[k];
      if (!d->on) {
        continue;
      }
      const int32_t *source = 0;
      if (dv_uses_metal(d->c.type)) {
        dv_metal_run(&d->m, metal, CTL);
        source = metal;
      }
      dv_run(&d->c, &d->v, source, y, CTL);
      if (d->fadek) {
        for (uint32_t i = 0; i < CTL; ++i) {
          y[i] = (y[i] * (int32_t)(d->fade >> 1)) >> 15;
          d->fade = (d->fade * d->fadek) >> 15;
        }
        if (d->fade < 16u) {
          d->v.live = 0;
        }
      }
      for (uint32_t i = 0; i < CTL; ++i) {
        block[i] += y[i];
      }
      if (!d->v.live) {
        d->on = 0;
        if (last == d) {
          last = 0;
        }
      }
    }
    // Each voice peaks near -7 dBFS. Overlapping hits go into a soft knee.
    for (uint32_t i = 0; i < CTL; ++i) {
      block[i] = soft_knee(block[i], 16384);
    }
  }
}

uint32_t drums_live(void) {
  uint32_t count = 0;
  for (uint32_t i = 0; i < Voices; ++i) {
    count += drums[i].on;
  }
  return count;
}

const char *drums_bank_name(uint32_t bank) {
  return bank < DRUM_BANKS ? BankNames[bank] : "";
}

const char *drums_lane_name(uint32_t bank, uint32_t lane) {
  static const char *const Names[DRUM_LANES] = {"KICK", "SNARE", "CLAP", "HATCL",
                                                "HATOP", "TOM", "RIM", "BELL"};
  if (bank >= DRUM_BANKS || lane >= DRUM_LANES) {
    return "";
  }
  const uint32_t swaps = Swaps[bank];
  if (lane == DV_TOM && (swaps & 1u)) return "CONGA";
  if (lane == DV_RIM && (swaps & 2u)) return "CLAVE";
  if (lane == DV_BELL && (swaps & 4u)) return "CYM";
  return Names[lane];
}
