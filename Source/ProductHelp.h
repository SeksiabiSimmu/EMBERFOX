#pragma once

inline juce::String emberfoxHelpText()
{
    return juce::String("EMBERFOX ")+JucePlugin_VersionString+"\nEmberSoft\n\n"
        "QUICK START\nInsert EMBERFOX on an audio track. Start at a comfortable playback level, raise Drive, and compare the five modes. Use Mix to keep some of the original transient and dynamics.\n\n"
        "CONTROLS\nDrive boosts the signal entering distortion (0 to 36 dB). Tone rolls off high frequencies after distortion (400 Hz to 20 kHz, limited by sample rate). Mix blends dry and processed sound. Output adjusts only the wet signal (-24 to +6 dB); at 0% Mix, Output has no effect. Active/Bypassed switches to latency-aligned dry. There is no output limiter or automatic loudness matching.\n\n"
        "EDITING\nClick a numeric value to type; Enter commits, Escape cancels. Double-click a knob to reset its default. All six parameters support DAW automation and project save/restore.\n\n"
        "FIVE CHARACTERS\nVelvet: smooth rounded saturation. Crunch: dense soft clipping. Razor: sharp hard clipping. Fold: complex wavefolding. Fuzz: biased asymmetric saturation. All use 4x oversampling.\n\n"
        "FACTORY STARTING POINTS\nThe footer menu offers eight examples. Loading one changes the existing parameters and activates the effect. Editing a setting displays Custom. DAW project state retains the complete sound; no separate preset file is needed. Default / Velvet restores the original startup settings. Presets are starting points and are not loudness matched.\n\n"
        "METERS AND FOX\nBars show peak envelopes on a -60 to +6 dBFS scale. The fixed 0 tick marks digital full scale. Peak markers hold for one second, then fall; orange indicates above zero. Input drives expression and movement, while output independently drives eye light and halo. Bypass relaxes the face; meters continue measuring audio.\n\n"
        "INSTALLATION AND TROUBLESHOOTING\nClose your DAW before installing or updating. Install the whole VST3 bundle, then rescan plugins in your DAW. If a user installation is not found, add its VST3 folder to the DAW's scan paths or use an all-user install. The standalone has an audio-device settings panel. Detailed installation steps, system requirements, rollback and a diagnostic checklist are included in the offline User Guide.\n\n"
        "ABOUT AND LICENSE\nOriginal EmberSoft code and vector artwork. AGPL-3.0-only; complete corresponding source and pinned JUCE source accompany the download. JUCE and third-party notices are included. No online support email or website is published for this release. Help works offline; the plugin has no activation or telemetry.\n";
}
