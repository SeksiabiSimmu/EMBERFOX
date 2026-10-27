# EMBERFOX 1.3.0 — EmberSoft

Windows x64 VST3 distortion/saturation effect and standalone application. Check `release-status.json` for this download's signing status. Installers ending in `-unsigned.exe` are **unsigned previews**. Clean-Windows verification remains pending for this preview.

Run the separately supplied Setup application, choose installation for yourself or all users, then rescan VST3 plugins in your DAW. Setup includes the required Microsoft-signed x64 Visual C++ runtime and installs it only when needed. All-user installation and a missing shared runtime may request Windows administrator approval.

For manual ZIP installation, copy the entire `EMBERFOX.vst3` folder into your DAW's VST3 folder. Do not copy only the DLL inside Contents. The standard all-user location is `C:\Program Files\Common Files\VST3`; the current-user location is `%LOCALAPPDATA%\Programs\Common\VST3`. Some hosts require adding the current-user location to their scan paths.

Open **Documentation/UserGuide.html** for controls, modes, factory examples, installation, update/rollback and troubleshooting. In the plugin, use **HELP / ABOUT** for offline help. **Output changes only the wet signal**; at zero Mix, Output does not change dry audio.

The source archive is included. License: **AGPL-3.0-only**. Preserve LICENSE.txt, THIRD_PARTY.md and corresponding source when redistributing as required by the licence. No activation, telemetry or online connection is needed by the plugin. No support email or website is currently published by EmberSoft.
