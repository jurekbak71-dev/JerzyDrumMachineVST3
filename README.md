# Jerzy Drum Machine VST3
12-głosowy kreatywny automat perkusyjny VST3 dla Windows x64. Publikowany build obejmuje wyłącznie VST3.

## Założenia
- 4 głosy analogowe, 4 cyfrowe, 3 sample, 1 synth
- 32 patterny wyzwalane nutami MIDI C4–G6 (60–91)
- bezpośrednie granie 12 głosów nutami C2–B2 (36–47)
- długość patternu 1–64 kroki oraz niezależna długość pętli 1–64 dla każdego instrumentu
- host sync, fabryczna baza patternów, probability, velocity, ratchet, microtiming, accent i flam
- per-step automatyzacja parametrów brzmienia dla każdego głosu; dla sampli pitch/decay/tone/reverse, a dla syntha ADSR
- przycisk CLEAR SEQ do czyszczenia aktualnego patternu; hi-hat perkusisty z podziałem 1/4, 1/8 i 1/16
- wirtualny perkusista z ewoluującą frazą, fillami i breakami w wybranych odstępach taktów
- song chain z sekcjami intro/zwrotka/refren/bridge/break/outro i długością każdej sekcji
- monofoniczny syntezator dual-oscillator z ADSR, filtrem i klawiaturą ekranową C3–B4; MIDI C4 = 261,626 Hz
- powtarzalne seedem artefakty losowe, mixer z kolorowymi kanałami i oddzielny panel FX
- mixer, per-track reverb/delay sends, master EQ/compressor/drive/clipper
- GUI: czarny bakelit, miedziane akcenty i bursztynowe podświetlenie; SEQ/SOUND/MIX/FX/DRUMMER/SONG

Każdy push uruchamia Windows build i publikuje ZIP VST3 jako artifact GitHub Actions.


Build status: CI validation enabled.

## FL Studio recovery v0.2.0 (October 2026)
This repair branch is based on the complete `feature/sequencer-preview-layout` implementation,
**not** the reduced main/early MIDI branch. It preserves independent per-track triplets and
lengths, copy/paste/duplicate track, drummer, song chain, sound edit (PUNCH, DETUNE,
BRIGHTNESS), per-step automation, synth editor, independent stems, master and FX.
Use this branch as the only forward base. Do not overwrite it with the older main-branch
JerzyDrumMachine-VST3 ZIP.
The plug-in identity remains `com.jerzyaudio.drummachine` / `JDrm`.
Windows CI + physical FL Studio acceptance remain mandatory before replacing a stable version.

## Jerzy Drum Machine v0.3.0 (development): Bank editing + extended FX

**SEQUENCER**
- Select instrument and bank **1–16**, press **COPY 16**.
- Select **17–32** (or 33–48 / 49–64), press **PASTE 16**.
- Only the destination 16-step segment changes; unrelated banks and tracks are retained.
  Copy keeps per-step velocity, note, probability, ratchet, microtiming, accent,
  flam and all step-sound automation.
- **CLEAR: TRACK** (default) + **CLEAR SEQ** clears all 64 steps of just the
  visible instrument. Toggle to **CLEAR: ALL** to clear the whole pattern.
- **DUPLICATE** is unchanged and remains a separate action.
- For precise MIDI transfer retain the `.jdmkit` kit file: MIDI alone cannot
  encode per-step sound-automation, effect sends or complete song arrangement.

**MIXER**
- 12 channel strips each with additional **CHORUS** and **TAPE** send knobs.
- The FX are master stereo send/return processors. Existing clean stem outputs
  remain unchanged; the summed master receives the FX returns.
- Sends default to 0 and both effects default OFF, so an existing FL Studio
  project retains its original sound until explicitly enabled.

**FX**
- Chorus / Widener: ON, return, modulation depth, stereo width.
- Tape Drive: ON, return, magnetic saturation/age, mechanical wear (wow,
  flutter, dropouts, occasional surface crackles, hiss), tone LPF.
- Delay mode: Classic/Pingpong, Stereo, Tape Echo, tempo-aligned Sample & Hold
  (stepped output at 1/16), Diffuse Dub. Original Ping-Pong button remains for
  the Classic mode. Delay length/feedback/mix remain host-automatable.
- Tempo automation is smoothed at the read head to avoid abrupt sample jumps;
  very rapid tempo changes may still create a deliberate pitch glide.

**Build / compatibility**
- All NEW APVTS IDs are appended; original parameters, original VST3 identity
  `com.jerzyaudio.drummachine` / `JDrm` and .jdmkit format are preserved.
- CPU: tape filter/normalization, delay clock divisions precomputed per block,
  buffers allocated only on prepareToPlay.
- Windows CI must pass both `JerzyDrumMachine_VST3` and `JerzyDrumMachineTests`
  before it is appropriate to install this development binary in FL Studio.
- Tests cover GUI bank copy/paste, CLEAR scope, stereo chorus, tape, and
  finite output for 5 delay modes. A real FL Studio GUI/audio session remains
  mandatory before production release.

## v0.3.1 — FL Studio MIDI Piano Roll / offline WAV fix

### Main source selector (top-left)
- **MIDI / PIANO ROLL** (factory default, persisted with FL Project): only incoming MIDI notes trigger sound. FL transport and offline export cannot start the internal sequencer, drummer or song chain.
- **INTERNAL SEQ**: sequencer patterns follow FL Studio's BPM/transport and the dedicated SEQ RUN / SONG controls. MIDI notes are still playable on top of patterns.
- Existing presets without `playback_mode` default to MIDI, to prevent unexpected SEQ sounds and inappropriate WAV renders.
- MIDI Ch 1 notes 36..47 select drum voices 1..12. Ch 2 is for pitched synth. Other notes on Ch 1 and other MIDI channels also trigger the pitched synth.
- Ch 16 notes 60..91 do **NOT** automatically switch patterns. Set `Enable MIDI Ch16 Pattern Select` in FL Studio's exposed plug-in parameters if specifically needed; it is effective only in SEQ mode.
- MIDI event sample positions are preserved inside the VST3 audio block; FL Piano Roll velocities and notes determine the generated output without hidden SEQ triggers.

### Critical export and state bug
The earlier `DrumEngine::prepare()` reinitialized **all factory patterns**. Hosts including FL Studio can call prepare when rendering WAV, switching sample rate, rescanning or reopening plugins. As of v0.3.1, factory patterns are initialized **only in the engine constructor**, not in prepare. The complete kit/state and edited voice sounds survive `prepareToPlay` and offline WAV export.

### FL Studio usage
1. Load Jerzy Drum Machine v0.3.1, set source **MIDI / PIANO ROLL** in the top-left corner.
2. Put percussion notes 36..47 in Piano Roll, or use MIDI channel 2 for pitched synth parts.
3. Save the FL Project, re-open and render Song to WAV. The plugin must reproduce the same incoming MIDI without any internal sequencer.
4. Select **INTERNAL SEQ** only when using pattern/song generator; the transport can then run its own patterns.
5. Do not install both old 0.3.0 and 0.3.1 bundles in multiple scanned locations; back up the old bundle before replacing it.

### Automated regressions
`EngineSmoke.cpp` validates no sequence triggers on host transport in MIDI mode, correct note offset,
no Ch16 pattern switching by default, preserved sound/pattern data after audio sample-rate prepare,
and continued functioning of SEQ when explicitly selected.

Native FL Studio verification remains separate from Windows C++ CI.
