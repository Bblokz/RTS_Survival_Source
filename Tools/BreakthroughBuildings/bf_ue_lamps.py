"""Lamp helpers shared by the Breakthrough building imports (the fake-light kit, SharedParts/bf_lamps.py in Blender).

The concrete instance gets each bulb as BF_LampSource<k> (xyz in actor cm, A = pool radius), so the concrete shader paints the
lamps' light on the building. Ground pools under the lamps are M_BF_LampPoolDecal decals: the building's Blueprint places one
per lamp from the json's 'ground' spot and radius, projecting down, with the building mesh set not to receive decals (its
concrete already paints its own pools).
"""
import json

import unreal

SHARED = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared'
MATERIALS = dict(M_BF_LampGlass=SHARED + '/M_BF_LampGlass', M_BF_LampBeacon=SHARED + '/M_BF_LampBeacon')
MAX_LAMPS = 8


def apply_lamps(instance, json_path):
    lamps = [row for row in json.loads(open(json_path, encoding='utf-8').read())['lamps'] if row['pool_radius'] > 0]
    if len(lamps) > MAX_LAMPS:
        raise RuntimeError(f'{len(lamps)} lamp pools, the shader takes {MAX_LAMPS}')
    edit = unreal.MaterialEditingLibrary
    # the master compiles the lamp-pool code only into instances that switch it on
    edit.set_material_instance_static_switch_parameter_value(instance, 'BF_UseLampPools', bool(lamps))
    for k in range(MAX_LAMPS):
        row = lamps[k] if k < len(lamps) else dict(bulb=[0, 0, 0], pool_radius=0)
        edit.set_material_instance_vector_parameter_value(instance, f'BF_LampSource{k}', unreal.LinearColor(*row['bulb'], row['pool_radius']))
    edit.update_material_instance(instance)
    return len(lamps)
