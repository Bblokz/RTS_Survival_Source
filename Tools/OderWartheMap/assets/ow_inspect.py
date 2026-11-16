"""Read-only inspection for the Oder-Warthe kit: the crushable dead spruce template, its tree material.
Headless: UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this> -unattended -nullrhi
"""
import json

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/assets/inspect_spruce.json'
BP = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/DeadSpruces/BP_PolesieDeadSpruce_01_TallSentinel'
_SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
_SDL = unreal.SubobjectDataBlueprintFunctionLibrary
rep = {}
bp = unreal.load_asset(BP)
gc = bp.generated_class()
chain = []
for attempt in ('parent',):
    try:
        chain.append(str(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp)))
    except Exception as e:
        chain.append('ERR ' + str(e)[:100])
try:
    c = gc
    for _ in range(8):
        c = c.get_super_class() if hasattr(c, 'get_super_class') else None
        if c is None:
            break
        chain.append(c.get_path_name())
except Exception as e:
    chain.append('ERR2 ' + str(e)[:100])
rep['parent'] = chain
cdo0 = unreal.get_default_object(gc)
chain.append('isWpo=%s isDEA=%s' % (isinstance(cdo0, unreal.WpoTree), isinstance(cdo0, unreal.DestructableEnvActor)))
cdo = unreal.get_default_object(gc)
props = {}
for p in ('TankDestructionType', 'TreeMesh', 'bCanBeCrushed', 'Health', 'DestructionType', 'tank_destruction_type', 'tree_mesh'):
    try:
        props[p] = str(cdo.get_editor_property(p))
    except Exception as e:  # noqa: BLE001
        props[p] = 'ERR ' + str(e)[:80]
rep['cdo'] = props
comps = {}
for h in _SDS.k2_gather_subobject_data_for_blueprint(bp):
    o = _SDL.get_object_for_blueprint(_SDL.get_data(h), bp)
    if o is None or not isinstance(o, unreal.ActorComponent):
        continue
    d = dict(cls=o.get_class().get_name())
    if isinstance(o, unreal.PrimitiveComponent):
        d['profile'] = str(o.get_collision_profile_name())
        d['coll'] = str(o.get_collision_enabled())
    if isinstance(o, unreal.StaticMeshComponent):
        sm = o.get_editor_property('static_mesh')
        d['mesh'] = sm.get_path_name() if sm else None
        d['override_materials'] = [m.get_path_name() if m else None for m in o.get_editor_property('override_materials')]
        if sm:
            b = sm.get_bounding_box()
            body = sm.get_editor_property('body_setup')
            ag = body.get_editor_property('agg_geom')
            d['mesh_info'] = dict(lods=sm.get_num_lods(), tris=[sm.get_num_triangles(i) for i in range(sm.get_num_lods())],
                                  min=[round(b.min.x), round(b.min.y), round(b.min.z)],
                                  max=[round(b.max.x), round(b.max.y), round(b.max.z)],
                                  convex=len(ag.get_editor_property('convex_elems')),
                                  boxes=len(ag.get_editor_property('box_elems')),
                                  mats=[sm.get_material(i).get_path_name() for i in range(len(sm.get_editor_property('static_materials')))])
    comps[o.get_name()] = d
rep['components'] = comps
mi = unreal.load_asset('/Game/RTS_Survival/Meshes/Nature/Trees/DeadTrees/Materials/MI_Env_DeadTree_1')
if mi:
    rep['tree_mi_parent'] = mi.get_editor_property('parent').get_path_name()
    rep['tree_mi_scalars'] = [str(p.parameter_info.name) for p in mi.get_editor_property('scalar_parameter_values')]
    rep['tree_mi_textures'] = [(str(p.parameter_info.name), p.parameter_value.get_path_name() if p.parameter_value else None)
                               for p in mi.get_editor_property('texture_parameter_values')]
# full property dump of the CDO class chain fields that look relevant
names = []
for cls_path in (rep['parent'],):
    pass
with open(OUT, 'w') as f:
    json.dump(rep, f, indent=1)
print('OW_INSPECT_DONE')
