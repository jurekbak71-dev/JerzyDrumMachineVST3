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
