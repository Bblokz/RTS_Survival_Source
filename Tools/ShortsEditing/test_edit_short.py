import json
from fractions import Fraction
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from edit_short import audio_hash, probe, resolve_recipe

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "ShortsCreator" / "Tools"))
from post_shorts import apply_editorial_overlays


class EditorialTimingTests(unittest.TestCase):
    def setUp(self):
        self.recipe = json.loads(Path(__file__).with_name("paratrooper_callouts.json").read_text())

    def test_holds_and_gap_survive_output_frame_rates(self):
        for fps in (Fraction(30), Fraction(60)):
            with self.subTest(fps=fps):
                callouts, _ = resolve_recipe(self.recipe, fps, 120)
                first, second = callouts
                self.assertEqual(first.hold / fps, Fraction(3, 2))
                self.assertEqual(second.hold / fps, 2)
                self.assertEqual((second.start - first.end) / fps, 2)
                self.assertEqual(first.state(first.typing), (len(first.text), 1.0))
                self.assertEqual(first.state(first.end - first.start), (0, 0.0))
                self.assertEqual(second.end / fps, 7)

    def test_rejects_overlap_and_short_video(self):
        self.recipe["callouts"][1].pop("after_previous_seconds")
        self.recipe["callouts"][1]["start_seconds"] = 1
        with self.assertRaises(ValueError):
            resolve_recipe(self.recipe, Fraction(30), 120)
        self.recipe["callouts"] = self.recipe["callouts"][:1]
        with self.assertRaises(ValueError):
            resolve_recipe(self.recipe, Fraction(30), 1)

    def test_pipeline_opt_out_is_unchanged(self):
        self.assertEqual(apply_editorial_overlays("unused", "existing.mp4", {}), "existing.mp4")

    def test_pipeline_opt_in_preserves_duration_and_audio(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "fixture.mp4"
            subprocess.run([
                "ffmpeg", "-v", "error", "-f", "lavfi", "-i", "color=c=0x334a38:s=270x480:r=30:d=8",
                "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000:duration=8",
                "-c:v", "libx264", "-preset", "ultrafast", "-c:a", "aac", "-shortest", str(source),
            ], check=True)
            before = audio_hash(source)
            recipe = Path(__file__).with_name("paratrooper_callouts.json").resolve()
            apply_editorial_overlays(temporary, str(source), {"EditorialOverlayRecipe": str(recipe)})
            self.assertEqual(before, audio_hash(source))
            self.assertEqual(float(probe(source)["format"]["duration"]), 8.0)
            report = json.loads(source.with_suffix(".edit.json").read_text())
            self.assertTrue(report["audio_packets_identical"])
            self.assertEqual(report["output"], str(source))


if __name__ == "__main__":
    unittest.main(verbosity=2)
