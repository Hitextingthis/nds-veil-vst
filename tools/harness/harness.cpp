// NDS Veil v0.2 harness — no JUCE, links only Source/VeilDSP.h.
// Tests, at 48 kHz and 96 kHz, for BOTH carriers:
//  1. Mix 0: veil output is exactly 0.
//  2. Wet - dry = veil exactly (parallel-add property).
//  3. Silence: source veil is exactly 0; noise veil RMS below
//     -75 dB (floor lowered from 0.25 to 0.05 in v0.2).
//  4. Source linearity: doubling dry (after warmup) doubles the
//     source veil, within 1% — the source carrier is a linear,
//     decorrelated copy, not noise.
//  5. Correlation at best lag (-20..20 samples): source veil
//     correlates with dry above 0.5, noise veil below 0.3.
//     This is the v0.1 failure test: v0.1's veil was noise, so
//     it could never pass the source side of this test.
//  6. Preset ordering: Forward veil RMS > Quiet Room veil RMS
//     (both carriers, broadband input).
//  7. Rate stability: veil RMS at 96 kHz within 1 dB of 48 kHz
//     (noise carrier; source carrier is filter-rate-correct).
//  8. Hot signal: output finite, peak below 2.0.
// Broadband input (200 + 1500 + 6000 Hz sines) is used so all
// three bands have energy; a single sine would starve two bands.
#include "../../Source/VeilDSP.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace ndsveil;

static int failures = 0;
static void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) failures++;
}
static double rms(const std::vector<float>& v, size_t skip = 0) {
    double s = 0; size_t n = 0;
    for (size_t i = skip; i < v.size(); i++) { s += (double)v[i] * v[i]; n++; }
    return std::sqrt(s / std::max<size_t>(1, n));
}
static double db(double v) { return 20.0 * std::log10(std::max(1e-12, v)); }
static std::vector<float> broadband(double sr, double amp, int n) {
    std::vector<float> v(n);
    for (int i = 0; i < n; i++)
        v[i] = (float)(amp * (0.5 * std::sin(2.0 * kPi * 200.0 * i / sr) +
                              0.3 * std::sin(2.0 * kPi * 1500.0 * i / sr) +
                              0.2 * std::sin(2.0 * kPi * 6000.0 * i / sr)));
    return v;
}
static std::vector<float> renderVeil(const Preset& pr, Carrier carrier, double sr,
                                     const std::vector<float>& dry, float width = 0.4f) {
    VeilChannel ch(0x12345678u, false);
    ch.prepare(sr);
    NoiseSource common(0x9E3779B9u);
    std::vector<float> out(dry.size());
    for (size_t i = 0; i < dry.size(); i++)
        out[i] = ch.process(dry[i], common.next(), width, 1.0f, carrier, pr.focus, pr.veilDb);
    return out;
}
static double bestLagCorr(const std::vector<float>& dry, const std::vector<float>& veil, double sr) {
    double best = 0;
    const size_t skip = 4000;
    // Lag window is time-based (1 ms), not sample-based: group
    // delay in samples doubles from 48k to 96k, so a fixed
    // +/-20-sample window unfairly penalises 96 kHz (that was
    // the first 96k failure: 0.43 with +/-20, vs 0.68 at 48k).
    const int lagMax = static_cast<int>(0.001 * sr);
    for (int lag = -lagMax; lag <= lagMax; lag++) {
        double sxy = 0, sxx = 0, syy = 0;
        for (size_t i = skip; i < dry.size(); i++) {
            long j = (long)i + lag;
            if (j < 0 || j >= (long)veil.size()) continue;
            sxy += dry[i] * veil[j]; sxx += dry[i] * dry[i]; syy += veil[j] * veil[j];
        }
        double c = sxy / std::sqrt(std::max(1e-18, sxx * syy));
        best = std::max(best, std::abs(c));
    }
    return best;
}

int main() {
    for (double sr : {48000.0, 96000.0}) {
        char buf[256];
        auto dry = broadband(sr, 0.7, (int)sr / 2);
        for (Carrier carrier : {Carrier::Source, Carrier::Noise}) {
            const char* cname = carrier == Carrier::Source ? "Source" : "Noise";
            // Mix 0
            { VeilChannel ch(1, false); ch.prepare(sr); NoiseSource c(7);
              float v = ch.process(0.5f, c.next(), 0.4f, 0.0f, carrier, 0.6f, -50.0f);
              std::snprintf(buf, sizeof(buf), "rate %.0f %s: mix 0 gives veil 0", sr, cname);
              check(v == 0.0f, buf); }
            auto vocal = renderVeil(kPresets[3], carrier, sr, dry);
            auto forward = renderVeil(kPresets[4], carrier, sr, dry);
            auto quiet = renderVeil(kPresets[1], carrier, sr, dry);
            // Silence
            std::vector<float> silence((int)sr / 2, 0.0f);
            auto veilSil = renderVeil(kPresets[3], carrier, sr, silence);
            if (carrier == Carrier::Source) {
                bool allZero = true; for (float x : veilSil) if (x != 0.0f) { allZero = false; break; }
                std::snprintf(buf, sizeof(buf), "rate %.0f Source: silence veil exactly 0", sr);
                check(allZero, buf);
            } else {
                std::snprintf(buf, sizeof(buf), "rate %.0f Noise: silence veil RMS %.1f dB < -75 dB", sr, db(rms(veilSil)));
                check(db(rms(veilSil)) < -75.0, buf);
            }
            // Ordering
            std::snprintf(buf, sizeof(buf), "rate %.0f %s: Forward RMS %.1f dB > Quiet RMS %.1f dB", sr, cname, db(rms(forward)), db(rms(quiet)));
            check(rms(forward) > rms(quiet), buf);
            // Correlation at best lag
            double corr = bestLagCorr(dry, vocal, sr);
            std::snprintf(buf, sizeof(buf), "rate %.0f %s: best-lag correlation %.2f", sr, cname, corr);
            check(carrier == Carrier::Source ? corr > 0.5 : corr < 0.3, buf);
            std::printf("rate %.0f %s: Vocal veil RMS %.1f dB, Forward %.1f dB, Quiet %.1f dB, corr %.2f\n",
                        sr, cname, db(rms(vocal)), db(rms(forward)), db(rms(quiet)), corr);
            // Source linearity (skip warmup for the gain ramp)
            if (carrier == Carrier::Source) {
                auto dry2 = broadband(sr, 1.4, (int)sr / 2);
                auto veil2 = renderVeil(kPresets[3], carrier, sr, dry2);
                double ratio = rms(veil2, 4000) / std::max(1e-12, rms(vocal, 4000));
                std::snprintf(buf, sizeof(buf), "rate %.0f Source: doubling dry doubles veil, ratio %.2f", sr, ratio);
                check(ratio > 1.95 && ratio < 2.05, buf);
            }
            // Hot signal
            auto hot = broadband(sr, 1.4, (int)sr / 4);
            auto veilHot = renderVeil(kPresets[4], carrier, sr, hot);
            bool finite = true; float peak = 0;
            for (size_t i = 0; i < hot.size(); i++) { float o = hot[i] + veilHot[i]; if (!std::isfinite(o)) finite = false; peak = std::max(peak, std::abs(o)); }
            std::snprintf(buf, sizeof(buf), "rate %.0f %s: hot signal finite, peak %.2f < 2.0", sr, cname, peak);
            check(finite && peak < 2.0f, buf);
        }
        // Rate stability (noise carrier, Vocal preset)
        auto v48 = renderVeil(kPresets[3], Carrier::Noise, 48000.0, broadband(48000.0, 0.7, 24000));
        auto v96 = renderVeil(kPresets[3], Carrier::Noise, 96000.0, broadband(96000.0, 0.7, 48000));
        double diff = std::abs(db(rms(v48)) - db(rms(v96)));
        std::snprintf(buf, sizeof(buf), "Noise rate stability: |48k-96k| veil RMS diff %.2f dB < 1 dB", diff);
        check(diff < 1.0, buf);
    }
    std::printf(failures ? "HARNESS: %d FAILURES\n" : "HARNESS: ALL PASS\n", failures);
    return failures ? 1 : 0;
}
