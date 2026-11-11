// SPDX-License-Identifier: GPL-3.0-only
// The parts of Hortator's core.h and eng_sample.c its drum models need (Copyright (C) 2026 Leo Kuroshita,
// Hügelton Instruments; drum machine fork 2026 DEADACTIVE), then the models themselves, unmodified.
#include "hortator_engine.h"

#include <string.h>

#include "../third_party/hortator/felucca_tables.h"
#include "../third_party/hortator/dsp.c"
#include "../third_party/hortator/dm_state.h"

#define NTRK HORT_SLOTS
#define NDV 2

enum {
  F_INT, F_PCT, F_BIPCT, F_TIME, F_LFOHZ, F_CUTOFF, F_DB, F_SEMI, F_ENUM, F_BPM, F_NOTE,
  F_ONOFF, F_OCT, F_STEPS, F_CTHR, F_CRAT, F_CATK, F_CREL, F_CMKUP, F_LRATE1, F_LRATE2, F_LDEST, F_LPHASE,
  F_NOTEO, F_RDECAY, F_FDEC, F_SRR, F_SLOT, F_LSLOT
};

typedef struct {
  const char *label;
  uint8_t fmt;
  int16_t min, max, def;
  const char *const *names;
  const char *unit;
} param_desc_t;
#define PD(l, f, mn, mx, df) {l, f, mn, mx, df, 0, 0}
#define PE(l, n, df) {l, F_ENUM, 0, (int16_t)(sizeof(n) / sizeof(n[0]) - 1), df, n, 0}

enum { P_MODEL, P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7, P_COUNT };

typedef struct {
  uint32_t ph[3];
  int32_t s[8];
} voice_t;

typedef struct { int32_t a1, a2, a3, k; } dsvf_t;

typedef struct {
  uint8_t active, vel;
  uint32_t age;
  uint32_t t;
  uint32_t ph[3], inc[3];
  int32_t env[3];
  uint32_t k[3];
  dsvf_t c[2];
  int32_t f[4];
  int32_t x[4];
  int32_t rng;
  int32_t last;
  voice_t sv;
  dm_state_t ms;
} dvoice_t;

struct track;
typedef struct {
  const char *name;
  uint8_t voices;
  uint8_t choke;
  param_desc_t edit[8];
  void (*trigger)(struct track *t, dvoice_t *v);
  void (*render)(struct track *t, dvoice_t *v, int32_t *out, uint32_t n);
  uint8_t weight;
  void (*glide)(struct track *t, dvoice_t *v);
  void (*gate)(struct track *t, dvoice_t *v, int open);
} dmodel_t;

typedef struct track {
  int16_t p[P_COUNT];
  uint8_t model;
  dvoice_t v[NDV];
  int32_t dtail;
  int8_t nt;
} track_t;

static track_t trk[NTRK];
static uint32_t dblock;

static uint32_t pow2_q16(int32_t d16) {
  uint32_t u = (uint32_t)(d16 + 192 * 16), oct = u / 192u;
  uint32_t r = PITCH_INC[1600 + u % 192u] / (PITCH_INC[1600] >> 16);
  return oct >= 16u ? r << (oct - 16u) : r >> (16u - oct);
}

// Hortator keeps the engine view's private copies in its own ".pool" section; they are never used here.
#define section(name) aligned(4)
#include "../third_party/hortator/dm_dsp.c"
#include "../third_party/hortator/phys_dsp.c"
#include "../third_party/hortator/dm_kick.c"
#include "../third_party/hortator/dm_snare.c"
#include "../third_party/hortator/dm_metal.c"
#include "../third_party/hortator/dm_perc.c"
#include "../third_party/hortator/dm_memb.c"
#include "../third_party/hortator/dm_fm.c"
#include "../third_party/hortator/dm_ktrx.c"
#include "../third_party/hortator/dm_sdtrx.c"
#include "../third_party/hortator/dm_xttrx.c"
#include "../third_party/hortator/dm_trxn.c"
#include "../third_party/hortator/dm_rstrx.c"
#include "../third_party/hortator/dm_trxm.c"
#include "../third_party/hortator/dm_trxcb.c"
#include "../third_party/hortator/dm_cltrx.c"
#include "../third_party/hortator/dm_b2trx.c"
#include "../third_party/hortator/dm_bdefm.c"
#include "../third_party/hortator/dm_efmn.c"
#include "../third_party/hortator/dm_efmm.c"
#undef section

// Hortator's dmodels.c order. SAMPLE and ACID (a bass line) are not built here: they have no table entry.
enum { DM_K808, DM_K909, DM_S808, DM_S909, DM_C808, DM_C909, DM_HATC, DM_HATO, DM_CYMB, DM_COWB, DM_TOM, DM_CONGA,
       DM_RIM, DM_CLAVE, DM_SMPL, DM_KBOOM, DM_KPUNC, DM_SSNAP, DM_SCRAK, DM_HMETL, DM_HNOIS, DM_MEMB, DM_FM, DM_ACID,
       DM_KTRX, DM_SDTRX, DM_XTTRX, DM_XCTRX, DM_CPTRX, DM_RSTRX, DM_CBTRX, DM_CHTRX, DM_OHTRX, DM_CYTRX, DM_MATRX,
       DM_CLTRX, DM_B2TRX, DM_BDEFM, DM_SDEFM, DM_XTEFM, DM_CPEFM, DM_RSEFM, DM_CBEFM, DM_HHEFM, DM_CYEFM, NMODELS };
static const dmodel_t DMODELS[NMODELS] = {
    [DM_K808] = DM_K808_DEF,   [DM_K909] = DM_K909_DEF,   [DM_S808] = DM_S808_DEF,   [DM_S909] = DM_S909_DEF,
    [DM_C808] = DM_C808_DEF,   [DM_C909] = DM_C909_DEF,   [DM_HATC] = DM_HATC_DEF,   [DM_HATO] = DM_HATO_DEF,
    [DM_CYMB] = DM_CYMB_DEF,   [DM_COWB] = DM_COWB_DEF,   [DM_TOM] = DM_TOM_DEF,     [DM_CONGA] = DM_CONGA_DEF,
    [DM_RIM] = DM_RIM_DEF,     [DM_CLAVE] = DM_CLAVE_DEF, [DM_KBOOM] = DM_KBOOM_DEF, [DM_KPUNC] = DM_KPUNC_DEF,
    [DM_SSNAP] = DM_SSNAP_DEF, [DM_SCRAK] = DM_SCRAK_DEF, [DM_HMETL] = DM_HMETL_DEF, [DM_HNOIS] = DM_HNOIS_DEF,
    [DM_MEMB] = DM_MEMB_DEF,   [DM_FM] = DM_FM_DEF,       [DM_KTRX] = DM_KTRX_DEF,   [DM_SDTRX] = DM_SDTRX_DEF,
    [DM_XTTRX] = DM_XTTRX_DEF, [DM_XCTRX] = DM_XCTRX_DEF, [DM_CPTRX] = DM_CPTRX_DEF, [DM_RSTRX] = DM_RSTRX_DEF,
    [DM_CBTRX] = DM_CBTRX_DEF, [DM_CHTRX] = DM_CHTRX_DEF, [DM_OHTRX] = DM_OHTRX_DEF, [DM_CYTRX] = DM_CYTRX_DEF,
    [DM_MATRX] = DM_MATRX_DEF, [DM_CLTRX] = DM_CLTRX_DEF, [DM_B2TRX] = DM_B2TRX_DEF, [DM_BDEFM] = DM_BDEFM_DEF,
    [DM_SDEFM] = DM_SDEFM_DEF, [DM_XTEFM] = DM_XTEFM_DEF, [DM_CPEFM] = DM_CPEFM_DEF, [DM_RSEFM] = DM_RSEFM_DEF,
    [DM_CBEFM] = DM_CBEFM_DEF, [DM_HHEFM] = DM_HHEFM_DEF, [DM_CYEFM] = DM_CYEFM_DEF,
};

typedef struct { uint8_t model; int8_t k[8]; char name[9]; } preset_t;
enum { PB_MD, PB_MODERN, PB_CLASSIC, PB_USER, PB_N };
#define PRE_NUSER 96u
#include "../third_party/hortator/preset_bank.h"

static uint32_t ages;

void hort_init(void) {
  memset(trk, 0, sizeof trk);
  ages = 0;
  dblock = 0;
}

uint32_t hort_preset(const char *name) {
  for (uint32_t i = 0; i < PRE_NFACT; ++i) {
    if (DMODELS[PRE_FACTORY[i].model].trigger && strcmp(PRE_FACTORY[i].name, name) == 0) {
      return i;
    }
  }
  return HORT_NO_PRESET;
}

const char *hort_preset_name(uint32_t preset) {
  return preset < PRE_NFACT ? PRE_FACTORY[preset].name : "";
}

const char *hort_model_name(uint32_t preset) {
  return preset < PRE_NFACT ? DMODELS[PRE_FACTORY[preset].model].name : "";
}

// validation/bench-200.txt: each model's worst 128-sample block alone, from RAM at 200 MHz, in
// microseconds at 133 MHz (x 200 / 133, rounded up) like drums_budget. MATRX and CLTRX have no
// factory preset, so they were not measured.
static const uint16_t COST_US[NMODELS] = {
    [DM_K808] = 225, [DM_K909] = 200, [DM_S808] = 244, [DM_S909] = 321, [DM_C808] = 148,
    [DM_C909] = 146, [DM_HATC] = 187, [DM_HATO] = 187, [DM_CYMB] = 214, [DM_COWB] = 142, [DM_TOM] = 265,
    [DM_CONGA] = 181, [DM_RIM] = 217, [DM_CLAVE] = 182, [DM_KBOOM] = 1823, [DM_KPUNC] = 2004,
    [DM_SSNAP] = 2940, [DM_SCRAK] = 1912, [DM_HMETL] = 1182, [DM_HNOIS] = 1327, [DM_MEMB] = 3139,
    [DM_FM] = 178, [DM_KTRX] = 748, [DM_SDTRX] = 925, [DM_XTTRX] = 330, [DM_XCTRX] = 327,
    [DM_CPTRX] = 1279, [DM_RSTRX] = 223, [DM_CBTRX] = 2047, [DM_CHTRX] = 659, [DM_OHTRX] = 658,
    [DM_CYTRX] = 1226, [DM_B2TRX] = 504, [DM_BDEFM] = 807, [DM_SDEFM] = 808, [DM_XTEFM] = 862,
    [DM_CPEFM] = 772, [DM_RSEFM] = 879, [DM_CBEFM] = 709, [DM_HHEFM] = 551, [DM_CYEFM] = 614,
};

uint32_t hort_cost_us(uint32_t preset) {
  return preset < PRE_NFACT ? COST_US[PRE_FACTORY[preset].model] : 0;
}

void hort_start(uint32_t slot, uint32_t preset, int32_t semitones, uint32_t decay, uint32_t velocity) {
  if (slot >= NTRK || preset >= PRE_NFACT || !DMODELS[PRE_FACTORY[preset].model].trigger) {
    return;
  }
  const preset_t *pr = &PRE_FACTORY[preset];
  const dmodel_t *m = &DMODELS[pr->model];
  track_t *t = &trk[slot];
  dvoice_t *v = &t->v[0];
  t->model = pr->model;
  t->p[P_MODEL] = pr->model;
  for (uint32_t i = 0; i < 8u; ++i) {
    t->p[P_E0 + i] = pr->k[i];
  }
  // DECAY: the preset's at 64, shorter toward 0, longer toward 127, within the knob's range.
  const param_desc_t *d = &m->edit[1];
  const int32_t base = pr->k[1];
  const int32_t s = (int32_t)(decay > 127u ? 127u : decay);
  int32_t dec = s <= 64 ? d->min + (base - d->min) * s / 64 : base + (d->max - base) * (s - 64) / 63;
  t->p[P_E1] = (int16_t)clamp(dec, d->min, d->max);
  t->nt = (int8_t)clamp(semitones, -24, 24);
  t->dtail = 0;
  memset(v, 0, sizeof *v);
  v->active = 1;
  v->vel = (uint8_t)(velocity > 127u ? 127u : velocity < 1u ? 1u : velocity);
  v->age = ++ages;
  v->rng = (int32_t)(ages * 2654435761u) | 1;
  m->trigger(t, v);
}

void hort_stop(uint32_t slot) {
  if (slot < NTRK) {
    trk[slot].v[0].active = 0;
  }
}

uint32_t hort_render(uint32_t slot, int32_t *out, uint32_t n) {
  if (slot >= NTRK || !trk[slot].v[0].active) {
    return 0;
  }
  track_t *t = &trk[slot];
  const dmodel_t *m = &DMODELS[t->model];
  for (uint32_t offset = 0; offset + CTL <= n && t->v[0].active; offset += CTL) {
    m->render(t, &t->v[0], out + offset, CTL);
  }
  return t->v[0].active;
}

void hort_block(void) { ++dblock; }

uint32_t hort_q_overflows(void) {
#ifdef DM_QCHECK
  return dm_qover;
#else
  return 0;
#endif
}
