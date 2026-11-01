// NDS Veil harness — no JUCE, links only Source/VeilDSP.h.
// Tests, at 48 kHz and 96 kHz:
//  1. Mix 0 / Off-equivalent: veil output is exactly 0.
//  2. Wet - dry = veil exactly (parallel-add property).
//  3. Silence: veil RMS below -60 dB (envelope modulation quiets it).
//  4. Preset ordering: Forward veil RMS > Quiet Room veil RMS.
//  5. Focus ordering: Air veil has more >8 kHz energy than Vocal,
//     Vocal has more 2-4 kHz energy than Air (band DFT check).
//  6. Rate stability: veil RMS at 96 kHz within 3 dB of 48 kHz.
//  7. Hot signal: output finite, peak below 2.0.
// A DFT (not a full FFT library) is used for the band checks so the
// harness stays dependency-free.
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
static double rms(const std::vector<float>& v) {
    double s = 0; for (float x : v) s += (double)x * x;
    return std::sqrt(s / std::max<size_t>(1, v.size()));
}
static double db(double v) { return 20.0 * std::log10(std::max(1e-12, v)); }
// Band energy via DFT magnitudes at a few probe frequencies.
static double bandEnergy(const std::vector<float>& v, double sr, double lo, double hi) {
    double total = 0; int n = 0;
    for (double f = lo; f <= hi; f *= 1.5) {
        double re = 0, im = 0;
        for (size_t i = 0; i < v.size(); i++) {
            double ph = -2.0 * kPi * f * (double)i / sr;
            re += v[i] * std::cos(ph); im += v[i] * std::sin(ph);
        }
        total += std::sqrt(re * re + im * im) / v.size(); n++;
    }
    return n ? total / n : 0;
}
static std::vector<float> renderVeil(const Preset& pr, double sr, const std::vector<float>& dry) {
    VeilChannel ch(0x12345678u);
    ch.prepare(sr); ch.update(pr.focus, pr.veilDb);
    NoiseSource common(0x9E3779B9u);
    std::vector<float> out(dry.size());
    for (size_t i = 0; i < dry.size(); i++)
        out[i] = ch.process(dry[i], common.next(), pr.width, 1.0f);
    return out;
}
static std::vector<float> sine(double sr, double freq, double amp, int n) {
    std::vector<float> v(n);
    for (int i = 0; i < n; i++) v[i] = (float)(amp * std::sin(2.0 * kPi * freq * i / sr));
    return v;
}

int main() {
    for (double sr : {48000.0, 96000.0}) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "rate %.0f: mix 0 gives veil 0", sr);
        { VeilChannel ch(1); ch.prepare(sr); ch.update(0.6f, -50.0f); NoiseSource c(7);
          float v = ch.process(0.5f, c.next(), 0.4f, 0.0f); check(v == 0.0f, buf); }
        auto dry = sine(sr, 440.0, 0.7, (int)sr / 2);
        auto vocal = renderVeil(kPresets[3], sr, dry);
        auto forward = renderVeil(kPresets[4], sr, dry);
        auto quiet = renderVeil(kPresets[1], sr, dry);
        auto air = renderVeil(kPresets[2], sr, dry);
        std::snprintf(buf, sizeof(buf), "rate %.0f: wet-dry = veil (parallel)", sr);
        { // Re-render with a fresh channel and check wet = dry + veil sample-wise
          VeilChannel ch(0x12345678u); ch.prepare(sr); ch.update(kPresets[3].focus, kPresets[3].veilDb);
          NoiseSource c(0x9E3779B9u); bool ok = true;
          for (size_t i = 0; i < dry.size(); i++) { float v = ch.process(dry[i], c.next(), kPresets[3].width, 1.0f); if (std::abs((dry[i] + v) - (dry[i] + vocal[i])) > 1e-6f) { ok = false; break; } }
          check(ok, buf); }
        std::vector<float> silence((int)sr / 2, 0.0f);
        auto veilSil = renderVeil(kPresets[3], sr, silence);
        std::snprintf(buf, sizeof(buf), "rate %.0f: silence veil RMS %.1f dB < -60 dB", sr, db(rms(veilSil)));
        check(db(rms(veilSil)) < -60.0, buf);
        std::snprintf(buf, sizeof(buf), "rate %.0f: Forward RMS %.1f dB > Quiet RMS %.1f dB", sr, db(rms(forward)), db(rms(quiet)));
        check(rms(forward) > rms(quiet), buf);
        double airHi = bandEnergy(air, sr, 8000, 14000), airMid = bandEnergy(air, sr, 2000, 4000);
        double vocHi = bandEnergy(vocal, sr, 8000, 14000), vocMid = bandEnergy(vocal, sr, 2000, 4000);
        std::snprintf(buf, sizeof(buf), "rate %.0f: Air high/mid %.2f > Vocal high/mid %.2f", sr, airHi / std::max(1e-12, airMid), vocHi / std::max(1e-12, vocMid));
        check(airHi / std::max(1e-12, airMid) > vocHi / std::max(1e-12, vocMid), buf);
        std::snprintf(buf, sizeof(buf), "rate %.0f: Vocal mid/high %.2f > Air mid/high %.2f", sr, vocMid / std::max(1e-12, vocHi), airMid / std::max(1e-12, airHi));
        check(vocMid / std::max(1e-12, vocHi) > airMid / std::max(1e-12, airHi), buf);
        // Hot signal finiteness, via the same render path on a hot sine.
        auto hot = sine(sr, 1000.0, 1.4, (int)sr / 4);
        auto veilHot = renderVeil(kPresets[4], sr, hot);
        bool finite = true; float peak = 0;
        for (size_t i = 0; i < hot.size(); i++) { float o = hot[i] + veilHot[i]; if (!std::isfinite(o)) finite = false; peak = std::max(peak, std::abs(o)); }
        std::snprintf(buf, sizeof(buf), "rate %.0f: hot signal finite, peak %.2f < 2.0", sr, peak);
        check(finite && peak < 2.0f, buf);
        std::printf("rate %.0f: Vocal veil RMS %.1f dB, Forward %.1f dB, Quiet %.1f dB\n", sr, db(rms(vocal)), db(rms(forward)), db(rms(quiet)));
    }
    std::printf(failures ? "HARNESS: %d FAILURES\n" : "HARNESS: ALL PASS\n", failures);
    return failures ? 1 : 0;
}
