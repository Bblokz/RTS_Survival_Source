"""Builds M_WorldPiece_Concrete and four test settings of it, for one side-by-side picture (headless, -nullrhi).

The test instances are in Story/Shared/ConcreteTest; lineup_capture.py with WORLDPIECES_VARIANTS=1 shows them.
Log: Saved/WorldMapPieces/concrete_variants.txt
"""
import os
import sys
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import piece_lib as lib  # noqa: E402
import concrete_lib  # noqa: E402

TEST_FOLDER = concrete_lib.TEST_FOLDER
FLAK_TOWER_LENGTH_SCALE = 4000.0 / 61.0
VARIANTS = {
    "A": dict(TileCm=24.0, PourHeightCm=9.0, PanelWidthCm=20.0, JointWidthCm=0.5),
    "B": dict(TileCm=45.0, PourHeightCm=12.0, PanelWidthCm=26.0, JointWidthCm=0.6, ReliefStrength=1.5),
    "C": dict(TileCm=14.0, PourHeightCm=7.0, PanelWidthCm=16.0, JointWidthCm=0.4),
    "D": dict(TileCm=45.0, PourHeightCm=9.0, PanelWidthCm=20.0, JointWidthCm=0.5, ReliefStrength=2.0,
              NormalStrength=1.6, Variation=1.6),
}

try:
    material = concrete_lib.ensure_concrete_material()
    for name, overrides in (VARIANTS.items() if os.environ.get("WORLDPIECES") != "material" else []):
        concrete_lib.concrete_instance(material, TEST_FOLDER, "MI_WP_ConcreteTest_%s_Model" % name,
                                       GrimeBaseCm=12.5, **overrides)
        concrete_lib.concrete_instance(material, TEST_FOLDER, "MI_WP_ConcreteTest_%s_Tower" % name,
                                       length_scale=FLAK_TOWER_LENGTH_SCALE, **overrides)
        lib.log("variant %s done" % name)
except Exception:
    lib.log(traceback.format_exc())
lib.write_log("concrete_variants.txt")
