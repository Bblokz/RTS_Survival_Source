"""Editor side of the rear-ridge rock faces (exec'd in the throwaway editor through rear_cmd.py after rear_lib.py).

    exec(open(LIB).read()); exec(open(CLIFFS).read())
    dump_actors()                 -> Saved/IntroRear/cliffs/actors_before.json (offline rear_cliffs_props.py reads it)
    apply_terrain()               -> imports terrain/cliffs_full_height.r16 over the full grid
    apply_props()                 -> removes / re-seats the actors listed in cliffs/prop_actions.json
    make_material()               -> MI_IntroCliff (child of MI_OreCliff, desert tint) next to this map's dressing
    step_cliffs()                 -> REAR_Cliff_* instanced actors from cliffs/Intro_cliffs.json (no collision, no nav)
"""
import json

import unreal

CLIFF_DIR = 'D:/UE5Projects/RTS_Survival/Saved/IntroRear/cliffs/'
CLIFF_MI = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Materials/MI_IntroCliff'
ORE_MI = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Materials/MI_OreCliff'
FULL_GRID = (105, 75, 825, 585)
# desert retune of the Ore gneiss (Ore values: RockTint (0.74, 0.67, 0.58), DustAmount 0.75, GroundFromRVT 0.45)
INTRO_TUNE = {'vector': {'RockTint': (0.54, 0.32, 0.21, 1.0), 'DustColor': (0.5, 0.25, 0.13, 1.0), 'GroundColor': (0.46, 0.23, 0.12, 1.0)},
              'scalar': {'DustAmount': 0.9, 'GroundFromRVT': 0.6}}


def dump_actors():
    out = []
    for a in all_actors():
        loc = a.get_actor_location()
        if loc.x < 4000:
            continue
        out.append(dict(label=a.get_actor_label(), cls=a.get_class().get_name(), path=a.get_path_name(),
                        x=loc.x, y=loc.y, z=loc.z, folder=str(a.get_folder_path())))
    with open(CLIFF_DIR + 'actors_before.json', 'w') as handle:
        json.dump(out, handle)
    log('dumped %d actors' % len(out))


def apply_terrain():
    require_map()
    return import_heights('cliffs_full', FULL_GRID)


def apply_props():
    require_map()
    with open(CLIFF_DIR + 'prop_actions.json') as handle:
        acts = json.load(handle)
    by_path = {a.get_path_name(): a for a in all_actors()}
    removed = moved = missing = 0
    for item in acts:
        actor = by_path.get(item['path'])
        if actor is None:
            missing += 1
            continue
        if item['action'] == 'remove':
            actors_sub().destroy_actor(actor)
            removed += 1
        else:
            loc = actor.get_actor_location()
            actor.set_actor_location(unreal.Vector(loc.x, loc.y, loc.z + item['dz']), False, False)
            moved += 1
    log('props: removed %d, re-seated %d, missing %d' % (removed, moved, missing))


def make_material():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mi = unreal.load_asset(CLIFF_MI)
    if mi is None:
        folder, name = CLIFF_MI.rsplit('/', 1)
        mi = tools.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, unreal.load_asset(ORE_MI))
    mel = unreal.MaterialEditingLibrary
    for k, v in INTRO_TUNE['vector'].items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v))
    for k, v in INTRO_TUNE['scalar'].items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_asset(CLIFF_MI, only_if_is_dirty=False)
    log('material %s ready' % CLIFF_MI)
    return mi


def _instance_actor(label, folder, mesh, transforms, material):
    actor = actors_sub().spawn_actor_from_class(unreal.Actor, unreal.Vector(0.0, 0.0, 0.0))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    root = sds.k2_gather_subobject_data_for_instance(actor)[0]
    handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=root, new_class=unreal.HierarchicalInstancedStaticMeshComponent))
    comp = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    comp.set_static_mesh(mesh)
    comp.set_mobility(unreal.ComponentMobility.STATIC)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property('can_ever_affect_navigation', False)
    for k in range(comp.get_num_materials()):
        comp.set_material(k, material)
    comp.add_instances(transforms, False, True)
    return actor


def step_cliffs():
    require_map()
    removed = clear_prefix(PREFIX + 'Cliff_')
    mi = unreal.load_asset(CLIFF_MI) or make_material()
    with open(CLIFF_DIR + 'Intro_cliffs.json') as handle:
        data = json.load(handle)
    groups = {}
    for item in data['instances']:
        groups.setdefault((item['face'], item['piece']), []).append(item)
    total = 0
    for (face, piece), items in sorted(groups.items()):
        mesh = unreal.load_asset(data['mesh_path'].replace('<piece>', piece))
        if mesh is None:
            log('missing mesh ' + piece)
            continue
        transforms = [unreal.Transform(unreal.Vector(i['x'], i['y'], i['z']), unreal.Rotator(i['roll'], i['pitch'], i['yaw']),
                                       unreal.Vector(i['scale'], i['scale'], i['scale'])) for i in items]
        _instance_actor('%sCliff_%s_%s' % (PREFIX, face, piece), FOLDER + 'Cliffs/' + face, mesh, transforms, mi)
        total += len(items)
    log('cliffs: removed %d actors, placed %d instances in %d components' % (removed, total, len(groups)))
    return total
