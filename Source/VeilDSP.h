// NDS Veil — original psychoacoustic veil DSP, pure C++ (no JUCE).
// Concept, in general terms only: a very low-level, spectrally
// shaped, source-correlated noise veil added in parallel to an
// untouched dry signal. This is NOT FiDef's proprietary signal,
// code, or curves: the noise source, shaping filters, envelope
// follower, and preset values below are original, textbook designs.
// The dry path is never EQ'd, compressed, or limited here.
//
// Chain per channel:
//   noise (xorshift white) -> HP 150 Hz -> peaking filter at the
//   Focus frequency -> LP 18 kHz -> envelope modulation ->
//   veil gain (dB) -> mix -> added to dry.
// Stereo width blends a shared (common) noise with each channel's
// own noise: width 0 = fully common, width 1 = fully independent.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ndsveil {

inline constexpr double kPi = 3.14159265358979323846;

inline float dbToGain(float db) {
    return static_cast<float>(std::pow(10.0, static_cast<double>(db) / 20.0));
}

// RBJ biquad, minimum-phase IIR, sample-rate correct.
struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
    void reset() { z1 = 0.0; z2 = 0.0; }
    float process(float x) {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return static_cast<float>(y);
    }
    void setHighPass(double freq, double sampleRate, double q = 0.7071) {
        const double w = 2.0 * kPi * freq / sampleRate;
        const double alpha = std::sin(w) / (2.0 * q);
        const double cw = std::cos(w);
        const double a0 = 1.0 + alpha;
        b0 = ((1.0 + cw) / 2.0) / a0; b1 = -(1.0 + cw) / a0; b2 = ((1.0 + cw) / 2.0) / a0;
        a1 = (-2.0 * cw) / a0; a2 = (1.0 - alpha) / a0;
    }
    void setLowPass(double freq, double sampleRate, double q = 0.7071) {
        const double w = 2.0 * kPi * freq / sampleRate;
        const double alpha = std::sin(w) / (2.0 * q);
        const double cw = std::cos(w);
        const double a0 = 1.0 + alpha;
        b0 = ((1.0 - cw) / 2.0) / a0; b1 = (1.0 - cw) / a0; b2 = ((1.0 - cw) / 2.0) / a0;
        a1 = (-2.0 * cw) / a0; a2 = (1.0 - alpha) / a0;
    }
    void setPeaking(double freq, double sampleRate, double gainDb, double q = 0.8) {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * freq / sampleRate;
        const double alpha = std::sin(w) / (2.0 * q);
        const double cw = std::cos(w);
        const double a0 = 1.0 + alpha / A;
        b0 = (1.0 + alpha * A) / a0; b1 = (-2.0 * cw) / a0; b2 = (1.0 - alpha * A) / a0;
        a1 = (-2.0 * cw) / a0; a2 = (1.0 - alpha / A) / a0;
    }
};

// Deterministic xorshift white-noise source (own noise per channel,
// plus a shared source created by the processor for width blending).
struct NoiseSource {
    std::uint32_t state = 0x12345678u;
    explicit NoiseSource(std::uint32_t seed) : state(seed ? seed : 0x12345678u) {}
    float next() {
        std::uint32_t x = state;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        state = x;
        return static_cast<float>(x >> 8) / 8388607.0f - 1.0f; // ~[-1, 1]
    }
};

struct Preset {
    const char* name;
    const char* job;
    float veilDb;   // -60..-40
    float focus;    // 0..1 -> focus freq log 500..12000 Hz
    float width;    // 0..1
};

inline const Preset kPresets[5] = {
    { "Gentle", "Streaming master", -52.0f, 0.45f, 0.30f },
    { "Quiet Room", "Dynamic master", -58.0f, 0.35f, 0.20f },
    { "Air", "Wide streaming master", -48.0f, 0.85f, 0.80f },
    { "Vocal Focus", "Loud vocal-forward master", -50.0f, 0.60f, 0.40f },
    { "Forward", "Loud label master", -44.0f, 0.65f, 0.60f },
};

inline double focusFreqHz(float focus) {
    const double f = std::min(1.0, std::max(0.0, static_cast<double>(focus)));
    return 500.0 * std::pow(12000.0 / 500.0, f);
}

class VeilChannel {
public:
    explicit VeilChannel(std::uint32_t seed) : ownNoise(seed) {}

    void prepare(double sampleRate) {
        rate = sampleRate;
        updateFilters();
        env = 0.0f;
        smoothGain = 0.0f;
    }
    void reset() {
        hp.reset(); peak.reset(); lp.reset(); env = 0.0f;
    }
    // focus 0..1, veilDb -60..-40. Rebuilds filters only on change.
    void update(float focus, float veilDb) {
        if (focus != lastFocus || veilDb != lastVeilDb || rate <= 0.0) {
            lastFocus = focus; lastVeilDb = veilDb;
            updateFilters();
        }
    }
    // Returns the veil sample for this channel. commonNoise is the
    // shared noise sample for this frame; width blends it with the
    // channel's own noise before shaping. mix is 0..1.
    float process(float dry, float commonNoise, float width, float mix) {
        // Envelope follower: fast attack, slow release, on |dry|.
        const float a = std::abs(dry);
        const double coeff = (a > env) ? attackCoeff : releaseCoeff;
        env = static_cast<float>(env + coeff * (a - env));
        const float mod = 0.25f + 0.75f * std::min(1.0f, env * 1.5f);
        const float blended = (1.0f - width) * commonNoise + width * ownNoise.next();
        float v = hp.process(blended);
        v = peak.process(v);
        v = lp.process(v);
        // Rate compensation: white noise through a fixed-frequency
        // low-pass loses energy as sample rate rises (the passband
        // is a smaller fraction of Nyquist). Scaling by
        // sqrt(rate/48k) keeps the veil RMS within ~1 dB across
        // 48/96/192 kHz, verified by the harness. Without it, the
        // 96 kHz veil measured ~2.8 dB quieter than at 48 kHz.
        const float target = dbToGain(lastVeilDb) * mod * mix *
            static_cast<float>(std::sqrt(rate / 48000.0));
        // 10 ms gain smoothing to avoid clicks on parameter moves.
        const double gCoeff = 1.0 - std::exp(-1.0 / (0.010 * rate));
        smoothGain += static_cast<float>(gCoeff * (target - smoothGain));
        return v * smoothGain;
    }

private:
    void updateFilters() {
        if (rate <= 0.0) return;
        const double f = focusFreqHz(lastFocus);
        const double peakGain = 6.0 * std::min(1.0, std::max(0.0, static_cast<double>(lastFocus)));
        hp.setHighPass(150.0, rate);
        peak.setPeaking(f, rate, peakGain, 0.8);
        lp.setLowPass(18000.0, rate);
        // Filter state is kept across parameter changes (no click
        // from a reset); only a fresh prepare/reset clears it.
        attackCoeff = 1.0 - std::exp(-1.0 / (0.005 * rate));
        releaseCoeff = 1.0 - std::exp(-1.0 / (0.100 * rate));
    }
    double rate = 48000.0;
    float lastFocus = 0.6f, lastVeilDb = -50.0f;
    float env = 0.0f, smoothGain = 0.0f;
    double attackCoeff = 0.01, releaseCoeff = 0.001;
    Biquad hp, peak, lp;
    NoiseSource ownNoise;
};

} // namespace ndsveil
