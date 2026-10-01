"""Cheap tests of the labels; standard library only: python -m unittest test_scene_labels"""

import json
import pathlib
import tempfile
import unittest

from scene_labels import change_times, label_day, near_change, scene_label


class scene_label_t(unittest.TestCase):
    def test_menu_or_stale_status_says_nothing(self):
        self.assertIsNone(scene_label({"Flags": 0}))

    def test_gui_focus_wins_over_flags(self):
        self.assertEqual(scene_label({"Flags": 1 << 4, "GuiFocus": 6}), "galaxy_map")
        self.assertEqual(scene_label({"Flags": 1, "GuiFocus": 5}), "station_services")
        self.assertEqual(scene_label({"Flags": 1 << 4, "GuiFocus": 3}), "panel")

    def test_flags(self):
        self.assertEqual(scene_label({"Flags": 16777224}), "normal")
        self.assertEqual(scene_label({"Flags": 1 << 4}), "supercruise")
        self.assertEqual(scene_label({"Flags": (1 << 4) | (1 << 30)}), "hyperspace")
        self.assertEqual(scene_label({"Flags": 1 | (1 << 3)}), "docked")
        self.assertEqual(scene_label({"Flags": (1 << 1) | (1 << 26)}), "srv")
        self.assertEqual(scene_label({"Flags": 0, "Flags2": 1 | (1 << 4)}), "on_foot")


class near_change_t(unittest.TestCase):
    def test_guard(self):
        changes = change_times([{"ms": 5000, "flags_changed": True}, {"ms": 9000, "flags_changed": False}])
        self.assertTrue(near_change(changes, 4000))
        self.assertTrue(near_change(changes, 6000))
        self.assertFalse(near_change(changes, 3999))
        self.assertFalse(near_change(changes, 9000))


class label_day_t(unittest.TestCase):
    def test_day(self):
        with tempfile.TemporaryDirectory() as tmp:
            day = pathlib.Path(tmp) / "2026-10-01"
            day.mkdir()
            region = {"left": 0.287, "top": 0, "region_width": 0.427, "region_height": 1}
            frames = [
                {"file": "1.png", "taken_ms": 1000, "status": {"Flags": 16}, **region},
                {"file": "2.png", "taken_ms": 20000, "status": {"Flags": 16}, **region},
                {"file": "3.png", "taken_ms": 40000, "status": {"Flags": 0}, **region},
                {"file": "4.png", "taken_ms": 60000, "status": {"Flags": 16}, **region},
            ]
            for f in frames[:3]:
                (day / f["file"]).write_bytes(b"")
            (day / "frames.jsonl").write_text("".join(json.dumps(f) + "\n" for f in frames) + "{cut")
            (day / "status.jsonl").write_text(json.dumps({"ms": 1500, "flags_changed": True}) + "\n")
            rows, dropped = label_day(day)
            self.assertEqual([r["label"] for r in rows], ["supercruise"])
            self.assertEqual(dropped, {"near a change": 1, "no status": 1, "missing file": 1})


if __name__ == "__main__":
    unittest.main()
