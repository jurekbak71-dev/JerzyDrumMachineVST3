# Jerzy Composer — Windows x64 (wersja rozwojowa 0.4)

Projekt jest odizolowany w katalogu tools/ i nie zastępuje Jerzy Drum Machine VST3.

## Moduły

### Jerzy Composer Piano Roll 0.3 (.pyscript)
Tworzy nuty bezpośrednio w Piano Roll FL Studio: perkusja, bas, akordy, arpeggio i melodia. Tonika wybierana jako C, Cis/Des, D, Dis/Es, E, F, Fis/Ges, G, Gis/As, A, Ais/B lub H; osobno oktawa. Jest 7 skal, 7 progresji, 8 stylów, swing, gęstość, artykulacja i wariacje. Uwaga: zaznaczenie "Zastąp istniejące nuty" może usunąć wszystkie nuty z aktywnego rolla. Pracuj na kopii.

### Jerzy Composer VST3 0.4
VST3 to **generator MIDI, nie syntezator audio**. Domyślny JUCE Generic Editor zastąpiono responsywnym panelem po polsku, ze sterowaniem C–H + oktawa, progresją i typem akordu, ośmioma groove, długością frazy, swingiem, naturalnością i wariacjami.

Pięć trybów: Perkusja / Bas / Akordy / Arpeggio / Melodia. Bas i harmonia podążają za akordami, melodia stosuje motywy, przerwy i rozwiązania fraz. Generator uwzględnia BPM, metrum, pozycję transportu i wejściowe MIDI w arpeggiatorze. Sygnał audio na wyjściu VST3 jest celowo cichy. Parametry są automatyzowalne i zapisują się w projekcie.

**Ważne:** silnik VST3 i skrypt Piano Roll mają pokrewne rozwiązania muzyczne, lecz nie generują identycznych nut dla danego seed. VST3 nie zapisuje samodzielnie nut w Piano Roll. Do edytowalnych nut wybierz skrypt.

### JerzyFLBridge.exe
Aplikacja .NET 8 Windows x64: instaluje pięć skryptów Piano Roll, generuje MIDI/JPF i konwertuje pliki.

## Instalacja i użycie VST3 w FL Studio

1. Pobierz ZIP Jerzy-Composer-VST3-Windows-x64 z zielonej kompilacji GitHub Actions i rozpakuj.
2. Skopiuj cały folder Jerzy Composer.vst3 do C:\Program Files\Common Files\VST3.
3. Otwórz FL Studio → Options → Manage plugins i zleć przeskanowanie VST3.
4. W Channel Rack dodaj Jerzy Composer. Ustaw: PARTIA = Bas, TONIKA = C, REJESTR = C3, KANAŁ MIDI = 1, GENERUJ MIDI = ON.
5. W ustawieniach wrappera Jerzy Composer znajdź porty MIDI i ustaw **Output port = 10**.
6. Dodaj syntezator VST, którym chcesz sterować. W jego wrapperze ustaw **Input port = 10** i domyślny MIDI channel 1.
7. Naciśnij PLAY. Composer wysyła nuty, instrument syntezuje audio.

**Port 10 jest przykładem.** Oba porty muszą mieć tę samą wartość. FL Studio zarządza portami wrappera, VST3 nie może ustawić ich samodzielnie. Routing wyjścia MIDI VST3 nie był jeszcze sprawdzony interaktywnie na Twoim FL Studio; jeżeli dany host go nie przekazuje, wygodnym obejściem jest skrypt Piano Roll. Nie twierdzimy, że wszystkie warianty FL Studio/VST3/Patcher są przetestowane.

## Co robią parametry?
- TONIKA / DŹWIĘK = podstawowa nuta np. C, D lub E; REJESTR = oktawa C2–C6.
- SKALA = zbiór dostępnych wysokości; PROGRESJA = następstwo akordów w taktach.
- GĘSTOŚĆ = ile nut i pauz; SWING = kołysanie; NATURALNOŚĆ = subtelne różnice velocity i timingu.
- RODZAJ AKORDU = triada, 7, sus, add9; UKŁAD HARMONII = bliski, szeroki albo płynne przejścia.
- WARIACJE FRAZY = urozmaicenie powracających motywów; DŁUGOŚĆ NUT = staccato/legato.
- WARIANT / SEED = powtarzalny wariant generowania; KANAŁ MIDI (1–16) nie jest portem wrappera FL Studio.

## Testy automatyczne
Workflow .github/workflows/jerzy-composer-windows.yml: kontrola Python, testy czystego silnika C++, kompilacja JUCE 8.0.4 Windows x64, osobne artefakty EXE i VST3.

## Warunki pełnego wydania
- [ ] Potwierdzić w FL Studio VST3 MIDI output → syntezator, również w Patcher.
- [ ] Sprawdzić skalowanie GUI przy DPI 100, 125, 150, 200%.
- [ ] Sprawdzić START/STOP, tempo, loop, zmianę kanału oraz eksport.
- [ ] Zmierzyć obciążenie procesora i pamięci dla 44,1/48/96 kHz i małych buforów.
- [ ] Sprawdzić zapis i wczytanie ustawień projektu.

Do momentu tych testów VST3 pozostaje wersją rozwojową.
