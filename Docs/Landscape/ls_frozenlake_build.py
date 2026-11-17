"""Builds the Ger_FrozenLake landscape material: M_RTS_Landscape_FrozenLake, MI_RTS_Landscape_FrozenLake and its seven
layer infos. It is ls_pripyat_build.py (same graph: two-scale albedo + normal, tint, height blend, colour variation,
RVT output, tank trails) run with this map's own folder and layer table, so Pripyat is never touched.

Layers (swampy Karelian tundra, owner's ask 2026-11-17: the brown sand did not fit): snow inside the cryo pumps'
range and a frost crust at its rim; outside it the thawed bog: sphagnum moss, pale reindeer lichen on the rises, rusty
heath (crowberry, dead sedge), dark wet peat in the hollows (also the lake bed, gorge and canal floors), granite on
slopes and outcrops, gravel (roads and pads) and burnt forest floor (the Forge-killed forests, craters, the burnt
convoy). Every texture is already in the project.

Run headless (layer info names can only be set there, see the Pripyat notes):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="D:/UE5Projects/RTS_Survival/Source/Docs/Landscape/ls_frozenlake_build.py"
or from an editor: sys.path.insert(0, ".../Source/Docs/Landscape"); import ls_frozenlake_build; ls_frozenlake_build.run()
"""
import importlib
import sys

import unreal

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Docs/Landscape")
import ls_pripyat_build as P  # noqa: E402

DIR = "/Game/RTS_Survival/Materials/ground/Landscape/FrozenLake"

UVB = "/Game/UVB80/Assets/UVB80/Environment/Textures/"
SNOW = "/Game/Brushify/Materials/Landscape/Textures/Snow/"
AIRPORT = "/Game/MilitaryAirport/Landscape/LandscapeTextures/"
MILBASE = "/Game/MillitaryBase/Landscape/Textures/"
GRASS_MS = "/Game/Megascans/Surfaces/Dried_Grass_wjcpehl/"
BURNT_MS = "/Game/Megascans/Surfaces/Burnt_Forest_Floor_uhwpehcdy/"

# name, preview weight, height offset, height contrast, luma mid, debug colour (see ls_pripyat_build.LAYERS)
# Hard layers sit higher, so granite and gravel poke through moss and peat at a transition.
LAYERS = [
    ("FLSnow", 1.0, 0.05, 3.0, 0.55, (0.95, 0.95, 1.00)),
    ("FLFrost", 0.2, 0.00, 4.0, 0.15, (0.75, 0.78, 0.82)),
    ("FLMoss", 0.2, 0.00, 4.0, 0.10, (0.40, 0.45, 0.22)),
    ("FLLichen", 0.2, 0.05, 4.0, 0.10, (0.78, 0.80, 0.70)),
    ("FLHeath", 0.2, 0.05, 4.0, 0.15, (0.50, 0.28, 0.20)),
    ("FLPeat", 0.2, -0.15, 5.0, 0.10, (0.16, 0.12, 0.09)),
    ("FLGranite", 0.2, 0.15, 6.0, 0.20, (0.55, 0.55, 0.55)),
    ("FLGravel", 0.2, 0.15, 6.0, 0.20, (0.62, 0.60, 0.56)),
    ("FLBurnt", 0.2, -0.05, 3.0, 0.06, (0.22, 0.20, 0.20)),
]
# layer -> (albedo, normal, tint rgb, desaturation 0..1, roughness, metres per tile)
LAYER_LOOK = {
    "FLSnow": ("%sT_Snow_D", "%sT_Snow_N", (0.96, 0.97, 1.00), 0.35, 0.70, 8.0),
    "FLFrost": ("%sT_brown_mud_dry_diff_4k", "%sT_brown_mud_dry_nor_4k_N", (0.86, 0.88, 0.92), 0.85, 0.80, 6.0),
    "FLMoss": ("%sT_Moss002_2K_Color", "%sT_Moss002_2K_NormalDX", (0.66, 0.64, 0.40), 0.30, 0.85, 5.0),
    "FLLichen": ("%sT_Moss002_2K_Color", "%sT_Moss002_2K_NormalDX", (0.92, 0.94, 0.82), 0.80, 0.92, 3.5),
    "FLHeath": ("%sT_Dried_Grass_wjcpehl_2K_D", "%sT_Dried_Grass_wjcpehl_2K_N", (0.78, 0.50, 0.38), 0.25, 0.90, 4.0),
    "FLPeat": ("%sT_brown_mud_dry_diff_4k", "%sT_brown_mud_dry_nor_4k_N", (0.30, 0.24, 0.19), 0.35, 0.35, 5.0),
    "FLGranite": ("%sT_rock_05_diff_4k", "%sT_rock_05_nor_4k_N", (0.80, 0.80, 0.78), 0.60, 0.80, 6.0),
    "FLGravel": ("%sT_Ground_Rocks_BaseColor_Height", "%sT_Ground_Rocks_Normal", (0.80, 0.78, 0.74), 0.45, 0.85, 5.0),
    "FLBurnt": ("%sT_Burnt_Forest_Floor_uhwpehcdy_4K_D", "%sT_Burnt_Forest_Floor_uhwpehcdy_4K_N", (0.85, 0.82, 0.80),
                0.30, 0.90, 6.0),
}
ROOTS = {"FLSnow": (SNOW, SNOW), "FLFrost": (UVB, UVB), "FLMoss": (MILBASE, MILBASE), "FLLichen": (MILBASE, MILBASE),
         "FLHeath": (GRASS_MS, GRASS_MS), "FLPeat": (UVB, UVB), "FLGranite": (UVB, UVB),
         "FLGravel": (AIRPORT, AIRPORT), "FLBurnt": (BURNT_MS, BURNT_MS)}
# tank trails dent these (weight factor)
# Snow keeps only a little of the shared (soil) rut look: the snow-rut stage below draws its own deeper ruts there.
SOFT_LAYERS = [("FLSnow", 0.35), ("FLFrost", 0.5), ("FLMoss", 1.0), ("FLLichen", 0.8), ("FLHeath", 0.8),
               ("FLPeat", 1.0), ("FLBurnt", 1.0)]


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
    import ls_frozenlake_snowruts as snow_ruts_module
    importlib.reload(snow_ruts_module)
    base_build = P.build

    def build(material):
        base_build(material)
        snow_ruts_module.snow_ruts(P, material)
    P.build = build
    return P


def run(stats=True):
    return configure().run(stats)


if __name__ == "__main__":
    # -run=pythonscript runs the file as __main__: build straight away
    print("FL_LS_REPORT %s" % run())
