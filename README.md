# Jerzy Drum Machine VST3
12-głosowy kreatywny automat perkusyjny VST3 dla Windows x64.

## Założenia
- 4 głosy analogowe, 4 cyfrowe, 3 sample, 1 synth
- 32 patterny wyzwalane nutami MIDI C4–G6 (60–91)
- bezpośrednie granie 12 głosów nutami C2–B2 (36–47)
- 16 kroków w pierwszej działającej wersji silnika; model danych przygotowany na 64
- host sync, fabryczna baza patternów, probability
- docelowo velocity/ratchet/microtiming/accent/flam, generator/mutate/fill/evolve
- mixer, per-track reverb/delay sends, master EQ/compressor/drive/clipper
- GUI: czarny bakelit, stara miedź, bursztynowe podświetlenie; trzy strony SEQ/SOUND/MIX

Każdy push uruchamia Windows build i publikuje ZIP VST3 jako artifact GitHub Actions.
