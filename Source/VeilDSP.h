// NDS Veil v0.2 — original psychoacoustic veil DSP, pure C++ (no JUCE).
//
// v0.1 lesson, from the user's FL test: the veil was shaped white
// noise, envelope-modulated only, so in Delta it sounded like
// white noise that blends in. v0.2 fixes the carrier, not the
// level: the default carrier is now the SOURCE itself, and the
// noise carrier is kept as an option with a much lower silence
// floor and per-band envelopes.
//
// Carriers (both parallel, dry untouched):
//   SOURCE (default): the FULL dry signal (not band-limited,
//     so a bass-forward mix does not lose its veil when Focus
//     sits in the mids), passed through a channel-specific
//     first-order allpass + a small time-based delay on the
//     right channel for width, then a peaking filter at the
//     Focus frequency for spectral emphasis, then added at
//     the veil gain. This is a quiet, decorrelated copy of
//     the music itself: it sounds like texture and width, not
//     hiss, because its spectrum and timing follow the source.
//     It is linear: doubling dry doubles the veil, and
//     silence gives veil exactly 0. Focus here is a peaking
//     frequency (500-12000 Hz log, +0-6 dB), as in v0.1 —
//     it emphasises a region, it does not exclude the rest.
//   NOISE: xorshift white noise, split into the same 3 bands,
//     each band scaled by that band's own envelope follower
//     (5 ms attack / 100 ms release) from the dry signal in
//     that band, with a silence floor of 0.05 (was 0.25 in v0.1,
//     which kept hissing in gaps). Focus weights the bands.
//     Noise veil is rate-compensated by sqrt(rate/48k) because
//     white noise through fixed-frequency filters loses energy
//     as sample rate rises; the source carrier needs no such
//     compensation (its filters are rate-correct and its delay
//     is time-based).
//
// Focus (0..1) does two jobs, by carrier: for SOURCE it is
// a peaking frequency (500-12000 Hz log); for NOISE it is a
// triangular band blend (low = max(0, 1-2*focus), mid =
// 1-|2*focus-1|, high = max(0, 2*focus-1)). At focus 0.5 the
// noise veil is all mid; at 0.85 it is 70% high, 30% mid.
// Preset focus values from v0.1 carry over unchanged.
//
// This is NOT FiDef's proprietary signal, code, or curves:
// the band split, allpass decorrelator, envelope followers,
// and preset values below are original, textbook designs.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

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

// First-order allpass, magnitude-flat, phase-shifting. Different
// coefficients per channel give stereo decorrelation without
// changing the spectrum.
struct Allpass {
    double a = 0.4, x1 = 0.0, y1 = 0.0;
    explicit Allpass(double coeff) : a(coeff) {}
    void reset() { x1 = 0.0; y1 = 0.0; }
    float process(float x) {
        const double y = -a * x + x1 + a * y1;
        x1 = x; y1 = y;
        return static_cast<float>(y);
    }
};

struct NoiseSource {
    std::uint32_t state = 0x12345678u;
    explicit NoiseSource(std::uint32_t seed) : state(seed ? seed : 0x12345678u) {}
    float next() {
        std::uint32_t x = state;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        state = x;
        return static_cast<float>(x >> 8) / 8388607.0f - 1.0f;
    }
};

struct Preset {
    const char* name;
    const char* job;
    float veilDb;
    float focus;
    float width;
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

// Triangular band weights for a focus value in [0, 1] (noise carrier).
inline void bandWeights(float focus, float& wLow, float& wMid, float& wHigh) {
    const float f = std::min(1.0f, std::max(0.0f, focus));
    wLow = std::max(0.0f, 1.0f - 2.0f * f);
    wMid = 1.0f - std::abs(2.0f * f - 1.0f);
    wHigh = std::max(0.0f, 2.0f * f - 1.0f);
}

enum class Carrier { Noise = 0, Source = 1 };

class VeilChannel {
public:
    // isRight: right channel gets a small time-based delay and a
    // different allpass coefficient, for width. Left gets none.
    VeilChannel(std::uint32_t seed, bool isRight)
        : ownNoise(seed), allpass(isRight ? -0.4 : 0.4), right(isRight) {}

    void prepare(double sampleRate) {
        rate = sampleRate;
        dryLow.setLowPass(500.0, rate);
        dryMidHp.setHighPass(500.0, rate);
        dryMidLp.setLowPass(4000.0, rate);
        dryHigh.setHighPass(4000.0, rate);
        noiseLow.setLowPass(500.0, rate);
        noiseMidHp.setHighPass(500.0, rate);
        noiseMidLp.setLowPass(4000.0, rate);
        noiseHigh.setHighPass(4000.0, rate);
        attackCoeff = 1.0 - std::exp(-1.0 / (0.005 * rate));
        releaseCoeff = 1.0 - std::exp(-1.0 / (0.100 * rate));
        delaySamples = right ? std::max(1, static_cast<int>(std::lround(0.0003 * rate))) : 0;
        delay.assign(1024, 0.0f);
        delayPos = 0;
        reset();
    }
    void reset() {
        dryLow.reset(); dryMidHp.reset(); dryMidLp.reset(); dryHigh.reset();
        noiseLow.reset(); noiseMidHp.reset(); noiseMidLp.reset(); noiseHigh.reset();
        focusPeak.reset();
        allpass.reset();
        envLow = envMid = envHigh = 0.0f;
        smoothGain = 0.0f;
        std::fill(delay.begin(), delay.end(), 0.0f);
        delayPos = 0;
    }

    // Returns the veil sample for this channel. commonNoise is the
    // shared noise sample for this frame (noise carrier only).
    float process(float dry, float commonNoise, float width, float mix,
                  Carrier carrier, float focus, float veilDb) {
        // Band-split dry, and per-band envelopes.
        const float bandDryLow = dryLow.process(dry);
        const float bandDryMid = dryMidLp.process(dryMidHp.process(dry));
        const float bandDryHigh = dryHigh.process(dry);
        auto follow = [&](float& env, float x) {
            const double coeff = (std::abs(x) > env) ? attackCoeff : releaseCoeff;
            env = static_cast<float>(env + coeff * (std::abs(x) - env));
        };
        follow(envLow, bandDryLow);
        follow(envMid, bandDryMid);
        follow(envHigh, bandDryHigh);

        float wLow = 0, wMid = 0, wHigh = 0;
        bandWeights(focus, wLow, wMid, wHigh);

        float veil = 0.0f;
        if (carrier == Carrier::Source) {
            if (focus != lastFocus) {
                lastFocus = focus;
                focusPeak.setPeaking(focusFreqHz(focus), rate,
                    6.0 * std::min(1.0, std::max(0.0, static_cast<double>(focus))), 0.8);
            }
            float decorrelated = allpass.process(dry);
            // Right-channel time delay (0.3 ms) for width. Left is
            // undelayed. Width scales the delay amount: at width 0
            // both channels are identical (mono veil), at width 1
            // the full delay applies.
            if (delaySamples > 0 && width > 0.0f) {
                const int readPos = static_cast<int>((delayPos + delay.size() -
                    static_cast<size_t>(std::lround(width * delaySamples))) % delay.size());
                delay[delayPos] = decorrelated;
                delayPos = (delayPos + 1) % delay.size();
                decorrelated = delay[readPos];
            }
            veil = focusPeak.process(decorrelated);
        } else {
            const float blended = (1.0f - width) * commonNoise + width * ownNoise.next();
            const float bandNoiseLow = noiseLow.process(blended);
            const float bandNoiseMid = noiseMidLp.process(noiseMidHp.process(blended));
            const float bandNoiseHigh = noiseHigh.process(blended);
            // Per-band envelope modulation, silence floor 0.05.
            const float modLow = 0.05f + 0.95f * std::min(1.0f, envLow * 1.5f);
            const float modMid = 0.05f + 0.95f * std::min(1.0f, envMid * 1.5f);
            const float modHigh = 0.05f + 0.95f * std::min(1.0f, envHigh * 1.5f);
            veil = wLow * bandNoiseLow * modLow + wMid * bandNoiseMid * modMid +
                   wHigh * bandNoiseHigh * modHigh;
            // Rate compensation for the noise carrier only.
            veil *= static_cast<float>(std::sqrt(rate / 48000.0));
        }

        const float target = dbToGain(veilDb) * mix;
        const double gCoeff = 1.0 - std::exp(-1.0 / (0.010 * rate));
        smoothGain += static_cast<float>(gCoeff * (target - smoothGain));
        return veil * smoothGain;
    }

private:
    double rate = 48000.0;
    double attackCoeff = 0.01, releaseCoeff = 0.001;
    float envLow = 0.0f, envMid = 0.0f, envHigh = 0.0f;
    float smoothGain = 0.0f;
    Biquad dryLow, dryMidHp, dryMidLp, dryHigh;
    Biquad noiseLow, noiseMidHp, noiseMidLp, noiseHigh;
    Biquad focusPeak;
    float lastFocus = -1.0f;
    Allpass allpass;
    NoiseSource ownNoise;
    bool right = false;
    std::vector<float> delay;
    size_t delayPos = 0;
    int delaySamples = 0;
};

} // namespace ndsveil
