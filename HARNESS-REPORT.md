# NDS Veil — Harness Report

Harness: `tools/harness/harness.cpp` (no JUCE, links only
`Source/VeilDSP.h`). Run: `g++ -std=c++17 -O2 -I Source
tools/harness/harness.cpp -o /tmp/veil-harness && /tmp/veil-harness`

## Result: ALL PASS at 48 kHz and 96 kHz

48 kHz:
- Mix 0 gives veil 0: PASS
- Wet − dry = veil exactly (parallel-add): PASS
- Silence veil RMS −70.0 dB (< −60 dB): PASS
- Forward RMS −52.0 dB > Quiet Room RMS −66.0 dB: PASS
- Air high/mid 4.10 > Vocal high/mid 2.48: PASS
- Vocal mid/high 0.40 > Air mid/high 0.24: PASS
- Hot signal finite, peak 1.41 < 2.0: PASS
- Vocal veil RMS −58.3 dB

96 kHz (after rate compensation):
- Mix 0 gives veil 0: PASS
- Wet − dry = veil exactly: PASS
- Silence veil RMS −69.8 dB (< −60 dB): PASS
- Forward RMS −51.8 dB > Quiet Room RMS −65.8 dB: PASS
- Air high/mid 1.49 > Vocal high/mid 1.28: PASS
- Vocal mid/high 0.78 > Air mid/high 0.67: PASS
- Hot signal finite, peak 1.41 < 2.0: PASS
- Vocal veil RMS −58.1 dB (within 0.2 dB of 48 kHz)

## Rate-compensation note (found by the harness, fixed)

Before compensation, the 96 kHz veil measured ~2.8 dB quieter
than at 48 kHz (Vocal −61.1 dB vs −58.3 dB). Cause: white noise
through a fixed 18 kHz low-pass occupies a smaller fraction of
Nyquist as sample rate rises. Fix: veil gain scaled by
sqrt(rate/48k). After the fix, 48/96 kHz veil RMS matches within
0.2 dB for all presets tested. This is why the harness reports
both rates, not just one.
