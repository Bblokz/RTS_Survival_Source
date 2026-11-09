"""PIE validation of wreck holes on EVERY wreck class (Docs/Environment/WreckHoleDamage.md).
Run in two calls over UECP execute_python while PIE runs on UT_VehicleObstacle (hold Saved/WallChunks/pie.lock):

    BATCH = 0; STEP = 'hit'    # spawns batch 0 (6 wrecks) on the flat strip, fires debug hits, keeps the actors
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WreckHoles/pie_validate_all_wrecks.py', encoding='utf-8').read())
    BATCH = 0; STEP = 'check'  # a second later: reads holes + swapped materials, adds to the report, destroys them
    exec(open(...).read())     # then BATCH = 1, 2, ... until BATCH * 6 >= the class count 'hit' prints

Report: Saved/WreckHoles/pie_validation.json, one row per wreck class: tracked meshes, holes, slots swapped to a
_WreckHoles material, slots left opaque (no entry), and FAIL reasons (no component, no tracked mesh, no hole).
"""
import builtins
import json
import unreal

REPORT = unreal.Paths.project_saved_dir() + 'WreckHoles/pie_validation.json'
ROW_Y = -3300.0
START_X = 11000.0
SPACING = 1100.0
PER_BATCH = 6
CALIBRE = 88.0


def _pie_world():
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    if not worlds:
        raise RuntimeError('PIE is not running')
    return worlds[0]


def _wreck_classes():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = registry.get_assets(unreal.ARFilter(
        package_paths=['/Game/RTS_Survival'], recursive_paths=True,
        class_paths=[unreal.TopLevelAssetPath('/Script/Engine', 'Blueprint')]))
    return sorted(f'{a.package_name}.{a.asset_name}_C' for a in found
                  if str(a.asset_name).startswith(('Env_Dest', 'ScavObj_')))


def _hole_component(actor):
    for component in actor.get_components_by_class(unreal.ActorComponent):
        if component.get_class().get_name() == 'WreckHoleDamageComponent':
            return component
    return None


def _fire_at(component, mesh):
    origin, extent, _ = unreal.SystemLibrary.get_component_bounds(mesh)
    direction = unreal.Vector(-0.3, 1.0, -0.45)
    direction.normalize()
    for shot in range(2):
        target = unreal.Vector(origin.x + (shot - 0.5) * extent.x * 0.6, origin.y, origin.z + extent.z * 0.25)
        # Like a shell striking the collision box: where the shot enters the mesh's bounds.
        exits = [abs(e / d) for e, d in ((extent.x, direction.x), (extent.y, direction.y), (extent.z, direction.z))
                 if abs(d) > 1e-4]
        impact = target - direction * (min(exits) if exits else 0.0)
        component.debug_fire_test_hit(impact, direction, CALIBRE)


def _spawn_in_pie(world, cls, class_path, location):
    """Python cannot spawn into the PIE world itself; the UECP play_test action does, then the actor is moved."""
    import uecp
    result = uecp.play_test.spawn_pie_actor(class_path=class_path)
    name = result.get('actor_name') if isinstance(result, dict) else None
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, cls):
        if actor.get_name() == name:
            actor.set_actor_location(location, False, True)
            return actor
    return None


def hit_step(batch_index):
    world = _pie_world()
    classes = _wreck_classes()
    # CLASSES (set before exec) stages chosen wrecks instead of a batch, e.g. for screenshots.
    batch = globals().get('CLASSES') or classes[batch_index * PER_BATCH:(batch_index + 1) * PER_BATCH]
    # Leftovers of an interrupted run on the strip.
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if actor.get_class().get_name().startswith(('Env_Dest', 'ScavObj_')) and                 abs(actor.get_actor_location().y - ROW_Y) < 200.0:
            actor.destroy_actor()
    spawned = []
    for index, class_path in enumerate(batch):
        cls = unreal.load_class(None, class_path)
        if cls is None:
            spawned.append((class_path, None))
            continue
        location = unreal.Vector(START_X + index * SPACING, ROW_Y, 120.0)
        actor = _spawn_in_pie(world, cls, class_path, location)
        spawned.append((class_path, actor))
        component = _hole_component(actor) if actor else None
        if component is None:
            continue
        for mesh in actor.get_components_by_class(unreal.StaticMeshComponent):
            if mesh.is_visible():
                _fire_at(component, mesh)
    builtins.WRECK_HOLES_SPAWNED = spawned
    return len(classes)


def check_step():
    # Batch 0 starts a fresh report; later batches add to it.
    rows = json.load(open(REPORT)) if globals().get('BATCH', 0) > 0 and unreal.Paths.file_exists(REPORT) else {}
    for class_path, actor in getattr(builtins, "WRECK_HOLES_SPAWNED", []):
        name = class_path.split('.')[-1]
        if actor is None:
            rows[name] = {'fail': 'could not load or spawn'}
            continue
        component = _hole_component(actor)
        row = {'tracked': 0, 'holes': 0, 'swapped': [], 'opaque': []}
        if component is None:
            row['fail'] = 'no WreckHoleDamage component'
        else:
            row['tracked'] = component.get_tracked_mesh_count()
            row['holes'] = component.get_hole_count()
            for mesh in actor.get_components_by_class(unreal.StaticMeshComponent):
                for material in mesh.get_materials():
                    if material is None:
                        continue
                    target = row['swapped'] if material.get_name().endswith('_WreckHoles') or \
                        (material.get_base_material() and material.get_base_material().get_name().endswith('_WreckHoles')) \
                        else row['opaque']
                    target.append(f'{mesh.get_name()}:{material.get_name()}')
            if row['tracked'] == 0:
                row['fail'] = 'no tracked mesh'
            elif row['holes'] == 0:
                row['fail'] = 'no hole cut'
            elif not row['swapped']:
                row['fail'] = 'holes but no material swapped'
        rows[name] = row
        actor.destroy_actor()
    builtins.WRECK_HOLES_SPAWNED = []
    with open(REPORT, 'w') as handle:
        json.dump(rows, handle, indent=1)
    fails = {k: v['fail'] for k, v in rows.items() if 'fail' in v}
    print(f'RTS_WRECK_HOLES_VALIDATE rows={len(rows)} fails={len(fails)} {fails}')


STEP = globals().get('STEP', 'hit')
if STEP == 'hit':
    BATCH = globals().get('BATCH', 0)
    print('classes', hit_step(BATCH))
else:
    check_step()
