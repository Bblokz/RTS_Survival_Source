"""PIE check for wreck holes (Docs/Environment/WreckHoleDamage.md). Run over UECP execute_python while PIE runs on
UT_VehicleObstacle (take Saved/WallChunks/pie.lock first, see Tools/WallChunks/AGENT_BRIEF.md).

    WRECKS = ['/Game/.../Env_Dest_PZIV.Env_Dest_PZIV_C', ...]   # optional, set before exec
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WreckHoles/pie_wreck_holes_check.py', encoding='utf-8').read())

Spawns each wreck on the flat strip south of the player vehicles, fires debug hits at its hull and turret from the
south-east, and prints one RTS_WRECK_HOLES_CHECK line per wreck (tracked meshes, holes, swapped materials).
Leaves the wrecks standing for screenshots; call cleanup() afterwards.
"""
import unreal

DEFAULT_WRECKS = [
    '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Env_Dest_PZIV.Env_Dest_PZIV_C',
    '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Env_Dest_T34_76.Env_Dest_T34_76_C',
]
SPACING = 900.0
ORIGIN = unreal.Vector(12500.0, -3300.0, 95.0)
CALIBRES = [37.0, 75.0, 122.0]


def _pie_world():
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    if not worlds:
        raise RuntimeError('PIE is not running')
    return worlds[0]


def _ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 5000.0), unreal.Vector(x, y, -5000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else ORIGIN.z


def _component(actor):
    for component in actor.get_components_by_class(unreal.ActorComponent):
        if component.get_class().get_name() == 'WreckHoleDamageComponent':
            return component
    return None


SPAWNED = []


def check(wrecks):
    world = _pie_world()
    for index, class_path in enumerate(wrecks):
        cls = unreal.load_class(None, class_path)
        x = ORIGIN.x + index * SPACING
        location = unreal.Vector(x, ORIGIN.y, _ground_z(world, x, ORIGIN.y))
        actor = unreal.GameplayStatics.begin_deferred_actor_spawn_from_class(
            world, cls, unreal.Transform(location, unreal.Rotator(0, 0, 0), unreal.Vector(1, 1, 1)),
            unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN)
        actor = unreal.GameplayStatics.finish_spawning_actor(actor, unreal.Transform(location, unreal.Rotator(0, 0, 0)))
        SPAWNED.append(actor)
        component = _component(actor)
        if component is None:
            print(f'RTS_WRECK_HOLES_CHECK {cls.get_name()} FAIL no component')
            continue
        meshes = [m for m in actor.get_components_by_class(unreal.StaticMeshComponent) if m.is_visible()]
        before = {m.get_name(): [mat.get_name() if mat else None for mat in m.get_materials()] for m in meshes}
        for mesh_index, mesh in enumerate(meshes):
            origin, extent = mesh.bounds.origin, mesh.bounds.box_extent
            for shot, calibre in enumerate(CALIBRES):
                # From the south-east, aimed at the mesh, spread along its length.
                target = unreal.Vector(origin.x + (shot - 1) * extent.x * 0.5, origin.y, origin.z + extent.z * 0.2)
                direction = unreal.Vector(-0.35, 1.0, -0.4)
                direction.normalize()
                # Like a shell: the impact on the mesh's simple collision, else the aim point itself.
                result = mesh.k2_line_trace_component(target - direction * 800.0, target + direction * 800.0,
                                                      False, False, False)
                impact = result[1] if result and result[0] else target
                component.debug_fire_test_hit(impact, direction, calibre)
        after = {m.get_name(): [mat.get_name() if mat else None for mat in m.get_materials()] for m in meshes}
        print(f'RTS_WRECK_HOLES_CHECK {cls.get_name()} tracked={component.get_tracked_mesh_count()} '
              f'holes={component.get_hole_count()} before={before} after={after}')


def cleanup():
    for actor in SPAWNED:
        if actor is not None:
            actor.destroy_actor()
    SPAWNED.clear()


check(globals().get('WRECKS') or DEFAULT_WRECKS)
