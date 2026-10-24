// Original Jerzy Audio implementation; not copied from Drum Foundry.
// Fixed-size, allocation-free multi-layer percussive voice (C++17).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace jerzy_drum {
struct LayeredVoice {
    double sampleRate = 48000.0;
    double bodyPhase = 0.0, partialPhase = 0.0;
    float bodyEnv = 0.0f, noiseEnv = 0.0f, clickEnv = 0.0f, sweepEnv = 0.0f;
    float noiseLow = 0.0f, strength = 0.0f;
    uint32_t seed = 0x12345678u;
    int type = 0;
    static constexpr double twoPi = 6.2831853071795864769;
    void prepare(double sr) noexcept { sampleRate = std::max(8000.0, sr); reset(); }
    void reset() noexcept {
        bodyPhase = partialPhase = 0.0; bodyEnv = noiseEnv = clickEnv = sweepEnv = noiseLow = strength = 0.0f;
    }
    void trigger(float velocity, int voice) noexcept {
        type = voice; strength = std::clamp(velocity, 0.0f, 1.0f);
        bodyEnv = noiseEnv = clickEnv = sweepEnv = 1.0f;
        bodyPhase = partialPhase = noiseLow = 0.0;
        seed ^= 0x9e3779b9u + uint32_t(voice * 131 + 1);
        if(seed == 0) seed = 0xa341316cu;
    }
    float random() noexcept {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        return float(seed >> 8) * (2.0f / 16777215.0f) - 1.0f;
    }
    float next(float tune, float decay, float tone, float character) noexcept {
        if(bodyEnv < 1.0e-6f && noiseEnv < 1.0e-6f && clickEnv < 1.0e-6f)
            return 0.0f;
        tune = std::clamp(tune, 0.0f, 1.0f);
        decay = std::clamp(decay, 0.0f, 1.0f);
        tone = std::clamp(tone, 0.0f, 1.0f);
        character = std::clamp(character, 0.0f, 1.0f);
        const bool kick = type == 0;
        const bool snare = type == 1 || type == 4;
        const bool hat = type == 3 || type == 7;
        const bool tom = type == 2 || type == 5;
        const float base = kick ? 47.0f : snare ? 180.0f : hat ? 520.0f
                          : tom ? 105.0f : 230.0f;
        const float frequency = base * std::exp2((tune - 0.5f) * 2.0f);
        const float pitchSweep = (kick ? 2.6f : tom ? 1.3f : 0.25f) * sweepEnv;
        const double mainFrequency = std::min(sampleRate * 0.40, double(frequency) * (1.0 + pitchSweep));
        bodyPhase += twoPi * mainFrequency / sampleRate;
        if(bodyPhase >= twoPi) bodyPhase -= twoPi;
        const double partialRatio = snare ? 1.49 : hat ? 2.83 : tom ? 1.71 : 2.07;
        partialPhase += twoPi * std::min(sampleRate * .40, double(frequency) * partialRatio) / sampleRate;
        if(partialPhase >= twoPi) partialPhase -= twoPi;
        const float s1 = float(std::sin(bodyPhase));
        const float s2 = float(std::sin(partialPhase));
        const float noise = random();
        const float noiseCoef = (0.04f + tone * 0.60f);
        noiseLow += noiseCoef * (noise - noiseLow);
        const float coloured = hat ? (noise - noiseLow) : noiseLow;
        const float body = s1 * (0.80f - character * 0.25f)
                         + s2 * (0.08f + character * 0.32f);
        const float noiseLevel = kick ? 0.035f : snare ? 0.75f : hat ? 0.95f : tom ? 0.12f : 0.35f;
        const float bodyLevel = hat ? 0.25f : snare ? 0.55f : 0.95f;
        const float click = (noise * 0.55f + s2 * 0.45f) * clickEnv * (0.05f + tone * 0.30f);
        const float result = body * bodyEnv * bodyLevel
                           + coloured * noiseEnv * noiseLevel + click;
        const float seconds = 0.045f + decay * decay * (kick ? 1.6f : hat ? .65f : 1.2f);
        const float bodyFall = float(std::exp(-1.0 / (sampleRate * seconds)));
        const float noiseFall = float(std::exp(-1.0 / (sampleRate * (0.012f + decay * (hat ? .28f : .35f)))));
        bodyEnv *= bodyFall;
        noiseEnv *= noiseFall;
        clickEnv *= float(std::exp(-1.0 / (sampleRate * .0025)));
        sweepEnv *= float(std::exp(-1.0 / (sampleRate * (kick ? .035 : .020))));
        return std::tanh(result * strength * 0.92f);
    }
};
} // namespace jerzy_drum
