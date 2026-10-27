# Third-party components

EMBERFOX by EmberSoft: original source and vector artwork are AGPL-3.0-only. No WXAudio code or art is included. NANI/WXAudio names identify the reference product only; this project is independent and is not endorsed by WXAudio.

JUCE: 8.0.16-security, commit `ea77444750de45e036e0279bb0291144bf8e3688`, https://github.com/juce-framework/JUCE. The included build follows the AGPLv3 option described in JUCE's LICENSE.md. The complete pinned repository source, including embedded dependencies and their notices, accompanies the source package as `external/JUCE-source.zip`. Extract it into `external/` to produce `external/JUCE/`.

The JUCE dependency's `LICENSE.md` lists embedded third-party components, including the VST3 SDK, HarfBuzz, SheenBidi, PNG, JPEG, zlib, FLAC and Ogg Vorbis. Preserve that document and the upstream notices when redistributing. The source package includes them in the vendor archive. The VST3 SDK in this pinned JUCE release identifies its license as MIT; see its own LICENSE.txt in the vendor source.

pluginval 1.0.4 (https://github.com/Tracktion/pluginval) and Steinberg VST3 SDK v3.8.1_build_84 (commit `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`, https://github.com/steinbergmedia/vst3sdk) are external validation tools, not plugin runtime dependencies or bundled product binaries. Their output does not imply certification by Steinberg or Tracktion. The separate validator SDK is MIT-licensed; its pinned revision and bootstrap instructions accompany the source.

The Windows installer is built with Inno Setup 7.1.0 (https://jrsoftware.org/isinfo.php). Its license is preserved in `installer/INNO-SETUP-LICENSE.txt` in the corresponding source. The compiler and its own installer are development tools and are not included in the product/source ZIPs.

Setup embeds the unmodified Microsoft Visual C++ x64 Redistributable 14.51.36247.0 as a prerequisite. This Microsoft-signed component remains subject to its own Microsoft terms; it is not part of EMBERFOX's AGPL source. Setup checks an existing compatible runtime before running it and uninstallation does not remove the shared runtime. The original download URL, SHA256 and expected signature publisher are in `scripts/tool-versions.json`. See https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist for Microsoft's runtime guidance.

The Microsoft compiler, Windows SDK and redistributable runtime are not included in the corresponding source archive. The product does not include NANI, validator executables or a signing certificate.
