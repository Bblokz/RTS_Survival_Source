"""-ExecutePythonScript in a throwaway DebugGame editor opened on Ger_IntroMission (2026-11-18): applies the base ridge
rework from ridge_gen.py build, the Rus building death-spawn skips and the fuel wreck lifetime, then saves.

1. Checks the landscape still matches the export the plan was made from (aborts otherwise: re-run the probe).
2. Imports terrain/ridge_height.r16 over the changed rectangle (heights only; the extension's paint already matches).
3. Moves / removes the props in Saved/RidgeRework/prop_actions.json (by label).
4. Places the cliff kit as RIDGE_Cliff_* instanced actors (folder IntroRidge/Cliffs, MI_IntroCliff, no collision/nav).
5. Rus_Building_Barracks / _CoolingTowers: DeathSpawnClassesToSkip = their loot pile; BP_Dest_RusFuelLarge: 10 s life.
6. Rebuilds navigation and saves the map (backup + size check) and the three Blueprints.
Log lines start with RIDGEAPPLY.
"""
import json
import os
import shutil
import sys
import traceback

from array import array

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/rear')
import rear_lib  # noqa: E402

OUT = 'D:/UE5Projects/RTS_Survival/Saved/RidgeRework/'
T = 'D:/UE5Projects/RTS_Survival/Saved/IntroRear/terrain/'
UMAP = 'D:/UE5Projects/RTS_Survival/Content/RTS_Survival/Maps/GerMissions/Ger_IntroMission.umap'
BACKUP = OUT + 'Ger_IntroMission_before_ridge.umap'
CLIFF_MI = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Materials/MI_IntroCliff'
PREFIX = 'RIDGE_Cliff_'
FOLDER = 'IntroRidge/Cliffs/'
GRID = (105, 75, 825, 585)
SY, SX = GRID[3] - GRID[1] + 1, GRID[2] - GRID[0] + 1
RUS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/'
SKIPS = {
    RUS + 'Rus_Building_Barracks': ['/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/VehicleParts/ScavObj_VehicleParts2.ScavObj_VehicleParts2_C'],
    RUS + 'Rus_Building_CoolingTowers': ['/Game/RTS_Survival/Blueprints/Resources/ChildBps/Metal/Resource_Metal_Barrels.Resource_Metal_Barrels_C'],
}
FUEL_WRECK = RUS + 'FuelStorage/BP_Dest_RusFuelLarge'


def log(m):
    unreal.log_warning('RIDGEAPPLY ' + str(m))


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def _read_u16(path):
    a = array('H')
    with open(path, 'rb') as handle:
        a.frombytes(handle.read())
    return a


def check_terrain(rect):
    rear_lib.export_region('ridge_check', rect)
    now = _read_u16(T + 'ridge_check_height.r16')
    full = _read_u16(T + 'now_full_height.r16')
    w = rect[2] - rect[0] + 1
    bad = total = 0
    for j in range(rect[1], rect[3] + 1):
        for i in range(rect[0], rect[2] + 1):
            a = now[(j - rect[1]) * w + (i - rect[0])]
            b = full[(j - GRID[1]) * SX + (i - GRID[0])]
            total += 1
            bad += abs(a - b) > 3
    log('terrain check: %d of %d vertices differ from the planning export' % (bad, total))
    return bad < 20


def apply_props():
    acts = json.load(open(OUT + 'prop_actions.json'))
    by_label = {}
    for a in actors():
        by_label.setdefault(a.get_actor_label(), []).append(a)
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    moved = removed = missing = 0
    for item in acts:
        found = by_label.get(item['label'], [])
        if len(found) != 1:
            missing += 1
            continue
        a = found[0]
        if item['action'] == 'remove':
            sub.destroy_actor(a)
            removed += 1
        else:
            loc = a.get_actor_location()
            a.set_actor_location(unreal.Vector(loc.x, loc.y, loc.z + item['dz']), False, False)
            moved += 1
    log('props: moved %d, removed %d, missing/ambiguous %d' % (moved, removed, missing))


def instance_actor(label, folder, mesh, transforms, material):
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = sub.spawn_actor_from_class(unreal.Actor, unreal.Vector(0.0, 0.0, 0.0))
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


def place_cliffs():
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    old = [a for a in actors() if a.get_actor_label().startswith(PREFIX)]
    for a in old:
        sub.destroy_actor(a)
    data = json.load(open(OUT + 'Ridge_cliffs.json'))
    mi = unreal.load_asset(CLIFF_MI)
    groups = {}
    for it in data['instances']:
        groups.setdefault(it['piece'], []).append(it)
    total = 0
    for piece, items in sorted(groups.items()):
        mesh = unreal.load_asset(data['mesh_path'].replace('<piece>', piece))
        if mesh is None:
            log('missing mesh ' + piece)
            continue
        tr = [unreal.Transform(unreal.Vector(i['x'], i['y'], i['z']), unreal.Rotator(i['roll'], i['pitch'], i['yaw']),
                               unreal.Vector(i['scale'], i['scale'], i['scale'])) for i in items]
        instance_actor(PREFIX + piece, FOLDER, mesh, tr, mi)
        total += len(items)
    log('cliffs: removed %d old actors, placed %d instances in %d actors' % (len(old), total, len(groups)))


def blueprint_defaults():
    for path, classes in SKIPS.items():
        bp = unreal.load_asset(path)
        cdo = unreal.get_default_object(bp.generated_class())
        values = [unreal.load_class(None, c) for c in classes]
        cdo.set_editor_property('death_spawn_classes_to_skip', values)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cdo = unreal.get_default_object(bp.generated_class())
        log('%s skip = %s' % (path.split('/')[-1], cdo.get_editor_property('death_spawn_classes_to_skip')))
        unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    bp = unreal.load_asset(FUEL_WRECK)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('initial_life_span', 10.0)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    log('fuel wreck life span %s' % cdo.get_editor_property('initial_life_span'))
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)


try:
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if 'Ger_IntroMission' not in world.get_path_name():
        raise RuntimeError('wrong map open: ' + world.get_path_name())
    rect = tuple(json.load(open(OUT + 'Ridge_cliffs.json'))['rect'])
    # RIDGE_CLIFFS_ONLY=1: the terrain is already on the map; only re-place the rocks (second pass, ridge_gaps.py)
    cliffs_only = os.environ.get('RIDGE_CLIFFS_ONLY', '0') == '1'
    if not cliffs_only:
        if not check_terrain(rect):
            raise RuntimeError('the landscape changed since the planning export: re-run base_leftovers_probe + ridge_gen')
        blueprint_defaults()
        log('import heights %s' % rear_lib.import_heights('ridge', rect))
        apply_props()
    place_cliffs()
    os.makedirs(OUT, exist_ok=True)
    if cliffs_only:
        BACKUP = OUT + 'Ger_IntroMission_before_ridge_rocks2.umap'
    shutil.copy2(UMAP, BACKUP)
    size_before = os.path.getsize(UMAP)
    unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, '/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission')
    size_after = os.path.getsize(UMAP)
    log('save %s size %d -> %d (%.3f)' % (ok, size_before, size_after, size_after / size_before))
    if size_after < 0.9 * size_before:
        log('SIZE DROP: restoring the backup')
        shutil.copy2(BACKUP, UMAP)
    log('DONE')
except Exception:  # noqa: BLE001
    log('EXCEPTION ' + traceback.format_exc())
