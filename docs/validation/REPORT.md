# Validation detail

EMBERFOX 1.3.0 · Windows x64 · 2026-10-27

The tested build uses MSVC 19.50.35739, Windows SDK 10.0.26100.0 and JUCE 8.0.16-security, commit `ea77444750de45e036e0279bb0291144bf8e3688`. See [release results](RELEASE-1.3.0.md) for the checklist and open release gates.

## Automated coverage

Six CTest suites passed: VisualState, Animation, DSP, ReleaseFeatures, actual VST3 host and native UI. pluginval 1.0.4 passed strictness level 10 with seed 73821 and a 120000 ms timeout. The official Steinberg validator from SDK v3.8.1_build_84 passed 47 applicable tests, both directly and through pluginval.

- Silence, high input, finite output and DC rejection.
- Mono/stereo at 44.1, 48, 96 and 192 kHz; blocks 0, 1, 17, 64, 511 and 4096 samples.
- Parameter/mode transitions, plugin/host bypass, latency alignment and repeated preparation.
- Full parameter save/restore, malformed-state rejection, legacy state, original defaults and all eight factory presets.
- Numeric editing, Enter/Escape, double-click reset, host-to-editor updates, mode buttons, bypass, factory menu and offline Help.
- Peak timing/release, actual meter scale, independent input/output animation, transitions and symmetrical geometry.

The actual VST3 is loaded in a separate host. Native tests separately exercise the real processor/editor classes; VST3 wrapper editor checks cover creation, resize and destruction. Native control tests use framework handlers on an off-screen peer; physical operating-system mouse routing was not exercised.

## DSP tolerances

- Reported integer latency: 61 samples; dry and bypass paths use matching delay.
- Silent right-channel output remains below 1e-7 when only the left channel is driven.
- Repeated control/mode/bypass changes on the specified 100 Hz test remain finite, with largest adjacent-sample jump below 0.15. This is a scoped regression, not an inaudibility guarantee for arbitrary audio.
- Settled Fuzz output mean on sustained DC remains below 1e-4.
- C++ new/new[] instrumentation counts zero allocations in prepared engine processing and the native processor's first regular/bypass callbacks, including a 4096-sample block against a 128-sample prepare hint. It does not intercept every C-runtime allocator or every wrapper/host thread.
- Input/output meter readings match dry peaks of 0.5 through host bypass and 1.25 through parameter bypass within 1e-6.

The VST3 test host announces a maximum block size of 4096 before submitting its block-size cases. Native engine/processor tests also exceed their preparation hint. Hosts that violate VST3's announced hard maximum remain outside the supported wrapper contract. Float64 processing is not advertised.

## Scoped aliasing measurement

Razor, 24 dB Drive, 0 dB wet Output, 20 kHz Tone, 100% Mix, 48 kHz sample rate: compare the complete 4x-oversampled engine against naive base-rate hard clipping. The source is a coherent 9 kHz sine with 0.5 peak amplitude. Discard 8192 settling samples and analyze 32768 samples with a rectangular FFT window; exclude DC and fundamental bin 6144 ±2. Normalize residual energy to fundamental power for each signal.

Measured reduction: **24.9349 dB**, exceeding the 10 dB regression threshold. At this frequency no higher hard-clip harmonic is below Nyquist, so residual energy mainly measures folded harmonics plus filter/numerical residuals. This includes the complete tone/DC path; it does not isolate the oversampler or establish an all-settings aliasing specification.

## Interface and installation

The editor was rendered at 770×503, 980×640 and 1470×960. Captures include all five steady/driven expressions, minimum-size transients, bypass, editing, over-zero meters and Help. Animation/drawing run on the message thread. Peak markers hold for one second and release at 18 dB/s using elapsed time, with calibrated −60 to +6 dBFS bars.

Actual setup was tested with redirected application/VST3 destinations and current-user registration. Tests covered upgrading retained 1.2.1 manual binaries, reinstall, exact installed-file hashes, standalone GUI startup, installed-plugin host checks and uninstall. User-created files, unrelated plugin-directory contents and the existing shared Microsoft runtime were preserved. Previous local release ZIPs and the reference plugin remained unchanged.

## Evidence and limitations

Downloadable source/portable packages include current test logs and captures; local filesystem paths are redacted for publication without changing numerical results. Final artifact hashes are separate release assets. Synthetic WAVs in this directory are deterministic functional examples, not musical presets or loudness-matched listening comparisons.

The plugin and standalone match the validated binaries. Documentation/repository cleanup changes neither their audio processing nor their saved-state/automation contracts. The Windows resource dependency is tracked so incremental builds update product version/publisher consistently.

Pending checks include fresh-machine runtime installation/elevation cancellation, all-users setup, trusted signing, actual DAW/audio-device audition, longer soak and broader machine testing. The offline HTML guide's source/relative links were checked; a rendered browser preview was not performed. Follow [the clean-Windows procedure](../clean-windows-test.md).

Passing automated validation does not imply Steinberg certification, universal compatibility or exact sonic equivalence to another effect. The downloads remain explicitly unsigned previews.

## Tools

[pluginval guidance](https://github.com/Tracktion/pluginval/blob/develop/README.md), [Steinberg validator](https://steinbergmedia.github.io/vst3_dev_portal/pages/What%2Bis%2Bthe%2BVST%2B3%2BSDK/Validator.html), [Microsoft runtime](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist), [Inno Setup](https://jrsoftware.org/isinfo.php).
