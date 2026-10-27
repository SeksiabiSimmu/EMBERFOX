# EMBERFOX 1.3.0 validation

2026-10-27 · Windows x64 · EmberSoft · AGPL-3.0-only · unsigned preview

## Results

| Check | Result |
|---|---|
| Release build | VST3 and standalone built with MSVC 19.50.35739, Windows SDK 10.0.26100.0 and pinned JUCE 8.0.16-security. |
| Product metadata | Both Windows binaries report file/product version 1.3.0 and publisher EmberSoft. |
| CTest | All six suites passed: VisualState (13 checks), Animation (7), DSP (19), ReleaseFeatures (4), VST3Host and NativeUI. |
| pluginval 1.0.4 | Strictness 10, seed 73821, timeout 120000 ms: SUCCESS, exit 0. |
| Steinberg validator | Official SDK v3.8.1_build_84: 47 passed, 0 failed, exit 0. Direct and pluginval-integrated executions passed. |
| Sound compatibility | DSP implementation and original processor functions unchanged from 1.2.1. Six deterministic actual-VST3 WAV renders are byte-identical. |
| State/automation | Original six parameter IDs, VST3 class IDs and state schema retained. Legacy state and all-preset save/restore passed. |
| Native interface | Numeric editing, Enter/Escape, double-click reset, automation refresh, eight presets, Custom matching and offline Help checks passed. |
| Rendering | Minimum/default/maximum editor, five mode portraits, driven/bypass/meter states and minimum/default Help images captured and inspected. |
| Installation | Isolated current-user upgrade/reinstall, installed standalone startup, installed-VST3 host checks and uninstall passed. Owned files/registration removed; user files and shared runtime preserved. |
| Signing guards | Missing certificate rejected before changing a binary; an unsigned installer requires explicit preview selection. |

Only documentation, packaging and repository presentation changed for the initial GitHub publication. The tested plugin and standalone binaries are unchanged. Original raw test records are retained locally. Packaged text evidence redacts local filesystem paths without changing test results; final delivered-file checksums are published separately with the release.

## Remaining checks

- Trusted signing of setup, uninstaller, plugin and standalone. No certificate/service is configured; the positive signing path has not been exercised.
- Fresh Windows prerequisite installation, elevation cancellation and all-users installation. Workspace tests used an existing compatible runtime and redirected destinations.
- Interactive DAW/audio-device audition, longer soak and broader machine/host testing.
- Browser-rendered inspection of the offline HTML guide. Its source/relative links and the native Help panel were checked.

No public support email or website is currently published. GitHub issues can be used for non-sensitive project reports. See [the test detail](REPORT.md) and [clean-Windows procedure](../clean-windows-test.md).

Passing validators does not imply Steinberg certification, universal host compatibility or an exact sonic match to another product.
