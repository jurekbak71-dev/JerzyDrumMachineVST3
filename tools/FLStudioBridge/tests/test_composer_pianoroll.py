"""Smoke tests for the Jerzy Composer FL Studio Python Piano Roll script.
Run without FL Studio using the limited documented flpianoroll API mock.
This does NOT replace actual FL Studio integration tests.
"""
import pathlib
import runpy
import sys
import types
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "piano_roll_scripts" / "Jerzy Composer Piano Roll.pyscript"


class Note:
    def __init__(self):
        self.number = 60
        self.time = 0
        self.length = 1
        self.velocity = 0.8
        self.color = 0


class Score:
    PPQ = 960
    tsnum = 4
    tsden = 4

    def __init__(self):
        self.notes = []

    def addNote(self, note):
        self.notes.append((note.number, note.time, note.length,
                           round(note.velocity, 6), note.color))

    def clearNotes(self, selected):
        self.notes.clear()


class Dialog:
    def __init__(self, title, description):
        self.inputs = {}
        self.options = {}

    def addInputCombo(self, name, options, default, **kwargs):
        self.inputs[name] = default
        self.options[name] = options

    def addInputKnobInt(self, name, default, minimum, maximum, **kwargs):
        self.inputs[name] = default

    def addInputKnob(self, name, default, minimum, maximum, **kwargs):
        self.inputs[name] = default

    def addInputCheckbox(self, name, default, **kwargs):
        self.inputs[name] = default

    def getInputValue(self, name):
        return self.inputs[name]


class ComposerSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.flp = types.ModuleType("flpianoroll")
        cls.flp.score = Score()
        cls.flp.Note = Note
        cls.flp.ScriptDialog = Dialog
        sys.modules["flpianoroll"] = cls.flp
        cls.script = runpy.run_path(str(SCRIPT))
        cls.fields = cls.script["FIELD"]

    def form(self, **params):
        dialog = self.script["createDialog"]()
        for name, value in params.items():
            dialog.inputs[self.fields[name]] = value
        return dialog

    def generate(self, **params):
        self.flp.score.notes.clear()
        self.script["apply"](self.form(**params))
        return list(self.flp.score.notes)

    def test_musical_pitch_names_not_midi_numbers(self):
        f = self.form()
        pitch = f.options[self.fields["tonic"]]
        self.assertEqual(len(pitch), 12)
        self.assertEqual(pitch[0], "C")
        self.assertEqual(pitch[4], "E")
        self.assertTrue(pitch[-1].startswith("H"))
        # Bas: note 0 of first chord transposes exactly four semitones C -> E.
        c = self.generate(mode=1, progression=0, tonic=0, variation=0)[0][0]
        e = self.generate(mode=1, progression=0, tonic=4, variation=0)[0][0]
        self.assertEqual(e-c, 4)

    def test_all_five_modes_create_valid_notes(self):
        for mode in range(5):
            for style in range(8):
                notes = self.generate(mode=mode, style=style, bars=4, seed=17)
                with self.subTest(mode=mode, style=style):
                    self.assertGreater(len(notes), 0)
                    self.assertLess(len(notes), 1500)
                    for key, time, length, velocity, color in notes:
                        self.assertTrue(0 <= key <= 127)
                        self.assertTrue(0 <= time < 4*3840)
                        self.assertGreater(length, 0)
                        self.assertTrue(0 < velocity <= 1)
                        self.assertTrue(0 <= color < 16)

    def test_reproducibility(self):
        a = self.generate(mode=4, bars=8, seed=998, scale=2)
        b = self.generate(mode=4, bars=8, seed=998, scale=2)
        self.assertEqual(a, b)

    def test_melodic_variation_remains_harmonic(self):
        for scale in range(7):
            notes = self.generate(mode=4, bars=4, scale=scale, seed=71,
                                  variation=0.8, tonic=4)
            self.assertGreater(len(notes), 4)
            # All pitch classes belong to chosen scale (global tonality).
            pcs = {(4 + x) % 12 for x in self.script["SCALES"][scale]}
            self.assertTrue(all(n[0] % 12 in pcs for n in notes))

    def test_progression_changes_chord_pitches(self):
        notes = self.generate(mode=2, bars=4, progression=1, voicing=0,
                              style=4, color=1, seed=99)
        first_notes = [n[0] for n in notes if n[1] < 960]
        second_notes = [n[0] for n in notes if 3840 <= n[1] < 4800]
        self.assertNotEqual(sorted(first_notes), sorted(second_notes))

    def test_triplet_and_three_quarter_time(self):
        self.flp.score.tsnum = 3
        self.flp.score.tsden = 4
        try:
            for mode in range(5):
                notes = self.generate(mode=mode, division=2, bars=3)
                with self.subTest(mode=mode):
                    self.assertTrue(notes)
                    self.assertTrue(all(n[1] < 3*3*960 for n in notes))
        finally:
            self.flp.score.tsnum = 4
            self.flp.score.tsden = 4

    def test_keep_old_notes_when_replace_is_off(self):
        original = (45, 7, 12, 0.8, 0)
        self.flp.score.notes = [original]
        self.script["apply"](self.form(replace=False, mode=0))
        self.assertIn(original, self.flp.score.notes)
        self.assertGreater(len(self.flp.score.notes), 1)


if __name__ == "__main__":
    unittest.main()
