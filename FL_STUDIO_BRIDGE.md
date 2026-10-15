# FL Studio integration — development branch v0.1
## Implemented on this branch
- Active pattern MIDI import/export via LOAD KIT and SAVE KIT (select *.mid).
- Import maps note numbers 36..47 to 12 instrument slots, quantizes to 1/16 and supports up to 64 steps.
- Export creates SMF at PPQ 960; no internal DSP algorithm changes except caching oscillator tuning once per block.
- Source/EngineSmoke.cpp includes MIDI roundtrip regression check.
## External Jerzy FL Bridge
- Use `jerzybridge drums` to create separate MIDI tracks and per-voice swing/triplets in FL Studio Piano Roll.
- Correct notes: 36 KICK, 37 SNARE, 38 TOM, 39 METAL HAT, 40 FM PERC, 41 PHASE PERC, 42 WAVE METAL, 43 NOISE RESO, 44 SAMPLE 1, 45 SAMPLE 2, 46 SAMPLE 3, 47 SYNTH.
## Caveats
Not yet built in Windows CI; per-voice triplets are NOT persisted in internal sequencer: use independent Piano Roll notes. Check CPU before/after and MIDI import fidelity; no release until these tests pass.