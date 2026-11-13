// dsp.h: Meow Voice's state.
#ifndef DSP_H
#define DSP_H

#include <stdint.h>

#include "fx_core.hpp"
#include "params.h"

constexpr uint32_t kPoolFloats = 16u;   // no long buffers

struct Dsp {
    // pitch tracking (as a guitar synth does it)
    fx::Biquad track_lp, track_hp;
    fx::Follower track_env, env, env_slow;
    int side;
    float since_rise, note_log;
    // the meow
    float t, len, invLen, bend, formantScale, octave, ph, jitter, amp, vib;
    bool singing, gate_open, onset_armed;
    fx::Svf f1, f2, f3;
    fx::Rng rng;
    fx::LowPass1 breath_lp;
    // the purr
    float purr_ph, purr_amt, quiet;
    fx::LowPass1 purr_lp;
    uint32_t tick;
    float inc, g1, g2, g3;   // per-8-sample control values
    fx::Limiter limiter;
    fx::Smooth mix, level;
};

bool dsp_init(Dsp &d, fx::Pool &pool);
void dsp_settle(Dsp &d);
void dsp_reset(Dsp &d);
void dsp_param(Dsp &d, uint32_t index, float value);
void dsp_process(Dsp &d, const float *in, float *out, uint32_t frames);

#endif
