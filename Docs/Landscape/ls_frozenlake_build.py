"""Builds the Ger_FrozenLake landscape material: M_RTS_Landscape_FrozenLake, MI_RTS_Landscape_FrozenLake and its seven
layer infos. It is ls_pripyat_build.py (same graph: two-scale albedo + normal, tint, height blend, colour variation,
RVT output, tank trails) run with this map's own folder and layer table, so Pripyat is never touched.

Layers: snow inside the cryo pumps' range, a frost crust at its rim, scorched dry ground outside (the scorched 1976
world), dry grass, gravel (roads and pads), mud (lake bed under the ice plane, gorge and canal floors) and ash
(craters, the burnt convoy). Every texture is already in the project (snow from Brushify).

Run headless (layer info names can only be set there, see the Pripyat notes):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="D:/UE5Projects/RTS_Survival/Source/Docs/Landscape/ls_frozenlake_build.py"
or from an editor: sys.path.insert(0, ".../Source/Docs/Landscape"); import ls_frozenlake_build; ls_frozenlake_build.run()
"""
import importlib
import sys

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Docs/Landscape")
import ls_pripyat_build as P  # noqa: E402

DIR = "/Game/RTS_Survival/Materials/ground/Landscape/FrozenLake"

UVB = "/Game/UVB80/Assets/UVB80/Environment/Textures/"
SNOW = "/Game/Brushify/Materials/Landscape/Textures/Snow/"
DRY = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/Textures/"
AIRPORT = "/Game/MilitaryAirport/Landscape/LandscapeTextures/"

# name, preview weight, height offset, height contrast, luma mid, debug colour (see ls_pripyat_build.LAYERS)
LAYERS = [
    ("FLSnow", 1.0, 0.05, 3.0, 0.55, (0.95, 0.95, 1.00)),
    ("FLFrost", 0.2, 0.00, 4.0, 0.15, (0.75, 0.78, 0.82)),
    ("FLScorched", 0.2, 0.00, 4.0, 0.35, (0.70, 0.50, 0.30)),
    ("FLDryGrass", 0.2, 0.05, 4.0, 0.15, (0.75, 0.65, 0.35)),
    ("FLGravel", 0.2, 0.15, 6.0, 0.20, (0.55, 0.55, 0.55)),
    ("FLMud", 0.2, -0.15, 5.0, 0.08, (0.20, 0.16, 0.13)),
    ("FLAsh", 0.2, -0.05, 3.0, 0.10, (0.30, 0.30, 0.32)),
]
# layer -> (albedo, normal, tint rgb, desaturation 0..1, roughness, metres per tile)
LAYER_LOOK = {
    "FLSnow": ("%sT_Snow_D", "%sT_Snow_N", (0.96, 0.97, 1.00), 0.35, 0.70, 8.0),
    "FLFrost": ("%sT_brown_mud_dry_diff_4k", "%sT_brown_mud_dry_nor_4k_N", (0.86, 0.88, 0.92), 0.85, 0.80, 6.0),
    "FLScorched": ("%sT_Ground_Sand_Near_01_BC", "%sT_Ground_Sand_Near_01_N", (0.82, 0.64, 0.46), 0.20, 0.92, 5.0),
    "FLDryGrass": ("%sT_Grass_b_BaseColor_Height", "%sT_Grass_b_Normal", (0.88, 0.74, 0.50), 0.40, 0.92, 6.0),
    "FLGravel": ("%sT_Ground_Rocks_BaseColor_Height", "%sT_Ground_Rocks_Normal", (0.80, 0.78, 0.74), 0.45, 0.85, 5.0),
    "FLMud": ("%sT_brown_mud_dry_diff_4k", "%sT_brown_mud_dry_nor_4k_N", (0.22, 0.19, 0.17), 0.50, 0.35, 4.5),
    "FLAsh": ("%sT_brown_mud_dry_diff_4k", "%sT_brown_mud_dry_nor_4k_N", (0.36, 0.34, 0.33), 0.90, 0.90, 7.0),
}
ROOTS = {"FLSnow": (SNOW, SNOW), "FLFrost": (UVB, UVB), "FLScorched": (DRY, DRY), "FLDryGrass": (AIRPORT, AIRPORT),
         "FLGravel": (AIRPORT, AIRPORT), "FLMud": (UVB, UVB), "FLAsh": (UVB, UVB)}
# tank trails dent these (weight factor)
SOFT_LAYERS = [("FLSnow", 1.0), ("FLFrost", 0.8), ("FLScorched", 0.8), ("FLDryGrass", 0.8), ("FLMud", 1.0),
               ("FLAsh", 1.0)]


def configure():
    importlib.reload(P)
    P.BERLIN_DIR = DIR
    P.TEXTURE_DIR = DIR + "/Textures"
    P.DATA_DIR = DIR + "/DATA"
    P.MAT_NAME = "M_RTS_Landscape_FrozenLake"
    P.MI_NAME = "MI_RTS_Landscape_FrozenLake"
    P.MAT_PATH = DIR + "/" + P.MAT_NAME
    P.MI_PATH = DIR + "/" + P.MI_NAME
    P.DESC = "LSFrozenLake"
    P.GLOBAL_GROUP = "FrozenLake Global"
    P.LAYERS = LAYERS
    P.LAYER_LOOK = LAYER_LOOK
    P._ROOTS = ROOTS
    P.LAYER_NAMES = [layer[0] for layer in LAYERS]
    P.SOFT_LAYERS = SOFT_LAYERS
    return P


def run(stats=True):
    return configure().run(stats)


if __name__ == "__main__":
    # -run=pythonscript runs the file as __main__: build straight away
    print("FL_LS_REPORT %s" % run())
