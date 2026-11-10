"""Writes the howitzer impact recipe (howitzer_impacts.json) for RTS.ImpactVfx.BuildFromRecipe.

The classic layers (flash, fireball, sparks, debris, clods) come from the pack systems; the volume layers (dark dirt
column, lit dust mass, billowing dust, ground roll) are stateless emitters copied from the engine's FountainLightweight
template and set up from the specs below. Stateless properties are written as Unreal import text, which is what the
builder imports by reflection.

Usage: python make_recipe.py [output.json] [--only Sand,Stone] [--target-suffix _Dev]
"""
import json
import sys
from pathlib import Path

HOWITZER = '/Game/RTS_Survival/ImpactSfxVfx/5Artillery/Howitzer'
ALL_EXPLOSIONS = '/Game/RTS_Survival/SpecialEffects/AllExplosions/Niagara'
BIG_8 = f'{ALL_EXPLOSIONS}/Big/NS_Explosion_Big_8'
SAND_8 = f'{ALL_EXPLOSIONS}/Sand/NS_Explosion_Sand_8'
PACK_MATERIALS = '/Game/MsvFx_Niagara_Explosion_Pack_01/Resources/Materials/InstanceMTL'
TANK_EXPLOSION = '/Game/RTS_Survival/SpecialEffects/AA_OptimExplosions/NS_Expl_Tank_1'
FOUNTAIN = '/Niagara/DefaultAssets/Templates/Systems/FountainLightweight.FountainLightweight'


# ---- import text helpers -------------------------------------------------------------------------------------------

def num(value):
    return f'{float(value):.6g}'


def float_range(low, high=None):
    high = low if high is None else high
    mode = 'UniformConstant' if low == high else 'UniformRange'
    channels = num(low) if low == high else f'{num(low)},{num(high)}'
    return f'(Mode={mode},ChannelConstantsAndRanges=({channels}),Min={num(low)},Max={num(high)})'


def int_range(low, high=None):
    high = low if high is None else high
    mode = 'UniformConstant' if low == high else 'UniformRange'
    return f'(Mode={mode},Min={int(low)},Max={int(high)})'


def vec2(value):
    return f'(X={num(value)},Y={num(value)})'


def uniform_vector2_range(low, high):
    return (f'(Mode=UniformRange,ChannelConstantsAndRanges=({num(low)},{num(high)}),'
            f'Min={vec2(low)},Max={vec2(high)})')


def vec3(xyz):
    return f'(X={num(xyz[0])},Y={num(xyz[1])},Z={num(xyz[2])})'


def vector3_constant(xyz):
    channels = ','.join(num(v) for v in xyz)
    return f'(Mode=NonUniformConstant,ChannelConstantsAndRanges=({channels}),Min={vec3(xyz)},Max={vec3(xyz)})'


def color(rgba):
    return f'(R={num(rgba[0])},G={num(rgba[1])},B={num(rgba[2])},A={num(rgba[3])})'


def color_range(low, high):
    channels = ','.join(num(v) for v in list(low) + list(high))
    return f'(Mode=NonUniformRange,ChannelConstantsAndRanges=({channels}),Min={color(low)},Max={color(high)})'


def rich_curve(keys):
    key_text = ','.join(f'(Time={num(t)},Value={num(v)})' for t, v in keys)
    return f'(Keys=({key_text}))'


def uniform_curve(keys):
    """One curve for every channel (ScaleSpriteSize)."""
    return f'(Mode=UniformCurve,ChannelCurves=({rich_curve(keys)}))'


def color_curve(rgb_keys, alpha_keys):
    """ScaleColor over life: one curve for RGB (copied to R, G and B) and one for alpha."""
    curves = [rich_curve(rgb_keys)] * 3 + [rich_curve(alpha_keys)]
    return f'(Mode=NonUniformCurve,ChannelCurves=({",".join(curves)}))'


def bounds(half_width, below, above):
    return (f'(Min=(X={num(-half_width)},Y={num(-half_width)},Z={num(-below)}),'
            f'Max=(X={num(half_width)},Y={num(half_width)},Z={num(above)}),IsValid=True)')


def bursts(*entries):
    """entries: (spawn time, min count, max count)."""
    infos = ','.join(f'(Type=Burst,SpawnTime={num(t)},Amount={int_range(lo, hi)},bEnabled=True)' for t, lo, hi in entries)
    return f'({infos})'


def material(name, folder=PACK_MATERIALS):
    return f'{folder}/{name}.{name}'


# ---- layer specs ---------------------------------------------------------------------------------------------------
# Sizes are authored at the AllExplosions pack's scale (about 5 m at scale 1); the weapon's ImpactScale scales the
# whole component (2.2 - 3.1 on the howitzers), stateless layers included (they simulate in local space).
# Why these layers: from the -70 degree RTS camera a wide radial spray projects onto the ground as a flat star. What
# reads as volume is a compact mass that rises, is lit by the sun (bright top, shaded side), is darker at its base and
# throws a shadow beside the crater. All four layers use the MsvFx pack's lit, normal-mapped flipbooks; the masked
# ones (dust mass, billow, ground roll) add no blended overdraw and cast shadows.

PYRO_DUST = material('MI_Pyro_Dust_01')     # pyro-sim dust (8x10); unused: its data texture is capped at 512 and renders blocky
SMOKE_PUFF = material('MI_Smoke_01')        # billowing smoke puff, masked + lit with normals, 6x5


def stateless_layer(name, material_path, sub_image, frames, spawn, lifetime, size, color_low, color_high,
                    velocity, drag, size_curve, alpha_curve, sphere=40.0, rotation=(0, 360), offset_z=0.0,
                    pivot=(0.5, 0.5), shadows=True, bounds_half_width=900, bounds_above=1600, sort=False):
    """One stateless sprite layer. velocity: ('cone', angle, min, max) or ('point', min, max, origin_z)."""
    if velocity[0] == 'cone':
        velocity_module = {'VelocityType': 'InCone', 'ConeAngle': num(velocity[1]),
                           'ConeRotation': '(Pitch=0,Yaw=0,Roll=0)',
                           'ConeVelocityDistribution': float_range(velocity[2], velocity[3])}
    else:
        velocity_module = {'VelocityType': 'FromPoint', 'PointOrigin': vec3((0, 0, velocity[3])),
                           'PointVelocityDistribution': float_range(velocity[1], velocity[2])}
    velocity_module['CoordinateSpace'] = 'Local'
    last_spawn = max(entry[0] for entry in spawn)
    return {
        'name': name,
        'from': FOUNTAIN,
        'emitter': 'Fountain',
        'sprite_scale_parameter': 'ImpactSpriteScale',
        'emitter_properties': {
            'EmitterState': f'(LoopBehavior=Once,LoopDuration={float_range(last_spawn + lifetime[1] + 0.25)})',
            'SpawnInfos': bursts(*spawn),
            'FixedBounds': bounds(bounds_half_width, 300, bounds_above),
        },
        'modules': {
            'InitializeParticle': {
                'LifetimeDistribution': float_range(*lifetime),
                'SpriteSizeDistribution': uniform_vector2_range(*size),
                'SpriteRotationDistribution': float_range(*rotation),
                'ColorDistribution': color_range(color_low, color_high),
                'InitialPositionDistribution': vector3_constant((0, 0, offset_z)),
            },
            'ShapeLocation': {'ShapePrimitive': 'Sphere', 'SphereMin': '0', 'SphereMax': num(sphere)},
            'AddVelocity': velocity_module,
            'Drag': {'DragDistribution': float_range(drag)},
            'GravityForce': {'bModuleEnabled': 'False'},
            'ScaleColor': {'bModuleEnabled': 'True', 'ScaleDistribution': color_curve([(0, 1), (1, 1)], alpha_curve)},
            'ScaleSpriteSize': {'bModuleEnabled': 'True', 'ScaleDistribution': uniform_curve(size_curve)},
            'SubUVAnimation': {'bModuleEnabled': 'True', 'NumFrames': str(frames), 'AnimationMode': 'Linear'},
        },
        'renderer': {
            'Material': material_path,
            'SubImageSize': f'(X={sub_image[0]},Y={sub_image[1]})',
            'bSubImageBlend': 'True',
            'Alignment': 'Unaligned',
            'FacingMode': 'FaceCamera',
            'PivotInUVSpace': f'(X={num(pivot[0])},Y={num(pivot[1])})',
            'bCastShadows': 'True' if shadows else 'False',
            'SortMode': 'ViewDistance' if sort else 'None',
        },
    }


def scaled(rgb, factor, alpha=1.0):
    return tuple(min(1.0, c * factor) for c in rgb) + (alpha,)


def volume_layers(palette):
    """palette: base rgb tint per layer (multiplied with the light grey puff sheet, so the tints are dark)."""
    column, dust, billow, roll = palette['column'], palette['dust'], palette['billow'], palette['roll']
    return [
        # The dark column: narrow and fast, so the eruption stands tall before it spreads; darkest layer, so from
        # above it reads as the dense core of the blast.
        stateless_layer('dirt_column', SMOKE_PUFF, (6, 5), 30, [(0.0, 6, 7), (0.08, 3, 4)], (1.9, 2.6), (190, 290),
                        scaled(column, 0.85), scaled(column, 1.0), ('cone', 9, 950, 1450), 2.1,
                        [(0, 0.4), (0.25, 1.0), (1, 1.5)], [(0, 0), (0.03, 1), (0.6, 0.9), (1, 0)], sphere=40,
                        offset_z=40, bounds_above=1800),
        # The lit dust eruption: a cluster of normal-lit puffs thrown up fast that slows, swells and hangs; the sun
        # shades one side of it. (The pack's pyro-sim dust is capped at a 512 texture, so it renders blocky here.)
        stateless_layer('dust_mass', SMOKE_PUFF, (6, 5), 30, [(0.03, 7, 8)], (2.6, 3.4), (330, 460),
                        scaled(dust, 0.85), scaled(dust, 1.0), ('cone', 30, 380, 640), 1.6,
                        [(0, 0.45), (0.3, 1.0), (1, 1.35)], [(0, 0), (0.04, 1), (0.7, 0.95), (1, 0)], sphere=60,
                        offset_z=60),
        # Dust that keeps billowing up after the burst and hangs: grows, drifts up, thins out.
        stateless_layer('dust_billow', SMOKE_PUFF, (6, 5), 30, [(0.25, 6, 7), (0.6, 3, 4)], (3.6, 4.8), (340, 480),
                        scaled(billow, 0.8), scaled(billow, 1.0), ('cone', 60, 90, 210), 1.1,
                        [(0, 0.6), (0.5, 1.2), (1, 1.6)], [(0, 0), (0.08, 1), (0.6, 0.9), (1, 0)], sphere=170,
                        offset_z=80),
        # Dark dust rolling out low along the ground: the dark base that makes the mass above read as tall.
        stateless_layer('ground_roll', SMOKE_PUFF, (6, 5), 30, [(0.05, 10, 12)], (2.4, 3.4), (220, 320),
                        scaled(roll, 0.85), scaled(roll, 1.0), ('point', 520, 760, -40), 2.6,
                        [(0, 0.5), (0.4, 1.3), (1, 1.9)], [(0, 0), (0.06, 1), (0.55, 0.85), (1, 0)], sphere=70,
                        offset_z=30, bounds_above=700, shadows=False),
    ]


# Tints multiply the light grey puff sheet under strong sun and a glossy default roughness, so muted mid browns wash
# out to grey: these are dark and saturated on purpose.
PALETTES = {
    'Sand': {'column': (0.22, 0.12, 0.06), 'dust': (0.46, 0.27, 0.12), 'billow': (0.45, 0.27, 0.13),
             'roll': (0.32, 0.19, 0.1)},
    'Stone': {'column': (0.2, 0.2, 0.2), 'dust': (0.4, 0.39, 0.37), 'billow': (0.38, 0.37, 0.36),
              'roll': (0.28, 0.28, 0.27)},
    'Regular': {'column': (0.1, 0.09, 0.08), 'dust': (0.25, 0.2, 0.16), 'billow': (0.14, 0.13, 0.12),
                'roll': (0.2, 0.16, 0.12)},
}

# Classic layers kept from the pack systems. The pack's wide radial dirt rays and its flat dust / smoke sheets
# (main, main001, smoke, smoke001, wisp, dust) are what made the old impacts read flat; they are left out.
SAND_8_DROPPED = ['refr_sprite', 'refr_mesh', 'main', 'main001', 'smoke', 'smoke001', 'wisp', 'dust']


# ---- systems -------------------------------------------------------------------------------------------------------

def ground_impact(surface, materials=None):
    recipe = {
        'target': f'{HOWITZER}/NS_HowitzerImpact_{surface}',
        'base': SAND_8,
        'add': [],
        'stateless': volume_layers(PALETTES[surface]),
        'disable': SAND_8_DROPPED,
        'spawn_scale': {'spark': 0.5, 'spark_l': 0.5, 'spark001': 0.5},
        'user': {'LifetimeMult': 1.3},
    }
    if materials:
        recipe['materials'] = materials
    return recipe


def sand():
    return ground_impact('Sand')


def stone():
    return ground_impact('Stone', {
        '/Game/RTS_Survival/SpecialEffects/AllExplosions/Materials/MI_Explosion_Sand_11.MI_Explosion_Sand_11':
            f'{HOWITZER}/Stone/MI_HowitzerStoneDust_1.MI_HowitzerStoneDust_1',
        '/Game/RTS_Survival/SpecialEffects/AllExplosions/Materials/MI_Explosion_Sand_12.MI_Explosion_Sand_12':
            f'{HOWITZER}/Stone/MI_HowitzerStoneDust_2.MI_HowitzerStoneDust_2',
    })


def regular():
    """Metal / flesh / air: the tank explosion's fireball, sparks and debris (NS_Expl_Tank_1, which the game state
    uses for vehicle deaths) with dark volume layers and the pack's dirt clods."""
    return {
        'target': f'{HOWITZER}/NS_HowitzerImpact_Regular',
        'base': TANK_EXPLOSION,
        'add': [{'from': SAND_8, 'emitter': 'dirt', 'name': 'dirt_clods'}],
        'stateless': volume_layers(PALETTES['Regular']),
        'disable': ['refr_mesh'],
        'spawn_scale': {'spark': 0.4, 'spark_l': 0.5, 'debris': 0.35},
    }


SYSTEMS = {'Regular': regular, 'Sand': sand, 'Stone': stone}


def main(argv):
    out = Path(argv[1]) if len(argv) > 1 and not argv[1].startswith('--') else Path(__file__).with_name('howitzer_impacts.json')
    only = None
    suffix = ''
    for index, arg in enumerate(argv):
        if arg == '--only':
            only = argv[index + 1].split(',')
        if arg == '--target-suffix':
            suffix = argv[index + 1]
    systems = []
    for name, build in SYSTEMS.items():
        if only and name not in only:
            continue
        recipe = build()
        recipe['target'] += suffix
        systems.append(recipe)
    out.write_text(json.dumps({'systems': systems}, indent=2) + '\n', encoding='utf-8')
    print(f'wrote {out} ({len(systems)} systems)')


if __name__ == '__main__':
    main(sys.argv)
