# NDS Veil v0.1 — preset psychoacoustic enhancer for mastering

A clean-room, preset-based psychoacoustic enhancer, built for
mastering. Original DSP and original UI (NDS Field System v1).
It is inspired only by the general idea of a low-level,
correlated, spectrally shaped addition on top of untouched audio
— it makes **no claim to match any commercial unit's exact
signal, curves, or sound**. No disassembly, no proprietary code,
no copied artwork, branding, presets, or names were used.

Company: NDS · Plugin code: NDVe · Product name: NDS Veil

## Signal chain (per channel, parallel)

```
IN (dry, untouched)
  ├─ envelope follower (|dry|, 5 ms attack / 100 ms release)
  └─ veil: white noise (xorshift, own + shared for width)
           → HP 150 Hz → peaking at Focus freq → LP 18 kHz
           → envelope modulation (0.25 + 0.75 * env, capped)
           → veil gain (dB) × rate compensation sqrt(rate/48k)
           → Mix
OUT = (dry + veil) × Trim, then Monitor (LR / Mono / Delta)
```

- **Dry is never EQ'd, compressed, or limited here.** Bypass
  and Mix 0 return dry exactly, overriding monitor modes.
- **Veil level is in dB** (−60 to −40), not 0–100, so the added
  signal is stated honestly. Measured veil RMS at defaults is
  lower than the nominal dB (filtering + envelope modulation):
  Vocal Focus measures about −58 dB RMS on a 0.7 sine, Forward
  about −52 dB, Quiet Room about −66 dB (harness, 48 kHz).
- **Rate compensation:** white noise through a fixed 18 kHz
  low-pass loses ~3 dB going 48 kHz → 96 kHz. The veil gain is
  scaled by sqrt(rate/48k) so the veil RMS matches within 0.2 dB
  across 48/96 kHz in the harness. Without it, 96 kHz measured
  2.8 dB quieter than 48 kHz — found and fixed before delivery.
- **Stereo width:** width 0 = fully shared noise, width 1 =
  fully independent per-channel noise, blended per frame.
- **No oversampling in v0.1.** The veil is filtered noise, not a
  waveshaper, so aliasing is not the concern it is in an exciter.
  Filters are sample-rate-correct RBJ biquads (minimum-phase IIR).
- **No parameter smoothing beyond a 10 ms veil-gain ramp.**
  Abrupt automation moves on Veil/Focus may click slightly;
  set presets before tracking, as with the exciter.
- **Host latency:** none reported, none added (no oversampler,
  no delay line). The veil is generated sample-by-sample in
  parallel with dry.

## Parameters (all via APVTS)

| Parameter | Range | Default |
|---|---|---|
| Veil Level | −60 to −40 dB | −50 dB (Vocal Focus) |
| Focus | 0–1 (500–12000 Hz log, peaking +0–6 dB) | 0.60 |
| Width | 0–1 | 0.40 |
| Mix | 0–100% | 100% |
| Output Trim | −6 to +6 dB | 0 dB |
| On | on/off | On |
| Monitor | LR / Mono / Delta | LR |
| Destination Target | Streaming −14 / Loud −9 / Dynamic −16 / Custom | Loud −9 |
| True Peak Ceiling | −2.0 to 0 dBTP | −1.0 dBTP |

Preset buttons in the editor are shortcuts, not parameters:
they set Veil / Focus / Width / Mix to the preset's values.
Session recall therefore restores those values. Presets:

| Preset | Job | Veil | Focus | Width |
|---|---|---|---|---|
| Gentle | Streaming master | −52 dB | 0.45 | 0.30 |
| Quiet Room | Dynamic master | −58 dB | 0.35 | 0.20 |
| Air | Wide streaming master | −48 dB | 0.85 | 0.80 |
| Vocal Focus | Loud vocal-forward master | −50 dB | 0.60 | 0.40 |
| Forward | Loud label master | −44 dB | 0.65 | 0.60 |

## Mastering use (read before sending a master)

- **Place BEFORE the final clipper/limiter.** The veil adds
  peak energy; after Pro-L 2 it can break the −1.0 dBTP ceiling.
  Before it, the limiter absorbs the veil's peaks.
- **Ceiling and Target are meter references only.** They never
  limit or change loudness. If OUT peak exceeds the ceiling,
  the editor flags "OVER CEILING" — that is a prompt to trim
  or re-check, not an auto-fix.
- **Meters in the editor are peak/RMS only** (IN, OUT, VEIL ×40
  so the tiny veil is visible). They are NOT LUFS/TP measurement.
  Verify a real master in RX (ceiling pass to −1.0 dBTP) and
  StreamCheck / phone speaker before sending, as always.
- **Mono check** collapses the full output (veil included) to
  mono. **Delta** is veil only — the proof control: wet minus
  dry must equal the veil, exactly, in the harness and by ear.
- This plugin does no EQ, compression, limiting, clipping, or
  dither, and makes no brain/neuroscience claims. Its claim is
  modest and measurable: a low-level, correlated, spectrally
  shaped addition at a stated dB level.

## Verification (local Linux proof only — see below)

* Harness (`tools/harness/harness.cpp`, no JUCE, links only
  `Source/VeilDSP.h`): **ALL PASS at 48 kHz and 96 kHz** —
  Mix 0 = veil 0, wet−dry = veil exactly, silence veil RMS
  −70.0 dB @48k / −69.8 dB @96k (below −60 dB), Forward RMS >
  Quiet Room RMS, Air high/mid ratio > Vocal high/mid ratio
  and Vocal mid/high ratio > Air mid/high ratio, hot signal
  finite with peak 1.41. Full log: `HARNESS-REPORT.md`.
* Linux VST3 + Standalone build: see build log in delivery.
* pluginval strictness 5, in-process, under Xvfb: see
  `pluginval-log.txt` in delivery.
* GUI screenshot, standalone under Xvfb, eyeballed:
  `ui-nds-veil-1280.png`.
* Bus guard: `processBlock` checks bus channel counts and
  never writes into a 0-channel bus; extra output channels are
  silenced. Mono and stereo layouts supported.
* Platform-constant guard: shipped headers define their own pi
  constant (`kPi`) and contain no `M_PI`.

## What is NOT proven (read before calling this done)

* **No Windows binary exists yet.** Linux VST3 + Standalone
  and pluginval are local proof only. They are not Windows delivery.
* **No FL Studio test.** Loading, routing, automation, and
  preset save/recall in FL Studio on Windows are untested.
* **No listening verdict.** The harness proves parallel-add
  exactness, veil levels, focus ordering, rate stability, and
  no blow-up. It does not prove the veil sounds good on a vocal,
  a mix, or a master. Ears in FL are the final gate.
* **No LUFS/TP measurement in the plugin.** The editor meters
  are peaks/RMS. Do not send a master on the basis of this
  plugin's meters alone.

## Installing the Windows build (once it exists)

1. Copy `NDS Veil.vst3` to
   `C:\Program Files\Common Files\VST3\`
2. In FL Studio: Options → Manage plugins → Find plugins (or
   rescan), then load "NDS Veil" from the plugin list.
3. If it does not appear, check the plugin manager's error list,
   confirm the file is the Windows VST3 (not the Linux one in
   this repo's `build/` folder), and rescan once more.
4. Place it BEFORE your final clipper/limiter in the chain.
   Chrome will flag the fresh unsigned Windows binary as a
   dangerous download — that is normal Safe Browsing behaviour
   for an unsigned first release, not a problem with the build;
   choose Keep to download it.

## Building locally (Linux proof build)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
# VST3: build/NDSVeil_artefacts/Release/VST3/NDS Veil.vst3
# Standalone: build/NDSVeil_artefacts/Release/Standalone/NDS Veil
# Harness: g++ -std=c++17 -O2 -I Source tools/harness/harness.cpp -o /tmp/veil-harness && /tmp/veil-harness
```

JUCE is expected at `../tools/JUCE` relative to this project
(override with `-DJUCE_DIR=/path/to/JUCE`). Never cross-compile
for Windows with MinGW; the Windows binary comes from GitHub
Actions (see below).

## Owner steps still needed (not done in this local build)

1. User creates an empty private GitHub repo (no README,
   .gitignore, or license) and shares its owner/name.
2. Muse pushes the project files (text files only) to that repo's
   main branch in one commit.
3. User adds the Windows build workflow via the GitHub web UI
   (pinned windows-2022 runner, JUCE CMake build, VST3 artifact).
   Do not use the Muse GitHub App for `.github/workflows/` — it
   returns 403 there; the workflow file goes via the web UI.
4. User downloads the Windows VST3 artifact, installs it per the
   steps above, and tests in FL Studio at 48 kHz: load, presets,
   Veil/Focus/Width, Mono, Delta, Bypass, automation, preset
   save/recall, and a listening verdict on a real master —
   especially: does the veil stay sub-audible on quiet passages,
   and does true peak stay under −1.0 dBTP after the limiter?
