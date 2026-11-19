"""Rus_Building_PlatformFactory: enemy AI training activity (Docs/EnemyAI/EnemyBuildingTraining.md).

Run headless (no UECP needed), safe to rerun:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script="D:/UE5Projects/RTS_Survival/Source/Tools/RusPlatformFactory/ue_platform_factory_trainer.py"
      -unattended -nop4 -nosplash -DisablePlugins=Fab,BpGeneratorUltimate
  PF_TRAINER_DRY=1 in the environment: print the component tree and the planned placement, save nothing.

Adds, all hung off the root (not off a mesh), so construction / death visibility never switches them on:
  - EnemyBuildingTrainer (UEnemyBuildingTrainerComponent): Vehicles, FinishCycle, time x1.
  - AITrainingExit: outside the open (+X) mouth of the yard, past the nav modifier, +X out.
  - Work lights, tagged AITrainingActivity, authored hidden: two warm floods under the gantry beam and one on the
    inner edge of the gun block, all on the assembly spot in the yard; a bluish weld light at the spot.
  - Welding sparks (NS_Sparks_Welding, the player cranes' sparks, referenced not copied), tagged AITrainingActivity,
    auto-activate off, scaled up to read at RTS camera distance.
  - Concept B "Laufkatze" (picked 2026-11-19): AI_Trolley, the skeletal trolley on the gantry beam
    (PlatformFactory/AITraining/SK_RusPlatformFactory_Trolley, 30 s loop A_RusPlatformFactory_TrolleyWork; frame 0 =
    parked over the turret rack), tagged AITrainingActivity, single-node, not autoplaying; AI_AssemblyHull, the game's
    SM_T34_Hull in MI_T34Soviet on the trolley's stands with its Turret socket on the hook line, untagged (always there,
    so the seated turret never hangs in the air while the loop finishes after training).
    Phase "import" (re)imports the trolley from the Blender build first (generate_pf_trolley.py, pf_trolley_ue.json).
The yard layout comes from the source blend (AA_BuildingDesign/CantryCrane/FactoryPlatform.blend): floor z 0,
x -1..12 m, y -8..10 m, open at +x; gantry box truss along y, chords at x 2.98 / 4.71 m (hook line 3.845), top
10.95 m, a runway rail under it at z 8.72-9.04. Blender y maps to -y in the mesh (checked from the mesh bounds below;
the script stops without saving when the bounds do not match).
"""
import json
import os

import unreal

BP_PATH = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_PlatformFactory'
SPARKS = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Ger/Equipment/NS_Sparks_Welding'
TAG = 'AITrainingActivity'
DRY = os.environ.get('PF_TRAINER_DRY', '') == '1'
SUBS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
LIB = unreal.SubobjectDataBlueprintFunctionLibrary
OUT = {'log': [], 'dry': DRY}

# Source blend bounds of SM_Rus_FactoryPlatform in metres (x, y, z).
BLEND_MIN = (-13.65, -14.64, -0.52)
BLEND_MAX = (13.53, 17.93, 13.62)

TOOLS = 'D:/UE5Projects/RTS_Survival/Source/Tools/RusPlatformFactory/'
CFG = json.loads(open(TOOLS + 'pf_trolley_ue.json', encoding='utf-8').read())
HOME = CFG['ue_folder']
SCRATCH = '/Game/__RusPFTrolleyImport'
TROLLEY = CFG['skeletal_mesh']
ANIM = 'A_RusPlatformFactory_TrolleyWork'
# The hook drops 7 m and the trolley runs 7.6 m along the beam: keep the bounds generous so it never culls.
TROLLEY_BOUNDS_CM = unreal.Vector(200, 200, 200)
GRAPH_DUMP = 'D:/UE5Projects/RTS_Survival/Saved/EnemyBuildingTrainer/platform_edit2/'

# Positions in blend metres; mapped to mesh cm by blend_to_rel().
SEAT = tuple(CFG['seat_blend_m'])                 # turret ring on the hull (the hook line, y -1)
HULL_MID = (SEAT[0] - 0.46, SEAT[1], 1.2)         # middle of the hull on its stands
WORK_SPOT = SEAT
EXIT_BLEND = (15.5, 1.0, 0.5)
EXIT_NAV_MARGIN_CM = 300.0
# The baseline's 9000 / 7000 cd burnt the yard white in the capture (platform_baseline 1_training30) and 4500 / 3500
# still burnt the hull (platform_trolley): the three cones overlap on it. The beam lamps hang under the truss (z 8.3):
# at z 10.2 they sat 2 cm from a chord and lit it as a white blob.
SPOTS = [
    # name, lamp position, aim point, colour, candela, inner, outer, attenuation cm
    ('AI_FloodBeamSouth', (3.0, -6.0, 8.3), HULL_MID, (255, 214, 160), 1600.0, 20.0, 40.0, 2400.0),
    ('AI_FloodBeamNorth', (3.0, 8.0, 8.3), HULL_MID, (255, 214, 160), 1600.0, 20.0, 40.0, 2400.0),
    ('AI_FloodGunBlock', (-1.6, 1.0, 5.4), HULL_MID, (255, 200, 140), 1100.0, 20.0, 40.0, 2400.0),
]
POINTS = [
    # name, position, colour, candela, attenuation cm: at the turret ring, on the camera side (+x / -y)
    ('AI_WeldLight', (SEAT[0] + 1.0, SEAT[1] - 0.9, SEAT[2] + 0.15), (170, 200, 255), 800.0, 700.0),
]
EFFECTS = [
    # name, position, scale: on the turret ring seam
    ('AI_WeldSparksA', (SEAT[0] + 0.9, SEAT[1] - 0.75, SEAT[2] + 0.05), 2.6),
    ('AI_WeldSparksB', (SEAT[0] - 0.85, SEAT[1] + 0.6, SEAT[2] + 0.05), 2.6),
]


def log(msg):
    OUT['log'].append(msg)
    unreal.log('PF_TRAINER ' + msg)


def gather(bp):
    rows = []
    for handle in SUBS.k2_gather_subobject_data_for_blueprint(bp):
        data = LIB.get_data(handle)
        rows.append({'handle': handle, 'data': data, 'name': str(LIB.get_variable_name(data)),
                     'obj': LIB.get_object_for_blueprint(data, bp)})
    return rows


def describe(rows):
    tree = []
    for row in rows:
        obj, data = row['obj'], row['data']
        parent = ''
        for other in rows:
            if other is not row:
                try:
                    if LIB.is_attached_to(data, other['handle']):
                        parent = other['name']
                        break
                except Exception:
                    pass
        item = {'name': row['name'], 'class': obj.get_class().get_name() if obj else None, 'parent': parent,
                'root': bool(LIB.is_root_component(data)), 'inherited': bool(LIB.is_inherited_component(data))}
        if isinstance(obj, unreal.SceneComponent):
            item['loc'] = str(obj.get_editor_property('relative_location'))
            item['rot'] = str(obj.get_editor_property('relative_rotation'))
            item['scale'] = str(obj.get_editor_property('relative_scale3d'))
            item['tags'] = [str(t) for t in obj.get_editor_property('component_tags')]
            item['visible'] = bool(obj.get_editor_property('visible'))
        if isinstance(obj, unreal.StaticMeshComponent):
            mesh = obj.get_editor_property('static_mesh')
            item['mesh'] = mesh.get_path_name() if mesh else None
        if isinstance(obj, unreal.NiagaraComponent):
            asset = obj.get_editor_property('asset')
            item['asset'] = asset.get_path_name() if asset else None
            item['auto_activate'] = bool(obj.get_editor_property('auto_activate'))
        if isinstance(obj, unreal.ParticleSystemComponent):
            template = obj.get_editor_property('template')
            item['asset'] = template.get_path_name() if template else None
            item['auto_activate'] = bool(obj.get_editor_property('auto_activate'))
        if isinstance(obj, unreal.ChildActorComponent):
            cls = obj.get_editor_property('child_actor_class')
            item['child'] = cls.get_name() if cls else None
        if isinstance(obj, unreal.BoxComponent):
            item['box_extent'] = str(obj.get_editor_property('box_extent'))
        if obj is not None and obj.get_class().get_name() == 'NavModifierComponent':
            try:
                item['failsafe_extent'] = str(obj.get_editor_property('failsafe_extent'))
            except Exception as exc:
                item['failsafe_extent'] = repr(exc)
        tree.append(item)
    return tree


def find(rows, name):
    return next((r for r in rows if r['name'] == name), None)


def add(bp, rows, parent_handle, cls, name):
    row = find(rows, name)
    if row:
        log('kept ' + name)
        return row['obj']
    params = unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=cls, blueprint_context=bp)
    result = SUBS.add_new_subobject(params)
    handle, fail = (result if isinstance(result, tuple) else (result, None))
    if fail is not None and not fail.is_empty():
        raise RuntimeError('add %s failed: %s' % (name, fail))
    if not SUBS.rename_subobject(handle, unreal.Text(name)):
        raise RuntimeError('rename to %s failed' % name)
    obj = LIB.get_object_for_blueprint(LIB.get_data(handle), bp)
    if obj.get_name().split('_GEN_VARIABLE')[0] != name and str(LIB.get_variable_name(LIB.get_data(handle))) != name:
        raise RuntimeError('component %s came out as %s' % (name, obj.get_name()))
    log('added %s (%s) as %s' % (name, cls.__name__, obj.get_name()))
    return obj


def tag(comp):
    tags = [str(t) for t in comp.get_editor_property('component_tags')]
    if TAG not in tags:
        tags.append(TAG)
    comp.set_editor_property('component_tags', [unreal.Name(t) for t in tags])


def check_frame(mesh):
    """Blend-to-mesh axis signs from the mesh bounds; raises when they match no simple mirror."""
    box = mesh.get_bounding_box()
    got_min = (box.min.x, box.min.y, box.min.z)
    got_max = (box.max.x, box.max.y, box.max.z)
    OUT['mesh_bounds_cm'] = [got_min, got_max]
    signs = []
    for axis in range(3):
        lo, hi = BLEND_MIN[axis] * 100.0, BLEND_MAX[axis] * 100.0
        if abs(got_min[axis] - lo) < 6.0 and abs(got_max[axis] - hi) < 6.0:
            signs.append(1.0)
        elif abs(got_min[axis] + hi) < 6.0 and abs(got_max[axis] + lo) < 6.0:
            signs.append(-1.0)
        else:
            raise RuntimeError('mesh bounds %s..%s do not match the blend on axis %d' % (got_min, got_max, axis))
    if signs[2] != 1.0:
        raise RuntimeError('mesh is upside down against the blend')
    log('blend->mesh axis signs %s' % signs)
    return signs


# ---------------------------------------------------------------------------------------------------------------
# Phase "import": the trolley skeletal mesh + its loop from the Blender build (like ue_rusfactory_a2_import.py)
# ---------------------------------------------------------------------------------------------------------------
ASSETS = unreal.EditorAssetLibrary


def save_asset(path):
    obj = unreal.load_asset(path)
    if obj is None:
        raise RuntimeError('nothing to save at ' + path)
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + path)


def legacy_import(task):
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')


def fbx_task(filename, destination, name, ui):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination
    if name:
        task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    return task


def trolley_ui(skeleton, with_mesh):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', with_mesh)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_as_skeletal', True)
    ui.set_editor_property('import_animations', not with_mesh)
    ui.set_editor_property('mesh_type_to_import',
                           unreal.FBXImportType.FBXIT_SKELETAL_MESH if with_mesh else unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property('original_import_type', unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property('create_physics_asset', False)
    if skeleton is not None:
        ui.set_editor_property('skeleton', skeleton)
    data = ui.get_editor_property('skeletal_mesh_import_data')
    data.set_editor_property('import_morph_targets', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    anim = ui.get_editor_property('anim_sequence_import_data')
    anim.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    anim.set_editor_property('remove_redundant_keys', False)
    return ui


def import_trolley():
    mesh_path = HOME + '/' + TROLLEY
    skeleton_path = mesh_path + '_Skeleton'
    legacy_import(fbx_task(CFG['fbx'], HOME, TROLLEY, trolley_ui(unreal.load_asset(skeleton_path), True)))
    mesh = unreal.load_asset(mesh_path)
    skeleton = unreal.load_asset(skeleton_path)
    if mesh is None or skeleton is None:
        raise RuntimeError('trolley import made no mesh / skeleton')
    legacy_import(fbx_task(CFG['fbx'], SCRATCH, None, trolley_ui(skeleton, False)))
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([SCRATCH], True)
    # One take in the FBX: the legacy importer names it <fbx name>_Anim, not after the take.
    sequences = [d.get_asset() for d in registry.get_assets_by_path(SCRATCH, recursive=True)
                 if isinstance(d.get_asset(), unreal.AnimSequence)]
    if len(sequences) != 1:
        raise RuntimeError('expected the one take %s in %s, found %s' % (CFG['take'], SCRATCH,
                                                                         [a.get_name() for a in sequences]))
    take = sequences[0]
    target = HOME + '/' + ANIM
    old = unreal.load_asset(target)
    if old is not None:
        if not ASSETS.consolidate_assets(take, [old]):
            raise RuntimeError('could not replace ' + target)
        ASSETS.delete_asset(target)
    if not ASSETS.rename_loaded_asset(take, target):
        raise RuntimeError('could not move the take to ' + target)
    registry.scan_paths_synchronous([SCRATCH], True)
    for data in registry.get_assets_by_path(SCRATCH, recursive=True):
        ASSETS.delete_loaded_asset(data.get_asset())
    ASSETS.delete_directory(SCRATCH)
    slots = []
    for slot in mesh.get_editor_property('materials'):
        name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in CFG['materials'] if name.startswith(k)), None)
        if key is None:
            raise RuntimeError('no material for trolley slot ' + name)
        material = unreal.load_asset(CFG['materials'][key])
        if material is None:
            raise RuntimeError('missing material ' + CFG['materials'][key])
        slot.set_editor_property('material_interface', material)
        slots.append(slot)
    mesh.set_editor_property('materials', slots)
    mesh.set_editor_property('positive_bounds_extension', TROLLEY_BOUNDS_CM)
    mesh.set_editor_property('negative_bounds_extension', TROLLEY_BOUNDS_CM)
    for path in (mesh_path, skeleton_path, target):
        save_asset(path)
    anim = unreal.load_asset(target)
    bounds = mesh.get_bounds()
    OUT['trolley_import'] = {
        'mesh': mesh_path, 'anim': target, 'anim_length_s': round(anim.get_play_length(), 3),
        'anim_frames': anim.get_editor_property('number_of_sampled_frames') if hasattr(anim, 'get_editor_property') else None,
        'materials': [str(s.get_editor_property('material_slot_name')) + ' -> ' +
                      s.get_editor_property('material_interface').get_name() for s in slots],
        'physics_asset': str(mesh.get_editor_property('physics_asset')),
        'bounds_origin': str(bounds.origin), 'bounds_extent': str(bounds.box_extent)}
    log('imported trolley: %s' % OUT['trolley_import'])
    return mesh, anim


def quiet(comp):
    comp.set_collision_profile_name('NoCollision')
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('generate_overlap_events', False)


def export_t3d(obj):
    task = unreal.AssetExportTask()
    task.object = obj
    task.filename = GRAPH_DUMP + obj.get_name() + '.t3d'
    task.exporter = unreal.ObjectExporterT3D()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    ok = bool(unreal.Exporter.run_asset_export_task(task))
    raw = open(task.filename, 'rb').read() if ok else b''
    text = raw.decode('utf-16') if raw[:2] == bytes((0xFF, 0xFE)) else raw.decode('utf-8', 'replace')
    return ok, text


def dump_graph(bp):
    """T3D of the Blueprint and its Blueprint parents, for the construction / pack-up / death visibility check.

    Blueprint.parent_class is not readable from python: the parent comes from the exported text's ParentClass line."""
    import os
    import re
    os.makedirs(GRAPH_DUMP, exist_ok=True)
    out = {}
    obj = bp
    for _ in range(4):
        ok, text = export_t3d(obj)
        match = re.search(r"ParentClass=[^']*'([^']+)'", text)
        out[obj.get_name()] = {'exported': ok, 'parent': match.group(1) if match else None}
        if not match or not match.group(1).startswith('/Game/'):
            break
        obj = unreal.load_asset(match.group(1).rsplit('.', 1)[0])
        if not isinstance(obj, unreal.Blueprint):
            break
    OUT['graph_dump'] = out


def main():
    if not DRY:
        trolley_mesh, trolley_anim = import_trolley()
    bp = unreal.load_asset(BP_PATH)
    if bp is None:
        raise RuntimeError('missing ' + BP_PATH)
    rows = gather(bp)
    OUT['before'] = describe(rows)
    root = next((r for r in rows if LIB.is_root_component(r['data'])), None)
    if root is None:
        raise RuntimeError('no root component')
    actor_handle = rows[0]['handle']
    mesh_row = find(rows, 'BuildingMeshComponent')
    if mesh_row is None:
        raise RuntimeError('no BuildingMeshComponent')
    mesh_comp = mesh_row['obj']
    static_mesh = mesh_comp.get_editor_property('static_mesh')
    if static_mesh is None or static_mesh.get_name() != 'SM_Rus_FactoryPlatform':
        raise RuntimeError('BuildingMeshComponent holds %s, expected SM_Rus_FactoryPlatform' % static_mesh)
    signs = check_frame(static_mesh)
    if root['name'] == 'BuildingMeshComponent':
        mesh_xf = unreal.Transform()
    else:
        mesh_xf = unreal.Transform(mesh_comp.get_editor_property('relative_location'),
                                   mesh_comp.get_editor_property('relative_rotation'),
                                   mesh_comp.get_editor_property('relative_scale3d'))
    log('root=%s mesh_rel=%s' % (root['name'], mesh_xf))

    def blend_to_rel(p):
        local = unreal.Vector(p[0] * 100.0 * signs[0], p[1] * 100.0 * signs[1], p[2] * 100.0 * signs[2])
        return mesh_xf.transform_location(local)

    # The mouth faces +x in the blend; in BP space that is the mesh's +x times its sign and relative rotation.
    out_dir = mesh_xf.transform_direction(unreal.Vector(signs[0], 0.0, 0.0))
    out_yaw = unreal.MathLibrary.make_rot_from_x(out_dir).yaw
    exit_loc = blend_to_rel(EXIT_BLEND)
    # Past the nav modifier and the selection box, whichever reaches further out along the mouth direction.
    reach = 0.0
    for row in rows:
        obj = row['obj']
        if isinstance(obj, unreal.BoxComponent):
            ext = obj.get_editor_property('box_extent')
            loc = obj.get_editor_property('relative_location')
            reach = max(reach, abs(loc.x) + ext.x * obj.get_editor_property('relative_scale3d').x)
    needed = reach + EXIT_NAV_MARGIN_CM
    along = exit_loc.x * out_dir.x + exit_loc.y * out_dir.y
    if along < needed:
        log('exit moved out from %.0f to %.0f cm (box reach %.0f)' % (along, needed, reach))
        exit_loc = unreal.Vector(exit_loc.x + out_dir.x * (needed - along), exit_loc.y + out_dir.y * (needed - along),
                                 exit_loc.z)
    OUT['plan'] = {'exit': str(exit_loc), 'exit_yaw': out_yaw, 'work_spot': str(blend_to_rel(WORK_SPOT)),
                   'box_reach': reach}
    if DRY:
        log('dry run: nothing changed')
        return

    trainer = add(bp, rows, actor_handle, unreal.EnemyBuildingTrainerComponent, 'EnemyBuildingTrainer')
    trainer.set_editor_property('trainer_kind', unreal.EnemyBuildingTrainerKind.VEHICLES)
    trainer.set_editor_property('idle_animation', unreal.EnemyTrainerIdleAnimation.FINISH_CYCLE)
    trainer.set_editor_property('training_time_mlt', 1.0)
    trainer.set_editor_property('exit_point_component_name', unreal.Name('AITrainingExit'))

    exit_comp = add(bp, rows, root['handle'], unreal.SceneComponent, 'AITrainingExit')
    exit_comp.set_editor_property('relative_location', exit_loc)
    exit_comp.set_editor_property('relative_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=out_yaw))

    for name, lamp, aim, color, candela, inner, outer, radius in SPOTS:
        light = add(bp, rows, root['handle'], unreal.SpotLightComponent, name)
        start, target = blend_to_rel(lamp), blend_to_rel(aim)
        light.set_editor_property('relative_location', start)
        light.set_editor_property('relative_rotation', unreal.MathLibrary.find_look_at_rotation(start, target))
        light.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
        light.set_editor_property('intensity', candela)
        light.set_editor_property('light_color', unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
        light.set_editor_property('inner_cone_angle', inner)
        light.set_editor_property('outer_cone_angle', outer)
        light.set_editor_property('attenuation_radius', radius)
        light.set_editor_property('source_radius', 15.0)
        light.set_editor_property('cast_shadows', False)
        light.set_editor_property('visible', False)
        tag(light)

    for name, pos, color, candela, radius in POINTS:
        light = add(bp, rows, root['handle'], unreal.PointLightComponent, name)
        light.set_editor_property('relative_location', blend_to_rel(pos))
        light.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
        light.set_editor_property('intensity', candela)
        light.set_editor_property('light_color', unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
        light.set_editor_property('attenuation_radius', radius)
        light.set_editor_property('source_radius', 20.0)
        light.set_editor_property('cast_shadows', False)
        light.set_editor_property('visible', False)
        tag(light)

    sparks = unreal.load_asset(SPARKS)
    if sparks is None:
        raise RuntimeError('missing ' + SPARKS)
    for name, pos, scale in EFFECTS:
        fx = add(bp, rows, root['handle'], unreal.NiagaraComponent, name)
        fx.set_editor_property('relative_location', blend_to_rel(pos))
        fx.set_editor_property('relative_scale3d', unreal.Vector(scale, scale, scale))
        fx.set_editor_property('asset', sparks)
        fx.set_editor_property('auto_activate', False)
        tag(fx)

    # Concept B: the trolley (tagged: the trainer plays it from frame 0 and lets it finish its cycle) + the hull.
    trolley = add(bp, rows, root['handle'], unreal.SkeletalMeshComponent, 'AI_Trolley')
    trolley.set_editor_property('relative_location', unreal.Vector(0.0, 0.0, 0.0))
    trolley.set_editor_property('relative_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    if mesh_xf.translation.length() > 0.01 or abs(mesh_xf.rotation.rotator().yaw) > 0.01:
        raise RuntimeError('the trolley is built in mesh space: BuildingMeshComponent must be the root at identity')
    trolley.set_editor_property('skeletal_mesh_asset', trolley_mesh)
    trolley.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    play = unreal.SingleAnimationPlayData()
    play.set_editor_property('anim_to_play', trolley_anim)
    play.set_editor_property('saved_looping', True)
    play.set_editor_property('saved_playing', False)
    play.set_editor_property('saved_position', 0.0)
    play.set_editor_property('saved_play_rate', 1.0)
    trolley.set_editor_property('animation_data', play)
    # AlwaysTickPose only refreshes bones while rendered: one-shot captures would show the previous capture's pose.
    trolley.set_editor_property('visibility_based_anim_tick_option',
                                unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    quiet(trolley)
    tag(trolley)

    hull_cfg = CFG['hull']
    hull = add(bp, rows, root['handle'], unreal.StaticMeshComponent, 'AI_AssemblyHull')
    hull_mesh = unreal.load_asset(hull_cfg['mesh'])
    hull.set_editor_property('static_mesh', hull_mesh)
    hull.set_editor_property('override_materials', [unreal.load_asset(hull_cfg['material'])])
    hull.set_editor_property('relative_location', unreal.Vector(*hull_cfg['location_ue_cm']))
    hull.set_editor_property('relative_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=hull_cfg['yaw_ue_deg']))
    quiet(hull)
    socket = hull_mesh.find_socket('Turret')
    seat = unreal.Vector(*hull_cfg['location_ue_cm']) + socket.get_editor_property('relative_location')
    OUT['hull_turret_socket_rel'] = str(seat)
    if (seat - unreal.Vector(*CFG['seat_ue_cm'])).length() > 1.0:
        raise RuntimeError('hull Turret socket at %s, the trolley seats the turret at %s' % (seat, CFG['seat_ue_cm']))

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    after_rows = gather(bp)
    OUT['after'] = describe(after_rows)
    for needed in ('AITrainingExit', 'EnemyBuildingTrainer', 'AI_Trolley', 'AI_AssemblyHull'):
        if find(after_rows, needed) is None:
            raise RuntimeError(needed + ' missing after compile')
    if not unreal.EditorAssetLibrary.save_loaded_asset(bp, False):
        raise RuntimeError('save failed')
    OUT['dirty_after_save'] = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    log('saved')
    t = find(after_rows, 'AI_Trolley')['obj']
    data = t.get_editor_property('animation_data')
    anim = data.get_editor_property('anim_to_play')
    OUT['trolley_template'] = {
        'mode': str(t.get_editor_property('animation_mode')), 'anim': anim.get_path_name() if anim else None,
        'looping': data.get_editor_property('saved_looping'), 'playing': data.get_editor_property('saved_playing'),
        'position': data.get_editor_property('saved_position'),
        'tick': str(t.get_editor_property('visibility_based_anim_tick_option')),
        'collision': str(t.get_collision_profile_name()), 'nav': t.get_editor_property('can_ever_affect_navigation'),
        'tags': [str(x) for x in t.get_editor_property('component_tags')], 'visible': t.get_editor_property('visible')}
    h = find(after_rows, 'AI_AssemblyHull')['obj']
    OUT['hull_template'] = {'collision': str(h.get_collision_profile_name()),
                            'nav': h.get_editor_property('can_ever_affect_navigation'),
                            'tags': [str(x) for x in h.get_editor_property('component_tags')],
                            'materials': [m.get_name() for m in h.get_editor_property('override_materials')]}
    try:
        dump_graph(bp)
    except Exception as exc:
        OUT['graph_dump_error'] = repr(exc)


try:
    main()
    OUT['ok'] = True
except Exception as exc:  # report and keep the commandlet exit clean
    OUT['ok'] = False
    OUT['error'] = repr(exc)
    import traceback
    OUT['trace'] = traceback.format_exc()
unreal.log('PF_TRAINER_RESULT ' + json.dumps(OUT, default=str))
