"""Bake static cargo tanks for BP_SovietCargoTrain from the real Soviet tank Blueprints (2026-11-18).

Exec'd in a throwaway editor through train_editor_server.py (`python train_cmd.py cargo_tank_bake.py`).
Each tank Blueprint is spawned far under the map in the editor world (never saved, destroyed again); every visible
mesh component of the hull actor and of its turret child actor (tracks + wheels SK, hull SM, turret base SM, gun SK)
is copied in its current (reference) pose with Geometry Script into one dynamic mesh, in actor space, with the
material IDs offset per part and then merged by material. The result is a new StaticMesh with the real materials,
no collision and 3 LODs. Only the new mesh assets are saved.
"""
import unreal

V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
DEST = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/CargoTrain/'
TANKS = globals().get('BAKE_TANKS', [
    ('SM_CargoTank_T3476', V + 'Rus_Medium/T34/Rus_Vehicle_T3476_Soviet'),
    ('SM_CargoTank_KV1E', V + 'Rus_Heavy/KV1/Rus_Vehicle_KV-1E_Soviet'),
    ('SM_CargoTank_IS2', V + 'Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-2_Soviet'),
])
asub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
GS = unreal.GeometryScript_SceneUtils
ME = unreal.GeometryScript_MeshEdits
MAT = unreal.GeometryScript_Materials


def mesh_parts(actor):
    parts = []
    actors = [actor]
    for cac in actor.get_components_by_class(unreal.ChildActorComponent):
        child = cac.get_editor_property('child_actor')
        if child is not None:
            actors.append(child)
    for a in actors:
        for c in a.get_components_by_class(unreal.MeshComponent):
            if not c.is_visible() or c.get_editor_property('hidden_in_game'):
                continue
            if isinstance(c, unreal.StaticMeshComponent):
                if c.static_mesh is None:
                    continue
            elif isinstance(c, unreal.SkinnedMeshComponent):
                if c.get_editor_property('skinned_asset') is None:
                    continue
            else:
                continue
            parts.append(c)
    return parts


def bake(name, bp_path):
    cls = unreal.load_class(None, bp_path + '.' + bp_path.split('/')[-1] + '_C')
    actor = asub.spawn_actor_from_class(cls, unreal.Vector(0, 0, -50000), unreal.Rotator(0, 0, 0))
    to_actor = unreal.Transform(location=-actor.get_actor_location())
    target = unreal.DynamicMesh()
    materials = []
    opts = unreal.GeometryScriptCopyMeshFromComponentOptions()
    opts.set_editor_property('want_normals', True)
    opts.set_editor_property('want_tangents', True)
    report = []
    for comp in mesh_parts(actor):
        part = unreal.DynamicMesh()
        part, _l2w, outcome = GS.copy_mesh_from_component(comp, part, opts, True)
        if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
            report.append('  skip %s (copy failed)' % comp.get_name())
            continue
        mats = list(comp.get_materials())
        offset = len(materials)
        max_id = MAT.get_max_material_id(part)
        max_id = max_id[0] if isinstance(max_id, tuple) else max_id
        top = max(max_id, len(mats) - 1)
        for mid in range(top, -1, -1):
            MAT.remap_material_i_ds(part, mid, mid + offset)
        materials.extend(mats)
        ME.append_mesh(target, part, to_actor)
        report.append('  + %-22s %-28s tris %d mats %s' % (
            comp.get_name(), comp.get_class().get_name(), part.get_triangle_count(),
            [m.get_name() if m else None for m in mats]))
    asub.destroy_actor(actor)
    unique = []
    for m in materials:
        if m not in unique:
            unique.append(m)
    MAT.remap_to_new_material_i_ds_by_material(target, materials, unique)

    path = DEST + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    copts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    copts.set_editor_property('enable_collision', False)
    copts.set_editor_property('enable_recompute_normals', False)
    copts.set_editor_property('enable_recompute_tangents', False)
    copts.set_editor_property('enable_nanite', False)
    mesh, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(target, path, copts)
    if mesh is None or outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        print('FAILED to create', path)
        return
    slots = mesh.get_editor_property('static_materials')
    while len(slots) < len(unique):
        mesh.add_material(unique[len(slots)])
        slots = mesh.get_editor_property('static_materials')
    for i, m in enumerate(unique):
        mesh.set_material(i, m)
    sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    red = unreal.EditorScriptingMeshReductionOptions()
    red.set_editor_property('reduction_settings', [
        unreal.EditorScriptingMeshReductionSettings(1.0, 1.0),
        unreal.EditorScriptingMeshReductionSettings(0.4, 0.3),
        unreal.EditorScriptingMeshReductionSettings(0.15, 0.12)])
    sms.set_lods(mesh, red)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    b = mesh.get_bounds()
    print('%s: tris %d, %d LODs, bounds origin %s extent %s, materials %s' % (
        mesh.get_path_name(), target.get_triangle_count(), sms.get_lod_count(mesh), b.origin, b.box_extent,
        [m.get_name() if m else None for m in unique]))
    print('\n'.join(report))


for old in asub.get_all_level_actors():
    if old.get_actor_location().z < -40000:
        asub.destroy_actor(old)
for tank_name, tank_bp in TANKS:
    bake(tank_name, tank_bp)
