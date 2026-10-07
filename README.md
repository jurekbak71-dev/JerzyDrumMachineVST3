# Jerzy Drum Machine VST3
12-głosowy kreatywny automat perkusyjny VST3 dla Windows x64. Publikowany build obejmuje wyłącznie VST3.

## Założenia
- 4 głosy analogowe, 4 cyfrowe, 3 sample, 1 synth
- 32 patterny wyzwalane nutami MIDI C4–G6 (60–91)
- bezpośrednie granie 12 głosów nutami C2–B2 (36–47)
- długość patternu 1–64 kroki oraz niezależna długość pętli 1–64 dla każdego instrumentu
- host sync, fabryczna baza patternów, probability, velocity, ratchet, microtiming, accent i flam
- wirtualny perkusista z ewoluującą frazą, fillami i breakami w wybranych odstępach taktów
- song chain z sekcjami intro/zwrotka/refren/bridge/break/outro i długością każdej sekcji
- monofoniczny syntezator dual-oscillator z ADSR, filtrem i klawiaturą ekranową C3–B4; MIDI C4 = 261,626 Hz
- powtarzalne seedem artefakty losowe, mixer z kolorowymi kanałami i oddzielny panel FX
- mixer, per-track reverb/delay sends, master EQ/compressor/drive/clipper
- GUI: czarny bakelit, miedziane akcenty i bursztynowe podświetlenie; SEQ/SOUND/MIX/FX/DRUMMER/SONG

Każdy push uruchamia Windows build i publikuje ZIP VST3 jako artifact GitHub Actions.


Build status: CI validation enabled.
