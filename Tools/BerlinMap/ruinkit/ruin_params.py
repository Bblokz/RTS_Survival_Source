"""Shared parameter defaults for the Berlin ruin kit material (M_BerlinRuin).

Imported by ruin_textures_gen.py (the numpy preview simulates the same shader maths) and by
ruin_material_build.py (the Unreal builder writes these as the master's parameter defaults).
Docs: Source/Docs/Environment/BerlinRuinKitMaterial.md
"""

# Texture asset names (one PNG each in Saved/Berlin/ruinkit_textures/).
#   _D  : RGB albedo (sRGB) + A linear data (Plaster: chip height, Brick: cavity, Rubble: height)
#   _N  : RG brick normal XY, BA plaster normal XY (linear, DirectX green, 0.5 = flat)
TEXTURES = {
    'T_BerlinRuin_Plaster_D': {'srgb': True},
    'T_BerlinRuin_Brick_D': {'srgb': True},
    'T_BerlinRuin_N': {'srgb': False},
    'T_BerlinRuin_Rubble_D': {'srgb': True},
}

# One texture tile = TILE_M metres of wall (UV0 is 1 unit = 4 m, so tile scale 1.0 = 4 m).
TILE_M = 4.0
BRICK_COURSES_PER_TILE = 52        # 4 m / 52 = 7.69 cm per course incl. joint
BRICK_STRETCHERS_PER_TILE = 16     # 25 cm stretcher incl. joint, 12.5 cm header

# Scalar parameters: name -> default.
SCALARS = {
    'RuinWallTile': 1.0,              # UV0 multiplier for all three wall textures (1.0 = 4 m tile)
    'RuinRubbleTile': 1.333,          # UV0 multiplier for the rubble texture (1.333 = 3 m tile)
    'RuinPlasterLoss': 0.0,           # added to vertex G before the plaster threshold (-1..1)
    'RuinPlasterEdgeWidth': 0.04,     # width of the plaster->brick transition in chip-height units (sharp ~0.02-0.08)
    'RuinPlasterEdgeLight': 0.22,     # brightening of the broken plaster rim (fresh lime core)
    'RuinPlasterEdgeShadow': 0.30,    # darkening of brick just below the plaster edge (plaster thickness)
    'RuinPlasterRoughness': 0.90,
    'RuinBrickRoughFace': 0.80,       # brick face roughness (brick A = 1)
    'RuinBrickRoughJoint': 0.97,      # mortar joint roughness (brick A = 0)
    'RuinNormalStrength': 1.0,        # scales the packed normal XY
    'RuinSootAmountFallback': 1.0,    # soot amount when CPD 3 is unset (0)
    'RuinSootStrength': 1.0,          # soot = vertex R * amount * (strength + breakup * (noise - 0.5))
    'RuinSootBreakup': 0.9,
    'RuinMatteRoughness': 0.97,       # roughness under soot and dust
    'RuinDustStrength': 1.0,          # dust = vertex B * (strength + breakup * (0.5 - noise))
    'RuinDustBreakup': 0.8,
    'RuinDustOpacity': 0.75,
    'RuinRubbleRoughness': 0.90,
}

# Vector parameters (linear RGB): name -> default.
VECTORS = {
    'RuinTintFallback': (1.0, 1.0, 1.0),   # per-actor tint when CPD 0-2 are all unset (0)
    'RuinPlasterColor': (1.0, 1.0, 1.0),   # multiplier on the plaster texture
    'RuinBrickColor': (1.0, 1.0, 1.0),     # multiplier on the brick texture
    'RuinRubbleColor': (1.0, 1.0, 1.0),    # multiplier on the rubble texture
    'RuinSootColor': (0.075, 0.068, 0.064),  # multiplier at full soot (keeps some texture)
    'RuinDustColor': (0.235, 0.212, 0.180),  # linear; sRGB ~ (0.52, 0.50, 0.47)
}

# Custom primitive data (the hole system owns 8-34).
CPD_TINT_FIRST = 0     # 0,1,2 tint RGB multiplier; all three 0 (unset) -> RuinTintFallback
CPD_SOOT = 3           # soot amount; 0 (unset) -> RuinSootAmountFallback; negative -> no soot

STATIC_SWITCH = 'Ruin_IsRubble'

# Instances: name -> (scalar overrides, vector overrides, static switch value)
INSTANCES = {
    'MI_BerlinRuin_Wall': ({}, {}, False),
    'MI_BerlinRuin_Rubble': ({}, {}, True),
}

# Hole instance (made by make_wall_holes_material.make_holes_instance from MI_BerlinRuin_Wall).
HOLES_INSTANCE = 'MI_BerlinRuin_Wall_Holes'
HOLES_INNER_DARKEN = 0.65
HOLES_INNER_COLOR = (0.085, 0.060, 0.050)   # dark sooty brick seen through the hole
