"""The 16 Berlin rubble decal cells: one table shared by the Blender render, the atlas compositor and the UE scripts.

Atlas contract (same as the impact-crater atlas, see Source/Docs/Environment/ImpactDecals.md):
  cell i (0..15) sits at column i % 4, row i // 4 (row 0 = top of the image).
  Every cell is drawn at one texel density: the full cell is CELL_CM wide in the world (decal box = CELL_CM square at
  scale 1), so a brick is the same size in every cell and cells mix on the ground. Placement scales 0.75 - 1.4.
  Image up (-V) is the "source" side for the directional cells: the fan spills from the top edge downwards, the strip
  runs left-right, the corner pile sits in the top-left corner.

Fields:
  name   asset suffix (MI_BerlinRubbleDecal_<NN>_<name>)
  shape  radial | fan | strip | corner | scatter | sett_patch
  n      piece count
  mix    list of (piece type, material, weight, scale min, scale max)
  heap   pile height in metres at the densest point (pieces stack up to it)
  bed    (sRGB colour, opacity, spread in metres, grit amount): the dust/soil layer under and around the pieces
  use    where the placement step uses it (documentation only)
"""

CELL_CM = 400.0
RENDER_PX = 1024
OUT_CELL_PX = 512
GRID = 4

# sRGB 0..1 base colours, anchored on the Berlin ruin kit textures (brick mean 0.43/0.33/0.29, plaster 0.60/0.58/0.55,
# kit rubble 0.43/0.38/0.35) and the lit ground of the critic shots (~0.55/0.52/0.51). Kept low in chroma: the map's
# rubble pink was toned down once already (berlin-ruin-material-pass).
MATERIALS = {
    #             colour A               colour B (share)            roughness  dust  variation
    "brick":      ((0.40, 0.31, 0.27), (0.44, 0.39, 0.34), 0.25, 0.86, 0.55, 0.12),
    "brick_dark": ((0.30, 0.22, 0.19), (0.36, 0.27, 0.22), 0.30, 0.88, 0.35, 0.12),
    "mortar":     ((0.55, 0.52, 0.48), (0.48, 0.45, 0.41), 0.40, 0.92, 0.30, 0.10),
    "plaster":    ((0.66, 0.64, 0.60), (0.58, 0.56, 0.52), 0.35, 0.90, 0.20, 0.08),
    "stucco":     ((0.62, 0.59, 0.53), (0.52, 0.53, 0.52), 0.40, 0.88, 0.20, 0.08),
    "concrete":   ((0.45, 0.44, 0.42), (0.39, 0.39, 0.38), 0.40, 0.90, 0.35, 0.09),
    "tile":       ((0.42, 0.29, 0.24), (0.36, 0.29, 0.26), 0.35, 0.80, 0.40, 0.12),
    "slate":      ((0.19, 0.20, 0.22), (0.24, 0.24, 0.25), 0.35, 0.50, 0.25, 0.10),
    "wood_charred": ((0.09, 0.08, 0.07), (0.16, 0.14, 0.12), 0.35, 0.85, 0.20, 0.25),
    "wood_old":   ((0.36, 0.30, 0.24), (0.28, 0.23, 0.18), 0.40, 0.82, 0.35, 0.15),
    "charcoal":   ((0.05, 0.05, 0.05), (0.09, 0.085, 0.08), 0.30, 0.70, 0.10, 0.20),
    "paper":      ((0.62, 0.60, 0.54), (0.45, 0.42, 0.37), 0.40, 0.85, 0.10, 0.12),
    "paper_burnt": ((0.20, 0.18, 0.16), (0.40, 0.36, 0.30), 0.50, 0.85, 0.05, 0.20),
    "glass":      ((0.32, 0.37, 0.36), (0.42, 0.45, 0.43), 0.35, 0.05, 0.15, 0.10),
    "granite":    ((0.37, 0.36, 0.37), (0.44, 0.42, 0.41), 0.45, 0.70, 0.30, 0.12),
    "paving":     ((0.44, 0.43, 0.40), (0.38, 0.37, 0.35), 0.35, 0.85, 0.30, 0.08),
    "steel_rust": ((0.28, 0.19, 0.13), (0.21, 0.16, 0.13), 0.40, 0.72, 0.25, 0.15),
    "steel_dark": ((0.16, 0.16, 0.16), (0.24, 0.21, 0.19), 0.30, 0.55, 0.25, 0.12),
    "sandstone":  ((0.55, 0.50, 0.43), (0.48, 0.44, 0.39), 0.35, 0.88, 0.25, 0.08),
    "cloth":      ((0.26, 0.25, 0.24), (0.30, 0.24, 0.20), 0.45, 0.95, 0.30, 0.15),
}

BED_MORTAR = (0.50, 0.47, 0.43)
BED_BRICKDUST = (0.47, 0.40, 0.36)
BED_PLASTER = (0.64, 0.62, 0.58)
BED_ASH = (0.27, 0.26, 0.25)
BED_SOIL = (0.38, 0.35, 0.31)
BED_SAND = (0.50, 0.46, 0.40)
BED_RUST = (0.33, 0.27, 0.23)
BED_SANDSTONE = (0.58, 0.53, 0.45)
BED_GLASSDUST = (0.53, 0.52, 0.50)

CELLS = [
    dict(name="BrickSpillFan", shape="fan", n=620, heap=0.42,
         mix=[("brick", "brick", 0.30, 0.9, 1.0), ("halfbrick", "brick", 0.25, 0.8, 1.0),
              ("chunk", "brick", 0.15, 0.04, 0.10), ("brickclump", "brick", 0.04, 0.9, 1.1),
              ("chunk", "mortar", 0.18, 0.03, 0.09), ("flake", "plaster", 0.08, 0.04, 0.09)],
         bed=(BED_MORTAR, 0.70, 0.35, 1.0), use="against lone walls and ruin faces, yaw so the top edge meets the wall"),
    dict(name="BrickHeapSpread", shape="radial", n=700, heap=0.45,
         mix=[("brick", "brick", 0.26, 0.9, 1.0), ("halfbrick", "brick", 0.24, 0.8, 1.0),
              ("chunk", "brick", 0.16, 0.04, 0.11), ("brickclump", "brick", 0.06, 0.9, 1.2),
              ("brick", "brick_dark", 0.06, 0.9, 1.0), ("chunk", "mortar", 0.22, 0.03, 0.11)],
         bed=(BED_BRICKDUST, 0.72, 0.40, 1.0), use="collapsed plots, round heaps"),
    dict(name="MasonryStrip", shape="strip", n=380, heap=0.30,
         mix=[("brick", "brick", 0.30, 0.9, 1.0), ("halfbrick", "brick", 0.20, 0.8, 1.0),
              ("chunk", "concrete", 0.15, 0.05, 0.16), ("chunk", "mortar", 0.15, 0.04, 0.10),
              ("flake", "plaster", 0.12, 0.05, 0.11), ("plank", "wood_old", 0.02, 0.4, 0.9),
              ("chunk", "sandstone", 0.05, 0.06, 0.14)],
         bed=(BED_MORTAR, 0.66, 0.30, 1.0), use="along wall runs and kerb berms, yaw = run heading"),
    dict(name="CornerPile", shape="corner", n=480, heap=0.55,
         mix=[("brick", "brick", 0.35, 0.9, 1.0), ("halfbrick", "brick", 0.20, 0.8, 1.0),
              ("brickclump", "brick", 0.10, 1.0, 1.4), ("chunk", "mortar", 0.15, 0.05, 0.14),
              ("flake", "plaster", 0.12, 0.05, 0.12), ("plank", "wood_charred", 0.02, 0.5, 1.0),
              ("lath", "wood_old", 0.02, 0.4, 0.8)],
         bed=(BED_MORTAR, 0.72, 0.35, 1.0), use="outside ruin corners, top-left = the corner"),
    dict(name="PlasterDust", shape="radial", n=420, heap=0.10,
         mix=[("flake", "plaster", 0.45, 0.04, 0.14), ("flake", "stucco", 0.20, 0.05, 0.16),
              ("lath", "wood_old", 0.025, 0.3, 0.7), ("chunk", "plaster", 0.25, 0.03, 0.09),
              ("halfbrick", "brick", 0.08, 0.8, 1.0)],
         bed=(BED_PLASTER, 0.78, 0.55, 1.4), use="pale dust at doorways and blown-out ground floors"),
    dict(name="ConcreteRebar", shape="radial", n=260, heap=0.40,
         mix=[("slabchunk", "concrete", 0.30, 0.22, 0.60), ("chunk", "concrete", 0.55, 0.04, 0.16),
              ("rebar", "steel_rust", 0.05, 0.5, 1.2), ("chunk", "mortar", 0.10, 0.02, 0.06)],
         bed=(BED_MORTAR, 0.62, 0.40, 1.2), use="bunker and flak-tower feet, Reichstag, industrial ruins"),
    dict(name="RoofTiles", shape="scatter", n=380, heap=0.15,
         mix=[("tileshard", "tile", 0.62, 0.05, 0.13), ("tile", "tile", 0.18, 1.0, 1.0),
              ("lath", "wood_old", 0.03, 0.4, 0.9), ("chunk", "mortar", 0.17, 0.03, 0.07)],
         bed=(BED_BRICKDUST, 0.45, 0.30, 0.7), use="under burnt-out roofs, beside tenements"),
    dict(name="SlateShards", shape="scatter", n=520, heap=0.10,
         mix=[("slate", "slate", 0.88, 0.06, 0.18), ("lath", "wood_charred", 0.02, 0.4, 0.8),
              ("chunk", "mortar", 0.10, 0.03, 0.06)],
         bed=(BED_ASH, 0.40, 0.25, 0.6), use="Mitte public buildings and villas, mixed with tiles"),
    dict(name="CharredTimber", shape="radial", n=260, heap=0.35,
         mix=[("beam", "wood_charred", 0.03, 1.0, 2.0), ("plank", "wood_charred", 0.08, 0.4, 1.0),
              ("charcoal", "charcoal", 0.65, 0.03, 0.12), ("lath", "wood_charred", 0.06, 0.4, 0.9),
              ("halfbrick", "brick_dark", 0.08, 0.8, 1.0)],
         bed=(BED_ASH, 0.80, 0.45, 1.2), use="smouldering ruins, burnt plots"),
    dict(name="AshSoot", shape="radial", n=260, heap=0.05,
         mix=[("charcoal", "charcoal", 0.45, 0.02, 0.06), ("paperbit", "paper_burnt", 0.35, 0.04, 0.10),
              ("paperbit", "paper", 0.12, 0.05, 0.12), ("lath", "wood_charred", 0.03, 0.3, 0.6)],
         bed=(BED_ASH, 0.88, 0.70, 1.5), use="burn scars, fire sites, around the east fires"),
    dict(name="GlassShards", shape="scatter", n=900, heap=0.03,
         mix=[("glass", "glass", 0.80, 0.02, 0.07), ("lath", "wood_old", 0.015, 0.3, 0.6),
              ("glass", "glass", 0.15, 0.07, 0.14), ("chunk", "mortar", 0.035, 0.02, 0.04)],
         bed=(BED_GLASSDUST, 0.30, 0.25, 0.4), use="below window rows facing the street, shop fronts"),
    dict(name="SettsTornUp", shape="sett_patch", n=230, heap=0.12,
         mix=[("sett", "granite", 0.85, 0.9, 1.15), ("chunk", "granite", 0.10, 0.04, 0.09),
              ("halfbrick", "brick", 0.05, 0.8, 1.0)],
         bed=(BED_SAND, 0.75, 0.45, 1.3), use="torn-up carriageway at craters and on cobble lanes"),
    dict(name="PavingSlabs", shape="radial", n=120, heap=0.10,
         mix=[("slabshard", "paving", 0.70, 0.12, 0.30), ("slab", "paving", 0.12, 1.0, 1.0),
              ("chunk", "concrete", 0.18, 0.03, 0.08)],
         bed=(BED_SAND, 0.65, 0.40, 1.0), use="pavements beside kerbs, squares"),
    dict(name="SteelScrap", shape="radial", n=220, heap=0.30,
         mix=[("sheet", "steel_rust", 0.16, 0.25, 0.6), ("pipe", "steel_rust", 0.08, 0.5, 1.2),
              ("rebar", "steel_rust", 0.10, 0.4, 1.0), ("girder", "steel_dark", 0.04, 0.6, 1.2),
              ("sheet", "steel_dark", 0.08, 0.2, 0.5), ("chunk", "concrete", 0.30, 0.04, 0.12),
              ("chunk", "steel_rust", 0.24, 0.03, 0.08)],
         bed=(BED_RUST, 0.60, 0.40, 1.0), use="rail yard, power station, factory ruins, wrecks"),
    dict(name="SandstoneBlocks", shape="radial", n=140, heap=0.45,
         mix=[("ashlar", "sandstone", 0.30, 0.25, 0.60), ("cornice", "sandstone", 0.10, 0.6, 1.2),
              ("chunk", "sandstone", 0.45, 0.04, 0.16), ("chunk", "mortar", 0.15, 0.03, 0.08)],
         bed=(BED_SANDSTONE, 0.70, 0.45, 1.2), use="Reichstag, Brandenburg Gate, other landmarks"),
    dict(name="GritSpill", shape="radial", n=1400, heap=0.06,
         mix=[("pebble", "brick", 0.40, 0.012, 0.035), ("pebble", "mortar", 0.35, 0.010, 0.030),
              ("pebble", "concrete", 0.15, 0.010, 0.030), ("halfbrick", "brick", 0.03, 0.6, 0.9),
              ("chunk", "mortar", 0.07, 0.03, 0.06)],
         bed=(BED_BRICKDUST, 0.62, 0.55, 1.6), use="filler: thin grit to blend heaps into the ground"),
]

assert len(CELLS) == 16


def asset_name(i):
    return "MI_BerlinRubbleDecal_%02d_%s" % (i, CELLS[i]["name"])
