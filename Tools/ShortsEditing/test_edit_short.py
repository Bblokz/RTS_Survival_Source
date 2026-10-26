import json
from fractions import Fraction
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from edit_short import CalloutArtwork, audio_hash, probe, resolve_recipe

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "ShortsCreator" / "Tools"))
from post_shorts import apply_editorial_overlays


class EditorialTimingTests(unittest.TestCase):
    def setUp(self):
        self.recipe = json.loads(Path(__file__).with_name("paratrooper_callouts.json").read_text())

    def test_holds_and_gap_survive_output_frame_rates(self):
        for fps in (Fraction(30), Fraction(60)):
            with self.subTest(fps=fps):
                callouts, _ = resolve_recipe(self.recipe, fps, 120)
                first, second, third, fourth, fifth, sixth = callouts
                self.assertEqual(first.hold / fps, Fraction(3, 2))
                self.assertEqual(second.hold / fps, 2)
                self.assertEqual((second.start - first.end) / fps, 2)
                self.assertEqual(first.state(first.typing), (len(first.text), 1.0))
                self.assertEqual(first.state(first.end - first.start), (0, 0.0))
                self.assertEqual(second.end / fps, 7)
                self.assertEqual(third.hold / fps, 2)
                self.assertEqual((third.start - second.end) / fps, 2)
                self.assertEqual(fourth.hold / fps, 3)
                self.assertEqual((fourth.start - third.end) / fps, 1)
                self.assertEqual(fourth.end / fps, Fraction(91, 5))
                self.assertEqual(fifth.start / fps, 26)
                self.assertEqual(fifth.hold / fps, 3)
                self.assertEqual(sixth.start, fifth.end)
                self.assertEqual(sixth.hold / fps, Fraction(3, 2))
                self.assertEqual(sixth.end / fps, Fraction(164, 5))

    def test_wipe_clears_pixels_without_fading_or_sliding(self):
        callouts, style = resolve_recipe(self.recipe, Fraction(30), 120)
        callout = callouts[-1]
        artwork = CalloutArtwork(callout, style, 540, 960)
        exit_start = callout.typing + callout.hold
        middle = artwork.frame(exit_start + callout.exit // 2, Fraction(30))
        self.assertEqual(callout.state(exit_start)[1], 1.0)
        self.assertIsNone(middle.crop((0, 0, artwork.size[0] // 2, artwork.size[1])).getbbox())
        self.assertIsNotNone(middle.getbbox())
        self.assertIsNone(artwork.frame(exit_start + callout.exit - 1, Fraction(30)).getbbox())

    def test_midpoint_and_closing_captions_follow_delivered_duration(self):
        recipe = json.loads(Path(__file__).with_name("king_tiger_is2_callouts.json").read_text())
        for duration in (45, 72.05, 120):
            for fps in (Fraction(30), Fraction(60)):
                with self.subTest(duration=duration, fps=fps):
                    first, last = resolve_recipe(recipe, fps, duration)[0]
                    tolerance = float(Fraction(1, 2) / fps) + 0.000001
                    self.assertAlmostEqual(float(first.start / fps), duration / 2, delta=tolerance)
                    self.assertAlmostEqual(float(last.end / fps), duration - 0.8, delta=tolerance)
                    self.assertEqual(first.hold / fps, 4)
                    self.assertEqual(last.hold / fps, 3)

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
                "ffmpeg", "-v", "error", "-f", "lavfi", "-i", "color=c=0x334a38:s=270x480:r=30:d=35",
                "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000:duration=35",
                "-c:v", "libx264", "-preset", "ultrafast", "-c:a", "aac", "-shortest", str(source),
            ], check=True)
            before = audio_hash(source)
            recipe = Path(__file__).with_name("paratrooper_callouts.json").resolve()
            apply_editorial_overlays(temporary, str(source), {"EditorialOverlayRecipe": str(recipe)})
            self.assertEqual(before, audio_hash(source))
            self.assertEqual(float(probe(source)["format"]["duration"]), 35.0)
            report = json.loads(source.with_suffix(".edit.json").read_text())
            self.assertTrue(report["audio_packets_identical"])
            self.assertEqual(report["output"], str(source))


if __name__ == "__main__":
    unittest.main(verbosity=2)
