"""Writes deploy_fx.json for RTS.ImpactVfx.BuildFromRecipe: the nomadic deploy dust systems (NomadicDeploy/FX).
Reuses the howitzer recipe helpers (Source/Tools/ImpactVfx/make_recipe.py): stateless, lit, normal-mapped MsvFx smoke
puffs (masked, cast shadows), authored at real size in cm; sprite size follows the user parameter ImpactSpriteScale
(component scale only moves them). Systems go to the VAT prototype folder.
Usage: python deploy_fx_recipe.py [out.json]
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'ImpactVfx'))
import make_recipe as mr  # noqa: E402

FOLDER = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/FX'
# desert dust, dark and saturated on purpose (the puff sheet is light grey under strong sun, see make_recipe.py)
DUST = (0.46, 0.29, 0.14)
ROLL = (0.33, 0.21, 0.11)
# translucent copy of the pack's M_Smoke without its DitherTemporalAA opacity mask (dither grain looked bad) and with a
# depth fade where the sprites meet the ground; made by hand 2026-11-11 in NomadicDeploy/FX
DEPLOY_DUST = FOLDER + '/MI_Deploy_Dust.MI_Deploy_Dust'


def layer(name, spawn, lifetime, size, tint, velocity, drag, size_curve, alpha_curve, sphere, offset_z, shadows=True,
          bounds_half_width=500, bounds_above=700):
    return mr.stateless_layer(name, DEPLOY_DUST, (6, 5), 30, spawn, lifetime, size, mr.scaled(tint, 0.85),
                              mr.scaled(tint, 1.0), velocity, drag, size_curve, alpha_curve, sphere=sphere,
                              offset_z=offset_z, shadows=shadows, bounds_half_width=bounds_half_width,
                              bounds_above=bounds_above)


def base_recipe(name):
    return {'target': f'{FOLDER}/{name}', 'base': mr.FOUNTAIN, 'add': [], 'disable': ['Fountain'], 'stateless': []}


def land_dust():
    """Small landing puff (planks, sandbags, props): a short lit puff and a low roll that spreads along the ground."""
    r = base_recipe('NS_Deploy_LandDust')
    r['stateless'] = [
        layer('puff', [(0.0, 4, 5)], (1.3, 1.9), (90, 140), DUST, ('cone', 55, 110, 210), 2.4,
              [(0, 0.4), (0.3, 1.0), (1, 1.6)], [(0, 0), (0.05, 0.9), (0.6, 0.7), (1, 0)], 30, 15),
        layer('roll', [(0.0, 7, 8)], (1.2, 1.8), (70, 110), ROLL, ('point', 170, 270, -25), 2.8,
              [(0, 0.5), (0.4, 1.2), (1, 1.7)], [(0, 0), (0.06, 0.85), (0.5, 0.6), (1, 0)], 30, 10, shadows=False,
              bounds_above=300),
    ]
    return r


def heavy_dust():
    """Containers set down / the mast plants: a bigger lit mass, a wide ground roll and dust that hangs."""
    r = base_recipe('NS_Deploy_HeavyDust')
    r['stateless'] = [
        layer('mass', [(0.0, 6, 7)], (2.0, 2.8), (220, 320), DUST, ('cone', 40, 200, 320), 2.0,
              [(0, 0.45), (0.3, 1.0), (1, 1.4)], [(0, 0), (0.04, 1), (0.65, 0.85), (1, 0)], 60, 40,
              bounds_half_width=900, bounds_above=1200),
        layer('roll', [(0.02, 10, 12)], (1.8, 2.6), (160, 230), ROLL, ('point', 360, 520, -40), 2.6,
              [(0, 0.5), (0.4, 1.3), (1, 1.9)], [(0, 0), (0.06, 1), (0.55, 0.8), (1, 0)], 70, 20, shadows=False,
              bounds_half_width=900, bounds_above=400),
        layer('billow', [(0.2, 4, 5)], (2.8, 3.8), (260, 360), DUST, ('cone', 60, 60, 140), 1.2,
              [(0, 0.6), (0.5, 1.2), (1, 1.6)], [(0, 0), (0.1, 0.8), (0.6, 0.7), (1, 0)], 140, 60,
              bounds_half_width=900, bounds_above=1200),
    ]
    return r


GLOW = '/Game/RTS_Survival/_Prototypes/DeployAnim/VAT/FX/M_DeploySpark.M_DeploySpark'  # unlit additive: particle colour x radial dot (MI_Glow_3 drew nothing on stateless sprites)


def sparks():
    """Metal set down hard: a short burst of hot dots that arc and fall (gone within a second) and a small puff.
    Own stateless layer: the tank explosion's GPU sparks lived for many seconds and carpeted the ground."""
    r = base_recipe('NS_Deploy_Sparks')
    hot = mr.stateless_layer('spark', GLOW, (1, 1), 1, [(0.0, 40, 50)], (0.4, 0.9), (32, 50), (6.0, 2.6, 0.6, 1.0),
                             (9.0, 4.0, 1.0, 1.0), ('cone', 70, 280, 620), 0.6, [(0, 1.0), (1, 0.4)],
                             [(0, 1), (0.7, 0.9), (1, 0)], sphere=15, offset_z=15, shadows=False,
                             bounds_half_width=500, bounds_above=500)
    hot['modules']['GravityForce'] = {'bModuleEnabled': 'True'}
    hot['modules']['SubUVAnimation'] = {'bModuleEnabled': 'False'}
    del hot['renderer']['SubImageSize']
    hot['renderer']['bSubImageBlend'] = 'False'
    r['stateless'] = [
        hot,
        layer('puff', [(0.0, 3, 4)], (1.0, 1.5), (70, 110), (0.3, 0.26, 0.22), ('cone', 60, 90, 170), 2.6,
              [(0, 0.4), (0.3, 1.0), (1, 1.5)], [(0, 0), (0.05, 0.8), (0.6, 0.6), (1, 0)], 25, 20),
    ]
    return r


def exhaust():
    """One diesel puff from the truck's exhaust: small, dark, rises and spreads (played every ~0.35 s)."""
    r = base_recipe('NS_Deploy_Exhaust')
    r['stateless'] = [
        layer('puff', [(0.0, 2, 3)], (1.8, 2.6), (120, 180), (0.16, 0.15, 0.14), ('cone', 25, 150, 240), 1.4,
              [(0, 0.5), (0.4, 1.4), (1, 2.6)], [(0, 0), (0.05, 0.85), (0.5, 0.6), (1, 0)], 10, 0,
              bounds_half_width=300, bounds_above=800),
    ]
    return r


def main(argv):
    out = Path(argv[1]) if len(argv) > 1 else Path(__file__).with_name('deploy_fx.json')
    systems = [land_dust(), heavy_dust()]  # the smoke screen setup uses heavy dust (+ NS_DestructionSmoke)
    out.write_text(json.dumps({'systems': systems}, indent=2) + '\n', encoding='utf-8')
    print(f'wrote {out} ({len(systems)} systems)')


if __name__ == '__main__':
    main(sys.argv)
