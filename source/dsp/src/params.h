// params.h: Meow Voice. Every note you pick is sung by a cat. The pitch is tracked from the
// guitar; a voice at that pitch runs through three formant filters that sweep "m-e-o-w" over
// the note while the pitch rises and falls like a real meow. SIZE goes from kitten to lion.
//
//   input -> pitch tracker -> voice (saw + breath) -> 3 formants sweeping M-E-O-W
//         -> MIX with the guitar -> limiter            PURR: a low rattle under quiet playing
#ifndef PARAMS_H
#define PARAMS_H

#include <stdint.h>

#include <resona/module.h>

#define MODULE_ID "org.resona.creative.meow-box.voice"
#define MODULE_NAME "Meow Voice"
#define MODULE_TITLE "MEOW VOICE"
#define MODULE_VERSION "1.0.3"

enum Param : uint32_t { P_MIX, P_SIZE, P_LENGTH, P_BEND, P_PURR, P_LEVEL, P_COUNT };

const resona_param kParams[P_COUNT] = {
    {"MIX", 0.0f, 100.0f, 80.0f, "%"},
    {"SIZE", 0.0f, 100.0f, 30.0f, "%"},
    {"LENGTH", 150.0f, 1500.0f, 550.0f, "ms"},
    {"BEND", 0.0f, 100.0f, 60.0f, "%"},
    {"PURR", 0.0f, 100.0f, 0.0f, "%"},
    {"LEVEL", -12.0f, 6.0f, 0.0f, "dB"},
};

#define PX_NONE {0, 0.0f, 0, nullptr}
const resona_param_ext kParamExt[P_COUNT] = {PX_NONE, PX_NONE, PX_NONE, PX_NONE, PX_NONE, PX_NONE};

#endif
