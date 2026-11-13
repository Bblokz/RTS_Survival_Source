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
    'RuinWallTile': 1.0,              # UV0 multiplier for the wall textures (1.0 = 4 m tile)
    'RuinRubbleTile': 1.333,          # UV0 multiplier for the rubble texture (1.333 = 3 m tile)
    'RuinPlasterLoss': 0.0,           # added to vertex G before the plaster threshold (-1..1)
    'RuinPlasterEdgeWidth': 0.04,     # width of the plaster->brick transition in chip-height units (sharp ~0.02-0.08)
    'RuinSootAmountFallback': 1.0,    # soot amount when CPD 3 is unset (0)
    'RuinSootStrength': 1.0,          # soot = vertex R * amount * (strength + breakup * (noise - 0.5))
    'RuinSootBreakup': 0.9,
    'RuinDustStrength': 1.0,          # dust = vertex B * (strength + breakup * (0.5 - noise))
    'RuinDustBreakup': 0.8,
    'RuinDustOpacity': 0.75,
    # Rubble branch only.
    'RuinRubbleDesaturation': 0.20,   # 0 = texture colour, 1 = grey; used when CPD 4 is unset (0)
    'RuinRubbleContrast': 0.90,       # 1 = texture contrast; lower flattens the fragments toward RuinRubblePivot
    'RuinRubblePivot': 0.13,          # linear grey the contrast turns about (about the rubble texture's mean)
    # Ruin_CloseUpDetail branch only (off by default: none of it shows from the game camera).
    'RuinPlasterEdgeLight': 0.22,     # brightening of the broken plaster rim (fresh lime core)
    'RuinPlasterEdgeShadow': 0.30,    # darkening of brick just below the plaster edge (plaster thickness)
    'RuinNormalStrength': 1.0,        # scales the packed normal XY
    # Only built when the master is not Fully Rough (ruin_material_build.MASTER_FLAGS); it is, so these are unused.
    'RuinRoughness': 0.90,            # one roughness for wall and rubble
    'RuinPlasterRoughness': 0.90,     # the next three: roughness by cavity in the Ruin_CloseUpDetail branch
    'RuinBrickRoughFace': 0.80,       # brick face roughness (brick A = 1)
    'RuinBrickRoughJoint': 0.97,      # mortar joint roughness (brick A = 0)
}
# Parameters that only exist in the Ruin_CloseUpDetail branch (the docs and the preview read this).
CLOSE_UP_SCALARS = ('RuinPlasterEdgeLight', 'RuinPlasterEdgeShadow', 'RuinNormalStrength')
ROUGHNESS_SCALARS = ('RuinRoughness', 'RuinPlasterRoughness', 'RuinBrickRoughFace', 'RuinBrickRoughJoint')

# Vector parameters (linear RGB): name -> default.
VECTORS = {
    'RuinTintFallback': (1.0, 1.0, 1.0),   # per-actor tint when CPD 0-2 are all unset (0)
    'RuinPlasterColor': (1.0, 1.0, 1.0),   # multiplier on the plaster texture
    'RuinBrickColor': (1.0, 1.0, 1.0),     # multiplier on the brick texture
    'RuinRubbleColor': (1.0, 1.0, 1.0),    # multiplier on the rubble texture (after desaturation and contrast)
    'RuinSootColor': (0.075, 0.068, 0.064),  # multiplier at full soot (keeps some texture)
    'RuinDustColor': (0.235, 0.212, 0.180),  # linear; sRGB ~ (0.52, 0.50, 0.47)
}

# Custom primitive data (the hole system owns 8-34 and never writes below 8; 5-7 are free).
CPD_TINT_FIRST = 0     # 0,1,2 tint RGB multiplier; all three 0 (unset) -> RuinTintFallback
CPD_SOOT = 3           # soot amount; 0 (unset) -> RuinSootAmountFallback; negative -> no soot
CPD_DESATURATION = 4   # rubble desaturation 0..1; 0 (unset) -> RuinRubbleDesaturation; negative -> none

STATIC_SWITCH = 'Ruin_IsRubble'
CLOSE_UP_SWITCH = 'Ruin_CloseUpDetail'     # normal map, plaster rim light / shadow
STATIC_SWITCHES = (STATIC_SWITCH, CLOSE_UP_SWITCH)

# Instances: name -> (parent asset name, scalar overrides, vector overrides, static switch values).
# Every switch is written on every instance of the master, so the hole instance can never drift.
INSTANCES = {
    'MI_BerlinRuin_Wall': ('M_BerlinRuin', {}, {}, {STATIC_SWITCH: False, CLOSE_UP_SWITCH: False}),
    'MI_BerlinRuin_Rubble': ('M_BerlinRuin', {}, {}, {STATIC_SWITCH: True, CLOSE_UP_SWITCH: False}),
    # Footprint skirts (berlin_map_build.step_feet): darker, thinner dust, or the skirts read as pale rings from above.
    'MI_BerlinRuin_Rubble_Foot': ('MI_BerlinRuin_Rubble', {'RuinDustOpacity': 0.6},
                                  {'RuinDustColor': (0.115, 0.102, 0.090)}, {}),
}

# Hole instance (made by make_wall_holes_material.make_holes_instance from MI_BerlinRuin_Wall).
HOLES_INSTANCE = 'MI_BerlinRuin_Wall_Holes'
HOLES_INNER_DARKEN = 0.65
HOLES_INNER_COLOR = (0.085, 0.060, 0.050)   # dark sooty brick seen through the hole
