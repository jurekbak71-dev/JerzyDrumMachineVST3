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


Build status: CI validation enabled.

## Dodatkowy silnik warstwowy (gałąź expressive-layered-dsp)

Na stronie SOUND dostępny jest piąty parametr **LAYER** dla każdego z 12 głosów:
- **0**: niezmienione oryginalne brzmienie silnika (zgodność z istniejącymi zestawami i projektami FL Studio).
- **1**: nowy silnik z osobnymi obwiedniami BODY, NOISE i CLICK, skokiem wysokości na początku uderzenia i dodatkowym rezonansem harmonicznym.
- **0..1**: mieszanie silników. TUNE, DECAY, TONE i CHARACTER kontrolują również nowe warstwy.

Dla trzech slotów samplerów załadowany WAV ma nadal pierwszeństwo; LAYER wpływa na syntezator awaryjny pustego slotu. Parametr jest automatyzowalny i zapisywany wraz z ustawieniami głosu oraz w plikach kit. Dodane testy smoke sprawdzają dźwięk, NaN/Inf i zapis/odczyt parametru.

Nowy DSP to autorska implementacja Jerzy Audio inspirowana koncepcją syntezy warstwowej, a nie kopia kodu Drum Foundry/AGPL. Implementacja nie ingeruje w mapowanie nut MIDI ani w uruchamianie SEQ przez host.
