# System requirements

- Windows x64 VST3 host; 32-bit hosts are unsupported. The standalone does not require a DAW.
- Installer baseline: Windows 10 build 19041 or later on x64. Automated validation was performed on this development machine; this is not a claim of comprehensive Windows/DAW compatibility.
- Microsoft Visual C++ x64 v14 runtime 14.50.35717 or newer. Setup bundles the official Microsoft-signed 14.51.36247 package and skips it when a suitable runtime is already present.
- Mono or stereo operation. Float32 processing; float64 processing is not advertised.
- Tested sample rates 44.1/48/96/192 kHz. Fixed 4x FIR oversampling; 61 samples compensated latency.
- Default editor 980x640, resizable 770x503 to 1470x960.

No minimum CPU/RAM performance claim is made before cross-machine profiling. Actual limits depend on sample rate, block size, instance count and host. Use a moderate playback level when trying factory examples; there is no automatic loudness match or output limiter.
