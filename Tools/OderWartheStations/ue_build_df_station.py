"""Imports the direction-finder station meshes and builds BP_OW_DFStation (headless, editor closed or a second editor).

    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash
        -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Needs a DLL with the supply point hook (FSupplyPointSettings::bRunsSupplies, EnemyMiniMapIcons, DFStation_* icons).
Sources: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Stations (df_station_build.py).
Creates, only inside /Game/RTS_Survival/Blueprints/Environment/OderWarthe/Stations:
  Meshes/SM_OW_DFStation_*        Base (UCX collision), LoopAerial, SignEnemy, SignPlayer, LampGlow
  Materials/MI_OW_DF_*Paint       red / white / black paint on the Oder-Warthe kit master M_OW_Kit
    BP_OW_DFStation                 ASupplyPoint child: capture-only (no supply runs), enemy can retake, 25 m radius,
                                  flag 5 m travel, EnemyHeld / PlayerHeld tagged sign + lamp, DFStation_* minimap icons
Re-runnable: meshes re-import in place, the Blueprint is deleted and rebuilt. Prints DFSTATION lines; the last one is
DFSTATION RESULT OK|FAIL.
"""
import json
import math

import unreal

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Stations/'
KIT = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit'
DEST = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Stations'
BP_NAME = 'BP_OW_DFStation'
FLAG_MESH = '/Game/RTS_Survival/Blueprints/Environment/SupplyPoint/Meshes/SM_SupplyPoint_Flag'
LAMP_MI = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Tent/AITraining/MI_RusTent_WindowGlow'
EMBLEMS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/SovietFlags/Mat/Emblems/'
POLES = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/MasterMaterials/FactionFlags/'
MESHES = ['SM_OW_DFStation_Base', 'SM_OW_DFStation_LoopAerial', 'SM_OW_DFStation_SignEnemy',
          'SM_OW_DFStation_SignPlayer', 'SM_OW_DFStation_LampGlow']
PAINTS = {
    'MI_OW_DF_RedPaint': dict(tex='T_OW_RustSteel_D', tint=(0.78, 0.10, 0.08), rough=0.72, metal=0.15),
    'MI_OW_DF_WhitePaint': dict(tex='T_OW_Wood_D', tint=(1.0, 1.0, 0.97), rough=0.85, metal=0.0),
    'MI_OW_DF_BlackPaint': dict(tex='T_OW_Concrete_D', tint=(0.07, 0.07, 0.07), rough=0.8, metal=0.0),
}
LOOP_MIS = {
    'MI_OW_DF_LoopSteel': dict(tex='T_OW_RustSteel_D', tint=(0.42, 0.40, 0.40), rough=0.7, metal=0.35),
    'MI_OW_DF_LoopRust': dict(tex='T_OW_RustSteel_D', tint=(0.85, 0.82, 0.80), rough=0.78, metal=0.25),
}
LOOP_SPIN_TURNS_PER_SECOND = 0.045
FLAG_W = 160.0
FLAG_GAP = 9.0
FLAG_LOWERED_TOP = 220.0
FLAG_RAISE = 500.0
FLAG_FLY_YAW = 90.0           # flies toward +Y (over the yard), broadside to the station front
CAPTURE_RADIUS = 2500.0
PROGRESS_BAR_Z = 950.0
LOOP_START_YAW = 35.0

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
OK = [True]


def log(*parts):
    unreal.log_warning('DFSTATION ' + ' '.join(str(p) for p in parts))


def fail(msg):
    OK[0] = False
    log('FAIL', msg)


def enum_value(enum_cls, wanted):
    key = wanted.replace('_', '').lower()
    for name in dir(enum_cls):
        if name.replace('_', '').lower() == key:
            return getattr(enum_cls, name)
    raise RuntimeError('%s not in %s' % (wanted, enum_cls))


def save(asset, what):
    if EAL.save_loaded_asset(asset, False):
        return True
    fail('save ' + what)
    return False


# ---------------------------------------------------------------------------------------------------------- materials
def kit_instance(path, parent, cfg):
    name = path.rsplit('/', 1)[1]
    mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
        name, DEST + '/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    MEL.set_material_instance_texture_parameter_value(mi, 'BaseTex', unreal.load_asset(KIT + '/Textures/' + cfg['tex']))
    t = cfg['tint']
    MEL.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(t[0], t[1], t[2], 1.0))
    MEL.set_material_instance_scalar_parameter_value(mi, 'Roughness', cfg['rough'])
    MEL.set_material_instance_scalar_parameter_value(mi, 'Metallic', cfg['metal'])
    MEL.set_material_instance_scalar_parameter_value(mi, 'Tiling', 1.0)
    MEL.update_material_instance(mi)
    save(mi, name)
    return mi


def loop_spin_master():
    """Not built: Python in 5.5 cannot reach a material's World Position Offset pin (MaterialProperty has no WPO
    entry, the editor-only input is not exposed) and deleting the half-built copy crashes the commandlet. The loop
    stays still on the kit materials until a spinner exists (C++ or a graph built in a second editor)."""
    return None


def materials():
    mats = {}
    kit_master = unreal.load_asset(KIT + '/Materials/M_OW_Kit')
    for name in ('MI_OW_Concrete', 'MI_OW_ConcreteDark', 'MI_OW_RustSteel', 'MI_OW_CastSteel', 'MI_OW_DarkSteel',
                 'MI_OW_Wood', 'MI_OW_Void'):
        mats[name] = unreal.load_asset(KIT + '/Materials/' + name)
    for name, cfg in PAINTS.items():
        mats[name] = kit_instance(DEST + '/Materials/' + name, kit_master, cfg)
    spin = loop_spin_master()
    loop = {name: kit_instance(DEST + '/Materials/' + name, spin, cfg) for name, cfg in LOOP_MIS.items()} if spin         else {'MI_OW_DF_LoopSteel': mats['MI_OW_DarkSteel'], 'MI_OW_DF_LoopRust': mats['MI_OW_RustSteel']}
    mats['MI_RusTent_WindowGlow'] = unreal.load_asset(LAMP_MI)
    for k, v in mats.items():
        if v is None:
            fail('material missing ' + k)
    return mats, loop


# ---------------------------------------------------------------------------------------------------------- meshes
def fbx_ui():
    ui = unreal.FbxImportUI()
    for k, v in dict(import_mesh=True, import_as_skeletal=False, import_materials=False, import_textures=False,
                     import_animations=False).items():
        ui.set_editor_property(k, v)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    d = ui.get_editor_property('static_mesh_import_data')
    for k, v in dict(combine_meshes=True, auto_generate_collision=False, one_convex_hull_per_ucx=True,
                     generate_lightmap_u_vs=False, build_nanite=False, remove_degenerates=True).items():
        d.set_editor_property(k, v)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return ui


def import_mesh(name, mats):
    folder = DEST + '/Meshes'
    task = unreal.AssetImportTask()
    task.filename = SRC + 'FBX/' + name + '.fbx'
    task.destination_path = folder
    task.destination_name = name
    task.automated, task.save, task.replace_existing, task.options = True, False, True, fbx_ui()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        TOOLS.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    mesh = unreal.load_asset(folder + '/' + name)
    if mesh is None:
        fail('import ' + name)
        return None
    ns = mesh.get_editor_property('nanite_settings')
    ns.enabled = False
    mesh.set_editor_property('nanite_settings', ns)
    statics = mesh.get_editor_property('static_materials')
    for i, s in enumerate(statics):
        sn = str(s.get_editor_property('material_slot_name'))
        key = next((k for k in sorted(mats, key=len, reverse=True) if sn.startswith(k)), None)
        if key is None:
            fail('%s slot %s has no material' % (name, sn))
            continue
        mesh.set_material(i, mats[key])
    mesh.set_editor_property('light_map_coordinate_index', 1)
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if name != 'SM_OW_DFStation_Base':
        sub.remove_collisions(mesh)
    body = mesh.get_editor_property('body_setup')
    if body is not None:
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    save(mesh, name)
    b = mesh.get_bounding_box()
    log('mesh', name, 'tris', mesh.get_num_triangles(0), 'bounds', [round(b.min.x), round(b.min.y), round(b.min.z)],
        [round(b.max.x), round(b.max.y), round(b.max.z)],
        'mats', [mesh.get_material(i).get_name() for i in range(len(statics))])
    return mesh


# ---------------------------------------------------------------------------------------------------------- blueprint
def templates(blueprint):
    found = {}
    for handle in SDS.k2_gather_subobject_data_for_blueprint(blueprint):
        obj = SDL.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(handle), blueprint)
        if obj:
            found.setdefault(obj.get_name().replace('_GEN_VARIABLE', ''), (handle, obj))
    return found


def add(blueprint, name, cls, parent_handle):
    params = unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=cls, blueprint_context=blueprint)
    handle, reason = SDS.add_new_subobject(params)
    if not SDL.is_handle_valid(handle):
        raise RuntimeError('add %s failed: %s' % (name, reason))
    SDS.rename_subobject(handle, unreal.Text(name))
    return handle, SDL.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(handle), blueprint)


def place(comp, loc=(0.0, 0.0, 0.0), rot=None):
    comp.set_editor_property('relative_location', unreal.Vector(*loc))
    comp.set_editor_property('relative_rotation', rot or unreal.Rotator(0.0, 0.0, 0.0))


def mesh_component(blueprint, name, parent_handle, mesh, loc=(0, 0, 0), rot=None, collide=False, tags=(),
                   movable=False):
    handle, comp = add(blueprint, name, unreal.StaticMeshComponent, parent_handle)
    comp.set_editor_property('static_mesh', unreal.load_asset(mesh))
    comp.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE if movable else unreal.ComponentMobility.STATIC)
    place(comp, loc, rot)
    if not collide:
        comp.set_collision_profile_name('NoCollision')
        comp.set_editor_property('can_ever_affect_navigation', False)
    if tags:
        comp.set_editor_property('component_tags', list(tags))
    return handle, comp


def flag_rotation():
    """Banner local +X (hoist -> fly) along the fly yaw, local +Y (bottom -> top edge) up (as BP_SupplyPoint_TowerCrane)."""
    a = math.radians(FLAG_FLY_YAW)
    return unreal.MathLibrary.make_rot_from_xy(unreal.Vector(math.cos(a), math.sin(a), 0.0), unreal.Vector(0, 0, 1))


LOOP_BP_NAME = 'BP_OW_DFStation_Loop'
LOOP_DEGREES_PER_SECOND = 16.0


def build_loop_actor():
    """BP_OW_DFStation_Loop: the loop mesh under the default scene root plus a RotatingMovementComponent (yaw)."""
    path = DEST + '/' + LOOP_BP_NAME
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.Actor)
    blueprint = TOOLS.create_asset(LOOP_BP_NAME, DEST, unreal.Blueprint, factory)
    handles = SDS.k2_gather_subobject_data_for_blueprint(blueprint)
    actor_handle = handles[0]
    root_handle = handles[1] if len(handles) > 1 else handles[0]
    _, mesh = add(blueprint, 'Loop', unreal.StaticMeshComponent, root_handle)
    mesh.set_editor_property('static_mesh', unreal.load_asset(DEST + '/Meshes/SM_OW_DFStation_LoopAerial'))
    mesh.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    mesh.set_collision_profile_name('NoCollision')
    mesh.set_editor_property('can_ever_affect_navigation', False)
    _, spin = add(blueprint, 'Spin', unreal.RotatingMovementComponent, actor_handle)
    spin.set_editor_property('rotation_rate', unreal.Rotator(0.0, LOOP_DEGREES_PER_SECOND, 0.0))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    found = templates(blueprint)
    log('loop actor components', sorted(found))
    root = [o for n, (h, o) in found.items() if isinstance(o, unreal.SceneComponent) and o.get_attach_parent() is None]
    for r in root:
        r.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    save(blueprint, LOOP_BP_NAME)
    return blueprint.generated_class()


def build_blueprint(layout):
    path = DEST + '/' + BP_NAME
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.SupplyPoint)
    blueprint = TOOLS.create_asset(BP_NAME, DEST, unreal.Blueprint, factory)
    comps = templates(blueprint)
    root = comps['SupplyPointRoot'][0]
    m = DEST + '/Meshes/'
    # Blender +Y is Unreal -Y (FBX axis conversion): every layout Y and yaw from df_station_report.json flips sign.
    mesh_component(blueprint, 'Base', root, m + 'SM_OW_DFStation_Base', collide=True, movable=True)
    lx, ly, lz = layout['hut']['loop_pivot']
    # The loop turns as its own small actor (RotatingMovementComponent turns its actor's root) in a child actor slot.
    _, loop = add(blueprint, 'LoopAerial', unreal.ChildActorComponent, root)
    loop.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    place(loop, (lx, -ly, lz), unreal.Rotator(0.0, 0.0, LOOP_START_YAW))
    loop.set_editor_property('child_actor_class', build_loop_actor())
    sx, sy = layout['sign']['xy']
    sign_rot = unreal.Rotator(0.0, 0.0, -layout['sign']['yaw'])
    sz = layout['sign']['board_z']
    mesh_component(blueprint, 'SignEnemy', root, m + 'SM_OW_DFStation_SignEnemy', (sx, -sy, sz), sign_rot,
                   tags=('EnemyHeld',), movable=True)
    mesh_component(blueprint, 'SignPlayer', root, m + 'SM_OW_DFStation_SignPlayer', (sx, -sy, sz), sign_rot,
                   tags=('PlayerHeld',), movable=True)
    ax, ay, az = layout['hut']['lamp']
    mesh_component(blueprint, 'LampGlow', root, m + 'SM_OW_DFStation_LampGlow', (ax, -ay, az), tags=('PlayerHeld',),
                   movable=True)
    px, py = layout['flag']['pole']
    a = math.radians(FLAG_FLY_YAW)
    off = FLAG_GAP + FLAG_W / 2
    pole_h, pole = add(blueprint, 'FlagPole', unreal.SceneComponent, root)
    pole.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    place(pole, (px + off * math.cos(a), -py + off * math.sin(a), FLAG_LOWERED_TOP))
    mesh_component(blueprint, 'Flag', pole_h, FLAG_MESH, rot=flag_rotation(), movable=True)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    cdo = unreal.get_default_object(blueprint.generated_class())
    s = cdo.get_editor_property('supply_point_settings')
    s.set_editor_property('runs_supplies', False)
    s.set_editor_property('enemy_can_capture', True)
    s.set_editor_property('initial_owning_player', 2)
    s.set_editor_property('capture_radius', CAPTURE_RADIUS)
    s.set_editor_property('flag_raise_height', FLAG_RAISE)
    s.set_editor_property('progress_bar_offset', unreal.Vector(0.0, 0.0, PROGRESS_BAR_Z))
    cdo.set_editor_property('supply_point_settings', s)
    cdo.set_editor_property('enemy_flag_materials', {
        enum_value(unreal.EnemyDirector, 'DirectorOfShield'): unreal.load_asset(EMBLEMS + 'MI_Emblem_DirectorOfShield'),
        enum_value(unreal.EnemyDirector, 'DirectorOfForge'): unreal.load_asset(EMBLEMS + 'MI_Emblem_DirectorOfForge'),
        enum_value(unreal.EnemyDirector, 'DirectorOfHarvest'): unreal.load_asset(EMBLEMS + 'MI_Emblem_DirectorOfHarvest'),
    })
    cdo.set_editor_property('player_flag_materials', {
        enum_value(unreal.RTSFaction, 'GerBreakthroughDoctrine'): unreal.load_asset(POLES + 'MI_BreakthroughPole'),
        enum_value(unreal.RTSFaction, 'GerStrikeDivision'): unreal.load_asset(POLES + 'MI_StrikeDivisionPole'),
    })
    icon = unreal.MinimapIconType
    cdo.set_editor_property('enemy_mini_map_icons', {
        enum_value(unreal.EnemyDirector, 'DirectorOfShield'): enum_value(icon, 'DFStation_DirectorOfShield'),
        enum_value(unreal.EnemyDirector, 'DirectorOfForge'): enum_value(icon, 'DFStation_DirectorOfForge'),
    })
    cdo.set_editor_property('player_mini_map_icons', {
        enum_value(unreal.RTSFaction, 'GerBreakthroughDoctrine'): enum_value(icon, 'DFStation_GerBreakthrough'),
        enum_value(unreal.RTSFaction, 'GerStrikeDivision'): enum_value(icon, 'DFStation_GerStrikeDivision'),
    })
    cdo.set_editor_property('flag_component_name', 'Flag')
    cdo.set_editor_property('flag_material_slot', 0)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint, BP_NAME)
    return blueprint


def verify(blueprint):
    cdo = unreal.get_default_object(blueprint.generated_class())
    s = cdo.get_editor_property('supply_point_settings')
    for k in ('runs_supplies', 'enemy_can_capture', 'initial_owning_player', 'capture_radius', 'flag_raise_height',
              'progress_bar_offset'):
        log('setting', k, s.get_editor_property(k))
    if s.get_editor_property('runs_supplies'):
        fail('runs_supplies still on')
    log('enemy icons', {str(k): str(v) for k, v in cdo.get_editor_property('enemy_mini_map_icons').items()})
    log('player icons', {str(k): str(v) for k, v in cdo.get_editor_property('player_mini_map_icons').items()})
    base_mesh = unreal.load_asset(DEST + '/Meshes/SM_OW_DFStation_Base')
    agg = base_mesh.get_editor_property('body_setup').get_editor_property('agg_geom')
    convex = len(agg.get_editor_property('convex_elems'))
    log('base collision convex hulls', convex, '(expect 7: hut, generator, flag plinth, 4 mast feet)')
    if convex != 7:
        fail('base collision hulls %d' % convex)
    comps = templates(blueprint)
    loop = comps.get('LoopAerial')
    if loop:
        log('loop child actor class', loop[1].get_editor_property('child_actor_class'))
    for name in ('Base', 'LoopAerial', 'SignEnemy', 'SignPlayer', 'LampGlow', 'FlagPole', 'Flag'):
        if name not in comps:
            fail('missing component ' + name)
            continue
        c = comps[name][1]
        log('comp', name, 'loc', c.get_editor_property('relative_location'), 'rot',
            c.get_editor_property('relative_rotation'), 'tags', list(c.get_editor_property('component_tags')))


def main():
    with open(SRC + 'df_station_report.json') as f:
        layout = json.load(f)['layout']
    mats, loop = materials()
    for name in MESHES:
        mesh = import_mesh(name, mats)
        if name == 'SM_OW_DFStation_LoopAerial' and mesh is not None:
            for i, s in enumerate(mesh.get_editor_property('static_materials')):
                sn = str(s.get_editor_property('material_slot_name'))
                mesh.set_material(i, loop['MI_OW_DF_LoopRust'] if sn.startswith('MI_OW_RustSteel')
                                  else loop['MI_OW_DF_LoopSteel'])
            save(mesh, name + ' (spin materials)')
    verify(build_blueprint(layout))
    log('RESULT ' + ('OK' if OK[0] else 'FAIL'))


try:
    main()
except Exception as exc:  # noqa: BLE001
    import traceback
    fail('exception %s\n%s' % (exc, traceback.format_exc()))
    log('RESULT FAIL')
