# Jerzy Composer for FL Studio (Windows x64) — 0.2 development

This is an isolated integration worktree under `tools/`; it does **not** replace the main Drum Machine VST3 or modify its presets.

## 1. Jerzy Composer Piano Roll
- Native .pyscript uses built-in FL Studio `flpianoroll` API.
- Generate drums, bass, chords, arpeggios and seed-reproducible generative melodies **directly** in the selected Piano Roll.
- Install with `JerzyFLBridge.exe` in the FL Studio tab; no external Python necessary.
- Select from Piano Roll > Tools > Scripts > Jerzy Composer Piano Roll.
- Changing parameters rebuilds the preview; **Replace existing notes** option replaces the active selection or roll, so save a copy of your pattern first.

## 2. Jerzy Composer VST3
- Host synced (PPQ/BPM), emits MIDI notes, outputs silence as an instrument; MIDI input can provide held notes for arpeggiation.
- 5 modes: Drums / Bass / Chords / Arp / Generative. 13 automatable VST3 parameters.
- Install `Jerzy Composer.vst3` under `C:\Program Files\Common Files\VST3`, scan plugins in FL Studio.
- VST3 has no direct authority to edit Piano Roll. Use a compatible FL Studio MIDI routing/record/burn workflow. Routing and Burn MIDI **must be tested in the actual host**.
- GUI is JUCE generic parameter editor v0.1 (functional, not final hardware appearance).
- Stops notes on transport stop, and resets on host timeline seek.

## 3. JerzyFLBridge.exe
- Portable, compiled as a **self-contained single-file .NET 8 Windows x64 EXE**.
- Creates MIDI/JPF, polyrhythms, bass/chords; converts MIDI/JPF; installs five Piano Roll scripts, reads a basic FLP header.
- Does not install Python or require manual command line.
- Extract ZIP to a normal folder (e.g. Downloads/JerzyFLBridge), double-click `JerzyFLBridge.exe`; use the **FL Studio** tab to install scripts.
- App is not digitally signed. Windows SmartScreen may request a trust decision; don't disable antivirus. Inspect project source/build artifacts and verify you trust them.

## Build
CI: `.github/workflows/jerzy-composer-windows.yml` builds both deliverables using .NET 8, JUCE 8.0.4, MSVC and Composer unit tests.

**Not yet validated on real FL Studio host** — leave this PR as draft until host routing and GUI verification pass.
