# NDS Veil — Harness Report

Harness: `tools/harness/harness.cpp` (no JUCE, links only
`Source/VeilDSP.h`).

## v0.2 — ALL PASS at 48 kHz and 96 kHz

Broadband input (200 + 1500 + 6000 Hz) so all three bands have
energy. Both carriers tested:

- Mix 0 gives veil 0: PASS (both carriers, both rates)
- Silence: Source veil exactly 0; Noise veil RMS −94.2 dB @48k /
  −92.8 dB @96k (below −75 dB; floor lowered from 0.25 to 0.05):
  PASS
- Forward RMS > Quiet Room RMS: PASS
  - Source: Forward −53.9 dB > Quiet −67.8 dB (both rates)
  - Noise: Forward −72.0 dB @48k / −70.4 dB @96k > Quiet
    −86.1 dB @48k / −85.9 dB @96k
- Best-lag correlation (1 ms lag window, time-based so 96 kHz
  is not penalised for its larger sample group delay):
  - Source: 0.99 @48k and @96k (> 0.5) — the veil IS the source
  - Noise: 0.01 @48k, 0.02 @96k (< 0.3) — the veil is NOT the source
  - This is the v0.1 failure test, in reverse: v0.1's veil
    could only ever score like the Noise row.
- Source linearity: doubling dry doubles the source veil,
  ratio 2.00 (both rates): PASS
- Hot signal finite, peak 1.37–1.38 < 2.0: PASS
- Noise rate stability: |48k − 96k| veil RMS diff 0.66 dB < 1 dB

Source veil RMS at defaults (Vocal Focus, broadband input):
−59.9 dB, vs v0.1 noise veil −58.3 dB — comparable level,
now with 0.99 source correlation instead of ~0.01.

### The 96 kHz correlation fix, found by the harness

First v0.2 run: 96 kHz Source correlation was 0.43, failing
the > 0.5 gate, while 48 kHz was 0.68. Cause was the test, not
the DSP: the lag window was fixed at ±20 samples, but group
delay in samples doubles from 48k to 96k. Changing the window
to a fixed 1 ms (±48 samples @48k, ±96 @96k) gave 0.99 at both
rates. The DSP was not changed for this fix.

## v0.1 — ALL PASS at 48 kHz and 96 kHz (superseded)

- Mix 0 = veil 0, wet−dry = veil exactly, silence veil RMS
  −70.0 dB @48k / −69.8 dB @96k (below −60 dB), Forward RMS >
  Quiet Room RMS, Air high/mid ratio > Vocal high/mid ratio
  and Vocal mid/high ratio > Air mid/high ratio, hot signal
  finite with peak 1.41. Vocal veil RMS −58.3 dB.
- Rate-compensation note: before compensation, the 96 kHz
  veil measured ~2.8 dB quieter than at 48 kHz (white noise
  through a fixed 18 kHz low-pass). Fix: veil gain scaled by
  sqrt(rate/48k). After the fix, 48/96 kHz veil RMS matched
  within 0.2 dB. That compensation is retained in v0.2 for
  the Noise carrier only; the Source carrier needs none
  (rate-correct filters, time-based delay).
