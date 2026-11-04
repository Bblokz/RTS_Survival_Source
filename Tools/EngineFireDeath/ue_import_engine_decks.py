"""Imports the hero tank engine decks made by build_engine_decks.py and wires them into the wreck Blueprints.

Run headless (save fails silently while another editor holds these packages; the script checks file times):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file>" -unattended -nop4 -nullrhi
Per tank:
  - reimports SM_Destroyed_<Tank>_Hull over the existing wreck hull (deck cut out, burnt engine bay); sockets,
    collision and LOD settings stay, the new M_EngineBay slot gets M_EngineDeckDebris;
  - imports SM_Destroyed_<Tank>_EngineDeck next to it (hull pivot, box collision for the physics flight);
  - adds a static mesh component EngineDeck under Hull (identity transform) to every wreck Blueprint using that hull.
UWreckEngineFireDeathComponent throws EngineDeck on an engine fire death.
"""
import os

import unreal

EXPORT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Export', 'Decks')
HEAVY = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy'
BAY_MATERIAL = '/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/EngineDeck/M_EngineDeckDebris'
BAY_SLOT = 'M_EngineBay'
DECK_COMPONENT = 'EngineDeck'
HULL_COMPONENT = 'Hull'
TURRET_COMPONENT = 'Turret'

TANKS = {
    'Tiger1': dict(folder=HEAVY + '/Tiger/Meshes', hull='SM_Destroyed_Tiger1_Hull',
                   blueprints=[HEAVY + '/Tiger/Env_Dest_Tiger1']),
    'PantherD': dict(folder=HEAVY + '/PantherD/Meshes', hull='SM_Destroyed_PantherD_Hull',
                     blueprints=[HEAVY + '/PantherD/Env_Dest_PantherD', HEAVY + '/PantherD/Env_Dest_PantherDNoFlame',
                                 HEAVY + '/PanzerV_III/Env_Dest_PanzerV_III']),
    'Tiger2': dict(folder=HEAVY + '/TigerII/Meshes', hull='SM_Destroyed_Tiger2_Hull',
                   blueprints=[HEAVY + '/TigerII/Env_Dest_TigerII', HEAVY + '/TigerII/Env_Dest_TigerII105']),
}

# Child wrecks that swap in their own (uncut) hull mesh: the inherited EngineDeck is emptied there, so they keep
# the generic deck debris and show no second deck.
OWN_HULL_CHILDREN = [HEAVY + '/Tiger/Env_Dest_TigerH1_ASPS', HEAVY + '/Tiger/Overcharged/Env_Dest_Tiger1_Overcharged',
                     HEAVY + '/Tiger/RailBunker/Env_Dest_TigerRail_RailBunker']

EAL = unreal.EditorAssetLibrary
MESH_SUBSYSTEM = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def log(text):
    unreal.log_warning('EDK ' + text)


def asset_file(asset_path):
    return unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_content_dir() + asset_path[len('/Game/'):] + '.uasset')


def save(asset_path):
    file_path = asset_file(asset_path)
    before = os.path.getmtime(file_path) if os.path.exists(file_path) else 0
    EAL.save_asset(asset_path, only_if_is_dirty=False)
    after = os.path.getmtime(file_path) if os.path.exists(file_path) else 0
    if after <= before:
        raise RuntimeError('save did not reach disk (package locked by another editor?): ' + asset_path)
    log('saved ' + asset_path)


def snapshot(mesh):
    body_setup = mesh.get_editor_property('body_setup')
    agg_geom = body_setup.get_editor_property('agg_geom')
    return dict(
        sockets=sorted((str(s.socket_name), round(s.relative_location.x, 1), round(s.relative_location.y, 1),
                        round(s.relative_location.z, 1)) for s in mesh.get_sockets_by_tag('')),
        convex=len(agg_geom.get_editor_property('convex_elems')),
        boxes=len(agg_geom.get_editor_property('box_elems')),
        trace=str(body_setup.get_editor_property('collision_trace_flag')),
        lods=mesh.get_num_lods())


def import_fbx(fbx_path, folder, name):
    existing = set(str(path).split('.')[0] for path in EAL.list_assets(folder, recursive=False))
    task = unreal.AssetImportTask()
    task.filename = fbx_path
    task.destination_path = folder
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(folder + '/' + name)
    if mesh is None:
        raise RuntimeError('import failed: ' + fbx_path)
    remove_created_materials(folder, existing)
    return mesh


def remove_created_materials(folder, existing):
    """Interchange may create material assets for the FBX materials; the meshes use the existing ones."""
    for path in EAL.list_assets(folder, recursive=False):
        package = str(path).split('.')[0]
        if package not in existing and isinstance(unreal.load_asset(package), unreal.MaterialInterface):
            log('deleting imported material ' + package)
            EAL.delete_asset(package)


def set_slot_materials(mesh, materials_by_slot):
    static_materials = mesh.get_editor_property('static_materials')
    for index, slot in enumerate(static_materials):
        material = materials_by_slot.get(str(slot.material_slot_name))
        if material is not None:
            mesh.set_material(index, material)
    log('%s slots %s' % (mesh.get_name(), [(str(s.material_slot_name), s.material_interface.get_name() if
                                             s.material_interface else None)
                                            for s in mesh.get_editor_property('static_materials')]))


def reimport_hull(tank, bay_material):
    hull_path = tank['folder'] + '/' + tank['hull']
    hull = unreal.load_asset(hull_path)
    before = snapshot(hull)
    hull_materials = {str(s.material_slot_name): s.material_interface for s in hull.get_editor_property('static_materials')}
    hull = import_fbx(os.path.join(EXPORT_DIR, tank['hull'] + '.fbx'), tank['folder'], tank['hull'])
    after = snapshot(hull)
    for key in before:
        if before[key] != after[key]:
            raise RuntimeError('%s %s changed on reimport: %s -> %s' % (tank['hull'], key, before[key], after[key]))
    hull_materials[BAY_SLOT] = bay_material
    set_slot_materials(hull, hull_materials)
    log('%s reimported: verts=%d %s' % (tank['hull'], MESH_SUBSYSTEM.get_number_verts(hull, 0), after))
    return hull_materials


def import_deck(tank, hull_materials):
    deck_name = tank['hull'].replace('_Hull', '_EngineDeck')
    deck = import_fbx(os.path.join(EXPORT_DIR, deck_name + '.fbx'), tank['folder'], deck_name)
    set_slot_materials(deck, hull_materials)
    # A box around the whole deck: it only tumbles for a few seconds and then lies as a static wreck part.
    unreal.EditorStaticMeshLibrary.remove_collisions(deck)
    unreal.EditorStaticMeshLibrary.add_simple_collisions(deck, unreal.ScriptingCollisionShapeType.BOX)
    box = deck.get_bounding_box()
    log('%s imported: verts=%d collision=%d bounds min=%s max=%s' % (
        deck_name, MESH_SUBSYSTEM.get_number_verts(deck, 0),
        unreal.EditorStaticMeshLibrary.get_simple_collision_count(deck), box.min, box.max))
    return deck


def find_subobject(subsystem, handles, name):
    for handle in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if template is not None and template.get_name().replace('_GEN_VARIABLE', '') == name:
            return handle, template
    return None, None


def add_deck_component(blueprint_path, deck_mesh):
    blueprint = unreal.load_asset(blueprint_path)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    hull_handle, hull_template = find_subobject(subsystem, handles, HULL_COMPONENT)
    if hull_handle is None:
        raise RuntimeError(blueprint_path + ' has no Hull component')
    deck_handle, deck_template = find_subobject(subsystem, handles, DECK_COMPONENT)
    if deck_handle is None:
        params = unreal.AddNewSubobjectParams(parent_handle=hull_handle, new_class=unreal.StaticMeshComponent,
                                              blueprint_context=blueprint)
        deck_handle, fail_reason = subsystem.add_new_subobject(params)
        if not unreal.SubobjectDataBlueprintFunctionLibrary.is_handle_valid(deck_handle):
            raise RuntimeError('could not add EngineDeck to %s: %s' % (blueprint_path, fail_reason))
        subsystem.rename_subobject(deck_handle, unreal.Text(DECK_COMPONENT))
        deck_template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(
            unreal.SubobjectDataBlueprintFunctionLibrary.get_data(deck_handle))

    _, turret_template = find_subobject(subsystem, handles, TURRET_COMPONENT)
    # The deck behaves like the other wreck parts: same collision and mobility as the hull.
    deck_template.set_editor_property('static_mesh', deck_mesh)
    deck_template.set_editor_property('relative_location', unreal.Vector(0, 0, 0))
    deck_template.set_editor_property('relative_rotation', unreal.Rotator(0, 0, 0))
    deck_template.set_editor_property('relative_scale3d', unreal.Vector(1, 1, 1))
    deck_template.set_editor_property('mobility', hull_template.get_editor_property('mobility'))
    deck_template.set_collision_profile_name(hull_template.get_collision_profile_name())
    deck_template.set_editor_property('can_ever_affect_navigation',
                                      hull_template.get_editor_property('can_ever_affect_navigation'))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    log('%s: EngineDeck under Hull (turret template %s)' % (blueprint_path, turret_template is not None))
    save(blueprint_path)


def clear_inherited_deck(blueprint_path):
    blueprint = unreal.load_asset(blueprint_path)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if template is None or template.get_name().replace('_GEN_VARIABLE', '') != DECK_COMPONENT:
            continue
        # The child's own override of the inherited template (created on first access).
        override = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, blueprint)
        override.set_editor_property('static_mesh', None)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        log('%s: inherited EngineDeck emptied (own hull)' % blueprint_path)
        save(blueprint_path)
        return
    raise RuntimeError(blueprint_path + ' has no inherited EngineDeck')


def main():
    bay_material = unreal.load_asset(BAY_MATERIAL)
    if bay_material is None:
        raise RuntimeError('missing ' + BAY_MATERIAL)
    for key, tank in TANKS.items():
        hull_materials = reimport_hull(tank, bay_material)
        deck = import_deck(tank, hull_materials)
        save(tank['folder'] + '/' + tank['hull'])
        save(tank['folder'] + '/' + deck.get_name())
        for blueprint_path in tank['blueprints']:
            add_deck_component(blueprint_path, deck)
    for blueprint_path in OWN_HULL_CHILDREN:
        clear_inherited_deck(blueprint_path)
    log('done')


if os.environ.get('EDK_ONLY_CHILDREN'):
    for path in OWN_HULL_CHILDREN:
        clear_inherited_deck(path)
else:
    main()
