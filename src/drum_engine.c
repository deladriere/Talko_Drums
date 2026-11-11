// SPDX-License-Identifier: GPL-3.0-only
// Talko banks: Felucca's DRUM voices (third_party/felucca) and Hortator's drum models
// (third_party/hortator through hortator_engine.c), all unmodified, sharing eight voices.
#include "drum_engine.h"
#include "hortator_engine.h"

#define NVOICE 8
#define NPART 1
#include "../third_party/felucca/fe_tables.h"
#include "../third_party/felucca/felucca_engine.h"
#include "../third_party/felucca/dsp.c"
#include "../third_party/felucca/drum_voice.c"

_Static_assert(FS == DRUM_RATE && CTL == DRUM_BLOCK, "drum_engine.h does not match the Felucca tables");

// A Felucca voice: validation/bench-200.txt, a whole kit struck at once, per voice at 133 MHz (191..214).
// A Hortator voice in a full mix costs about HortMixCostUs more than alone (hort_cost_us):
// validation/hardware-0.2-selftest.txt overshot the plan by 49..58 per voice in EFM and FM.
enum { Voices = HORT_SLOTS, FeluccaBanks = 6, FeluccaCostUs = 250, HortMixCostUs = 60, DeclickShift = 4 };
_Static_assert(Voices == 8, "one Hortator slot per voice");

enum { ENG_FELUCCA = 1, ENG_HORTATOR = 2 };

typedef struct {
  const char *name;
  const char *sound[DRUM_LANES];
  uint16_t gain[DRUM_LANES]; // Q12
  uint8_t hats;              // lane 3 (closed hat) chokes lane 4 (open hat)
} hort_bank_t;

// Banks 0..5: Felucca's own kit and its five model kits.
static const uint8_t FeluccaTypes[FeluccaBanks][DRUM_LANES] = {
    {DVT_PUNCH, DVT_SNARE, DVT_CLAP, DVT_HATC, DVT_HATO, DVT_TOM, DVT_RIM, DVT_BELL},
#define KIT(k)                                                                              \
  { DV_KTYPE(k, 0), DV_KTYPE(k, 1), DV_KTYPE(k, 2), DV_KTYPE(k, 3),                         \
    DV_KTYPE(k, 4), DV_KTYPE(k, 5), DV_KTYPE(k, 6), DV_KTYPE(k, 7) }
    KIT(1), KIT(2), KIT(3), KIT(4), KIT(5),
#undef KIT
};
static const char *const FeluccaNames[FeluccaBanks] = {"STD", "80", "10", "66", "55", "77"};
// Felucca's lane names, with its swaps: 1 TOM -> CONGA, 2 RIM -> CLAVE, 4 BELL -> CYM.
static const uint8_t Swaps[FeluccaBanks] = {0, 0, 4, 1, 0, 2 | 4};

// Banks 6..F: Hortator factory presets. Gains bring each peak to Felucca's level for its lane
// (tests/hortator_presets.cpp measures them).
static const hort_bank_t HortBanks[DRUM_BANKS - FeluccaBanks] = {
    {"808", {"ROUNDBD", "WARMSN", "HANDCLAP", "CLOSEDHH", "OPENHAT", "MIDTOM", "RIMSHOT", "COWBELL"},
     {4567, 3975, 9902, 17662, 26497, 4924, 3584, 5122}, 1},
    {"909", {"HOUSEBD", "CRISPSN", "SNAPCLAP", "LOOSEHAT", "WASHHAT", "HITOM", "HOTRIM", "CRASH"},
     {4584, 6950, 7805, 16862, 23942, 4933, 2791, 14632}, 1},
    {"TRX", {"SUBTHUMP", "HARDSNAP", "ROOMCLAP", "NOISEHAT", "AIROPEN", "SWOOPTOM", "KNOCK", "WASHCYM"},
     {2919, 9432, 10308, 36846, 35489, 13770, 10497, 23676}, 1},
    {"EFM", {"FMSUB", "TONESNR", "BUZZCLAP", "FMHAT", "TRASHHAT", "LONGTOM", "FMRIM", "FMCRASH"},
     {4559, 65535, 29050, 28071, 15105, 1814, 16527, 22051}, 1},
    {"DIRT", {"BIGBOOM", "POPSNARE", "WIDECLAP", "METALHAT", "DARKOPEN", "FUZZTOM", "GRITRIM", "THINCRSH"},
     {3301, 10658, 10910, 39227, 28379, 15842, 9832, 31104}, 1},
    {"ZAP", {"NOISEZAP", "RIMSNARE", "FMZAP", "SPITHAT", "TREMHAT", "DROPTOM", "DEEPBLIP", "GONG"},
     {3602, 14038, 36968, 15850, 21548, 2144, 30216, 6093}, 1},
    {"HAND", {"TUMBA", "CONGA", "SLAPCONG", "BONGO", "CLAVE", "WOODBLK", "LOWBELL", "CLINK"},
     {4162, 4736, 3565, 2337, 2654, 4243, 5091, 5901}, 0},
    {"DEEP", {"SUBBOOM", "TONALSN", "DARKCLAP", "DARKHAT", "DUSTYHAT", "FLOORTOM", "WOODRIM", "DARKCRSH"},
     {4729, 5163, 10516, 47906, 15128, 4850, 3275, 10786}, 1},
    {"TIGHT", {"TIGHTBD", "TIGHTSN", "DRYCLAP", "TICK", "HALFHAT", "PIP", "STICK", "SPLASH"},
     {4707, 7885, 7712, 51793, 32210, 3991, 6468, 16895}, 1},
    {"FM", {"RISEKICK", "FMNOISE", "RATTLE", "FMPERC", "SIZZRIDE", "FMTOM", "BLIP", "FMBELL"},
     {3769, 3404, 34233, 1847, 20730, 3046, 3923, 2425}, 0},
};
_Static_assert(FeluccaBanks + sizeof HortBanks / sizeof HortBanks[0] == DRUM_BANKS, "16 banks");

typedef struct {
  dv_param_t p;
  dv_coef_t c;
  dv_voice_t v;
  dv_metal_t m;
  uint32_t age;
  uint32_t fade;  // Q16
  uint32_t fadek; // Q15 per sample, 0 = not fading
  uint32_t cost;  // hort_cost_us units
  uint32_t preset;
  int32_t last;   // last sample added, for the declick when stolen
  uint16_t gain;  // Q12, Hortator voices
  uint8_t bank, lane, on, engine;
} drum_t;

static drum_t drums[Voices];
static drum_t *last;
static uint32_t ages;
static int32_t shapeTune;
static uint32_t shapeDecay = DRUM_DESIGN;
static uint32_t budget = DRUM_COST_FLOOR;
static int32_t declick;
static uint16_t presets[DRUM_BANKS - FeluccaBanks][DRUM_LANES];

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
  hort_init();
  for (uint32_t b = 0; b < DRUM_BANKS - FeluccaBanks; ++b) {
    for (uint32_t l = 0; l < DRUM_LANES; ++l) {
      presets[b][l] = (uint16_t)hort_preset(HortBanks[b].sound[l]);
    }
  }
  last = 0;
  ages = 0;
  declick = 0;
  shapeTune = 0;
  shapeDecay = DRUM_DESIGN;
}

void drums_budget(uint32_t us133) {
  budget = us133 < DRUM_COST_FLOOR ? DRUM_COST_FLOOR : us133;
}

void drums_shape(int32_t tune16, uint32_t decay) {
  decay = decay > 127u ? 127u : decay;
  if (tune16 == shapeTune && decay == shapeDecay) {
    return;
  }
  shapeTune = tune16;
  shapeDecay = decay;
  for (uint32_t i = 0; i < Voices; ++i) {
    if (drums[i].on && drums[i].engine == ENG_FELUCCA) {
      setup(&drums[i]);
    }
  }
}

static void stop(drum_t *d) {
  if (!d->on) {
    return;
  }
  declick += d->last;
  d->last = 0;
  d->on = 0;
  if (d->engine == ENG_HORTATOR) {
    hort_stop((uint32_t)(d - drums));
  }
  if (last == d) {
    last = 0;
  }
}

static drum_t *oldest(const drum_t *except) {
  drum_t *o = 0;
  for (uint32_t i = 0; i < Voices; ++i) {
    if (drums[i].on && &drums[i] != except && (!o || drums[i].age < o->age)) {
      o = &drums[i];
    }
  }
  return o;
}

static drum_t *voiceFor(uint32_t bank, uint32_t lane) {
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
  }
  drum_t *o = oldest(0);
  stop(o);
  return o;
}

// Older hits stop until the new one fits the budget.
static void makeRoom(const drum_t *d, uint32_t cost) {
  for (;;) {
    uint32_t total = cost;
    for (uint32_t i = 0; i < Voices; ++i) {
      if (drums[i].on && &drums[i] != d) {
        total += drums[i].cost;
      }
    }
    drum_t *o = oldest(d);
    if (total <= budget || !o) {
      return;
    }
    stop(o);
  }
}

static void fadeOut(drum_t *d, uint32_t tau_us) {
  if (d->on && !d->fadek) {
    d->fadek = dv_k(tau_us);
  }
}

void drums_hit(uint32_t bank, uint32_t lane, uint32_t accent) {
  if (bank >= DRUM_BANKS || lane >= DRUM_LANES) {
    return;
  }
  const uint32_t engine = drums_bank_engine(bank);
  const uint32_t hats = engine == ENG_FELUCCA || HortBanks[bank - FeluccaBanks].hats;
  if (hats && lane == DV_HATC) {
    for (uint32_t i = 0; i < Voices; ++i) {
      if (drums[i].on && drums[i].bank == bank && drums[i].lane == DV_HATO) {
        if (drums[i].engine == ENG_FELUCCA) {
          dv_choke(&drums[i].v);
        } else {
          fadeOut(&drums[i], 6000);
        }
      }
    }
  }
  accent = accent > 127u ? 127u : accent;
  drum_t *d = voiceFor(bank, lane);
  const uint32_t slot = (uint32_t)(d - drums);
  if (d->on && d->engine != engine) {
    stop(d);
  }
  if (engine == ENG_FELUCCA) {
    makeRoom(d, FeluccaCostUs);
    dv_default(&d->p, FeluccaTypes[bank][lane]);
    d->p.accent = (uint8_t)accent;
    setup(d);
    dv_trigger(&d->v);
    d->cost = FeluccaCostUs;
  } else {
    const uint32_t preset = presets[bank - FeluccaBanks][lane];
    d->cost = hort_cost_us(preset) + HortMixCostUs;
    makeRoom(d, d->cost);
    if (d->on) {
      declick += d->last;
    }
    const int32_t semis = (shapeTune + (shapeTune < 0 ? -8 : 8)) / 16;
    hort_start(slot, preset, semis, shapeDecay, 96u + accent / 4u);
    d->preset = preset;
    d->gain = HortBanks[bank - FeluccaBanks].gain[lane];
  }
  d->engine = (uint8_t)engine;
  d->bank = (uint8_t)bank;
  d->lane = (uint8_t)lane;
  d->on = 1;
  d->last = 0;
  d->fade = 65536u;
  d->fadek = 0;
  d->age = ++ages;
  last = d;
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

void drums_shed(void) {
  drum_t *o = oldest(0);
  if (o) {
    stop(o);
  }
}

void drums_render(int32_t *out, uint32_t n) {
  int32_t y[CTL], metal[CTL];
  for (uint32_t offset = 0; offset + CTL <= n; offset += CTL) {
    int32_t *block = out + offset;
    for (uint32_t i = 0; i < CTL; ++i) {
      block[i] = declick;
      declick -= (declick >> DeclickShift) ? (declick >> DeclickShift) : (declick > 0) - (declick < 0);
    }
    for (uint32_t k = 0; k < Voices; ++k) {
      drum_t *d = &drums[k];
      if (!d->on) {
        continue;
      }
      uint32_t live;
      if (d->engine == ENG_FELUCCA) {
        const int32_t *source = 0;
        if (dv_uses_metal(d->c.type)) {
          dv_metal_run(&d->m, metal, CTL);
          source = metal;
        }
        dv_run(&d->c, &d->v, source, y, CTL);
        live = d->v.live;
      } else {
        for (uint32_t i = 0; i < CTL; ++i) {
          y[i] = 0;
        }
        live = hort_render(k, y, CTL);
        for (uint32_t i = 0; i < CTL; ++i) {
          y[i] = clamp((y[i] * (int32_t)(d->gain >> 4)) >> 8, -65535, 65535);
        }
      }
      if (d->fadek) {
        for (uint32_t i = 0; i < CTL; ++i) {
          y[i] = (y[i] * (int32_t)(d->fade >> 1)) >> 15;
          d->fade = (d->fade * d->fadek) >> 15;
        }
        if (d->fade < 16u) {
          live = 0;
        }
      }
      for (uint32_t i = 0; i < CTL; ++i) {
        block[i] += y[i];
      }
      d->last = y[CTL - 1];
      if (!live) {
        d->last = 0;
        stop(d);
      }
    }
    hort_block();
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

uint32_t drums_bank_engine(uint32_t bank) {
  return bank < FeluccaBanks ? ENG_FELUCCA : ENG_HORTATOR;
}

const char *drums_bank_name(uint32_t bank) {
  if (bank >= DRUM_BANKS) {
    return "";
  }
  return bank < FeluccaBanks ? FeluccaNames[bank] : HortBanks[bank - FeluccaBanks].name;
}

const char *drums_lane_name(uint32_t bank, uint32_t lane) {
  static const char *const Names[DRUM_LANES] = {"KICK", "SNARE", "CLAP", "HATCL",
                                                "HATOP", "TOM", "RIM", "BELL"};
  if (bank >= DRUM_BANKS || lane >= DRUM_LANES) {
    return "";
  }
  if (bank >= FeluccaBanks) {
    return HortBanks[bank - FeluccaBanks].sound[lane];
  }
  const uint32_t swaps = Swaps[bank];
  if (lane == DV_TOM && (swaps & 1u)) return "CONGA";
  if (lane == DV_RIM && (swaps & 2u)) return "CLAVE";
  if (lane == DV_BELL && (swaps & 4u)) return "CYM";
  return Names[lane];
}
