"""Spawn Soviet tank Blueprints far under the map (editor world, never saved) and list their mesh components."""
import unreal

V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
BPS = globals().get('INSPECT_BPS', [V + 'Rus_Medium/T34/Rus_Vehicle_T3476_Soviet', V + 'Rus_Heavy/KV1/Rus_Vehicle_KV-1E_Soviet',
                                    V + 'Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-2_Soviet'])
asub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def fmt(v):
    return '(%.0f,%.0f,%.0f)' % (v.x, v.y, v.z)


for old in asub.get_all_level_actors():
    if old.get_actor_location().z < -40000:
        asub.destroy_actor(old)

for path in BPS:
    cls = unreal.load_class(None, path + '.' + path.split('/')[-1] + '_C')
    a = asub.spawn_actor_from_class(cls, unreal.Vector(0, 0, -50000), unreal.Rotator(0, 0, 0))
    print('==', path, a.get_name())
    base = a.get_actor_transform()
    for c in a.get_components_by_class(unreal.MeshComponent):
        mesh = None
        if isinstance(c, unreal.StaticMeshComponent):
            mesh = c.static_mesh
        elif isinstance(c, unreal.SkinnedMeshComponent):
            mesh = c.get_editor_property('skinned_asset') if hasattr(c, 'get_editor_property') else None
        rel = c.get_world_transform().make_relative(base) if hasattr(c.get_world_transform(), 'make_relative') else None
        loc = c.get_world_location() - a.get_actor_location()
        print('  %-28s %-26s vis %d hid %d mesh %s loc %s rot %s mats %s' % (
            c.get_name(), c.get_class().get_name(), c.is_visible(), c.get_editor_property('hidden_in_game'),
            mesh.get_name() if mesh else None, fmt(loc), c.get_world_rotation(),
            [m.get_name() if m else None for m in c.get_materials()]))
    # child actors (turrets are often child actor components)
    for cac in a.get_components_by_class(unreal.ChildActorComponent):
        ch = cac.get_editor_property('child_actor')
        print('  child actor', cac.get_name(), ch.get_class().get_name() if ch else None)
        if ch:
            for c in ch.get_components_by_class(unreal.MeshComponent):
                mesh = c.static_mesh if isinstance(c, unreal.StaticMeshComponent) else c.get_editor_property('skinned_asset')
                print('    %-26s %-26s vis %d mesh %s loc %s mats %s' % (
                    c.get_name(), c.get_class().get_name(), c.is_visible(), mesh.get_name() if mesh else None,
                    fmt(c.get_world_location() - a.get_actor_location()), [m.get_name() if m else None for m in c.get_materials()]))
    asub.destroy_actor(a)
