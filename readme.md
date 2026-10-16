Light Host 2.2.0
---

A lightweight VST3/AU audio plugin host for macOS, Windows, and Linux that sits in the menu bar / system tray.

### Features & Improvements
- **Latest Tech Stack:** Modern C++20 standard, latest JUCE 9.0.4 framework.
- **Native Plugin Support:** VST3 and AU plugins (VST3 on all platforms, AU on macOS).
- **Background Plugin Scanner:** Non-blocking real-time background scanner with first-run auto-scan and custom folder picker.
- **Startup Crash Protection:** Active plugin crash detection (dead-man's pedal) that bypasses failing plugins instead of crashing on launch.
- **Audio Device Resilience:** Graceful fallback if a saved audio driver is uninstalled or offline; prevents device settings reset on sleep or service restart.
- **One-Click Audio Recovery:** "Restart Audio Engine" option in tray menu to restore sound after PC sleep or driver reconnects.
- **Multiple Plugin Instances:** Add multiple instances of the same plugin (e.g. dual EQ or compressor) with independent states and bypass controls.
- **Order Preservation:** Moving plugins up or down in the chain safely retains all parameter settings.
- **Unified Tray Menu:** Accessible via both left and right clicks; includes quick access to Settings, Plugin Scanner, and Settings Folder.
- **Mono-to-Stereo Routing:** Automatic routing of mono plugin outputs to both stereo channels with user notification.
- **Resizable Plugin Windows:** Automatic support for resizable plugin editors.

### Releases
Download prebuilt executables and installers from the [Releases](https://github.com/Drahonn/LightHost/releases) page:
- **Windows:** `LightHost.exe` (standalone executable)
- **macOS:** `LightHost.dmg` (native installer disk image)
- **Linux:** `LightHost-Linux` (standalone executable)
