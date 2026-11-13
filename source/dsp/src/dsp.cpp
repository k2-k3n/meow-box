// dsp.cpp: Meow Voice.
#include "dsp.h"

namespace {

// Formant targets through a meow: M (closed), E, A, O, W (closing). Hz for a house cat.
const float kF1[5] = {320.0f, 620.0f, 950.0f, 600.0f, 340.0f};
const float kF2[5] = {1100.0f, 2500.0f, 1700.0f, 1000.0f, 750.0f};
const float kF3[5] = {2600.0f, 3400.0f, 3000.0f, 2700.0f, 2500.0f};
const float kOpen[5] = {0.25f, 0.9f, 1.0f, 0.8f, 0.2f};   // how open the mouth is

float along(const float *v, float p)   // p: 0..1 through the five shapes
{
    const float x = fx::clampf(p, 0.0f, 0.9999f) * 4.0f;
    const int i = (int)x;
    return v[i] + (v[i + 1] - v[i]) * (x - (float)i);
}

void reset_voice(Dsp &d)
{
    d.track_lp.reset(); d.track_hp.reset(); d.track_env.reset(); d.env.reset(); d.env_slow.reset();
    d.side = 0; d.since_rise = 0.0f; d.note_log = 7.5f;
    d.t = 0.0f; d.ph = 0.0f; d.jitter = 0.0f; d.amp = 0.0f; d.vib = 0.0f;
    d.singing = false; d.gate_open = false; d.onset_armed = true;
    d.f1.reset(); d.f2.reset(); d.f3.reset(); d.breath_lp.reset(); d.purr_lp.reset();
    d.purr_ph = 0.0f; d.quiet = 0.0f; d.tick = 0;
    d.inc = 0.004f; d.g1 = d.g2 = d.g3 = 0.05f;
    d.rng.reset();
}

}  // namespace

bool dsp_init(Dsp &d, fx::Pool &)
{
    d.track_lp.set(fx::Biquad::LP, 700.0f, 0.55f);
    d.track_hp.set(fx::Biquad::HP, 60.0f, 0.7f);
    d.track_env.set(1.0f, 30.0f);
    d.env.set(2.0f, 120.0f);
    d.env_slow.set(40.0f, 200.0f);
    d.breath_lp.set_hz(5000.0f);
    d.purr_lp.set_hz(900.0f);
    d.rng.seed(0x4D454F57u);
    d.limiter.init();
    reset_voice(d);
    dsp_param(d, P_SIZE, kParams[P_SIZE].def);
    dsp_param(d, P_LENGTH, kParams[P_LENGTH].def);
    d.mix.init(0.8f, 30.0f);
    d.level.init(1.0f, 30.0f);
    return true;
}

void dsp_settle(Dsp &d) { d.mix.snap(); d.level.snap(); }
void dsp_reset(Dsp &d) { reset_voice(d); d.limiter.reset(); dsp_settle(d); }

void dsp_param(Dsp &d, uint32_t index, float v)
{
    switch (index) {
    case P_MIX: d.mix.set(fx::pct(v)); break;
    case P_SIZE: {
        const float size = fx::pct(v);
        d.formantScale = resona_exp2f(0.55f - 1.3f * size);
        d.octave = 1.0f - 1.8f * size;
        break;
    }
    case P_LENGTH:
        d.len = fx::ms_to_samples(fx::clampf(v, kParams[P_LENGTH].min, kParams[P_LENGTH].max));
        d.invLen = 1.0f / d.len;
        break;
    case P_BEND: d.bend = fx::pct(v); break;
    case P_PURR: d.purr_amt = fx::pct(v); break;
    case P_LEVEL: d.level.set(resona_db_to_lin(v)); break;
    default: break;
    }
}

void dsp_process(Dsp &d, const float *in, float *out, uint32_t frames)
{
    const float thr = 0.004f;   // about -48 dBFS: quieter than this is not a note
    for (uint32_t i = 0; i < frames; ++i) {
        const float x = in[i];
        const float e = d.env.process(x);
        const float es = d.env_slow.process(x);

        // pitch: Schmitt trigger on the filtered string
        const float f = d.track_hp.process(d.track_lp.process(x));
        const float h = 0.25f * d.track_env.process(f) + 1.0e-5f;
        d.since_rise += 1.0f;
        if (d.side <= 0 && f > h) {
            d.side = 1;
            const float hz = fx::kRate / d.since_rise;
            if (hz > 65.0f && hz < 1400.0f && e > thr) d.note_log += 0.5f * (resona_log2f(hz) - d.note_log);
            d.since_rise = 0.0f;
        } else if (d.side >= 0 && f < -h) {
            d.side = -1;
        }

        // a new pick starts a new meow
        if (!d.gate_open && e > thr * 1.5f) {
            d.gate_open = true; d.singing = true; d.t = 0.0f; d.onset_armed = false;
        } else if (d.gate_open && e < thr) {
            d.gate_open = false; d.onset_armed = true;
        }
        // A later, softer pick need only rise above the recent playing level.
        // Re-arm below that level so a sustained note cannot repeatedly restart itself.
        if (d.gate_open && e < es * 1.12f + thr * 0.5f) d.onset_armed = true;
        if (d.gate_open && d.onset_armed && e > es * 1.45f + thr * 0.5f
            && e > thr * 3.0f && d.t > 0.25f * d.len) {
            d.singing = true; d.t = 0.0f; d.onset_armed = false;
        }

        if ((d.tick++ & 7u) == 0u) {
            float p = 1.0f;
            if (d.singing) {
                p = d.t * d.invLen;
                if (p >= 1.0f) { d.singing = false; p = 1.0f; }
            }
            // kitten (small head: high voice, high formants) .. lion
            // the meow's melody: up a little on "e", then falling away through "ow"
            const float contour = d.bend * (0.33f * fx::sin01(fx::clampf(p * 0.6f, 0.0f, 0.5f)) - 0.42f * p * p);
            d.vib += 5.5f * 8.0f * fx::kInvRate; if (d.vib >= 1.0f) d.vib -= 1.0f;
            const float vib = 0.012f * fx::sin01(d.vib) * p;
            d.inc = resona_exp2f(d.note_log + d.octave + contour + vib) * fx::kInvRate;
            const float pi_fs = fx::kPi * fx::kInvRate;
            d.g1 = fx::tan_small(fx::clampf(along(kF1, p) * d.formantScale, 150.0f, 7000.0f) * pi_fs);
            d.g2 = fx::tan_small(fx::clampf(along(kF2, p) * d.formantScale, 300.0f, 7500.0f) * pi_fs);
            d.g3 = fx::tan_small(fx::clampf(along(kF3, p) * d.formantScale, 600.0f, 8000.0f) * pi_fs);
            const float open = along(kOpen, p);
            // loudness: the mouth's opening times how hard the note was picked, fading at the end
            const float target = d.singing ? open * fx::clampf(es * 6.0f + 0.25f, 0.0f, 1.0f) : 0.0f;
            // Keep the voiced "w" for a short release instead of cutting it off and
            // leaving the separate purr noise as the only audible tail.
            const float slew = d.singing ? 0.06f : 0.003f;
            d.amp = fx::flush(d.amp + slew * (target - d.amp));
            d.jitter = 0.004f * d.rng.bi();
        }
        if (d.singing) d.t += 1.0f;

        // the voice: a bright pulse-ish source with a little breath, through the formants
        const float inc = d.inc * (1.0f + d.jitter);
        d.ph += inc; if (d.ph >= 1.0f) d.ph -= 1.0f;
        const float saw = 2.0f * d.ph - 1.0f - fx::blep(d.ph, inc);
        const float src = (saw + 0.12f * d.breath_lp.process(d.rng.bi())) * d.amp;
        float lp, b1, b2, b3;
        d.f1.process(src, d.g1, 0.14f, lp, b1);
        d.f2.process(src, d.g2, 0.10f, lp, b2);
        d.f3.process(src, d.g3, 0.12f, lp, b3);
        const float meow = (b1 * 0.14f + b2 * 0.08f + b3 * 0.04f) * 1.25f;

        // purr: a 25 Hz rattle of low noise while you play softly or let a note ring
        d.quiet += 0.0005f * (((d.gate_open && !d.singing) ? 1.0f : 0.0f) - d.quiet);
        d.purr_ph += 25.0f * fx::kInvRate; if (d.purr_ph >= 1.0f) d.purr_ph -= 1.0f;
        const float rattle = d.purr_ph < 0.35f ? 1.0f - d.purr_ph / 0.35f : 0.0f;
        const float purr = d.purr_lp.process(d.rng.bi()) * rattle * d.purr_amt * d.quiet * 0.25f;

        const float m = d.mix.next();
        out[i] = d.limiter.process((x * (1.0f - 0.85f * m) + (meow + purr) * m) * d.level.next());
    }
}
