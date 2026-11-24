"""Ger_OderWarthe Act 1 clue spots -> /Game/RTS_Survival/Blueprints/Environment/OderWarthe/Clues (headless).

    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file> [steps]" -unattended -nullrhi
        -nosplash -nosound -DisablePlugins=Fab,BpGeneratorUltimate
steps (comma list, default all): textures,materials,meshes,blueprints

Sources: Blender generator D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Clues/Model/clue_build.py
(FBX/, clue_materials.json); decal textures from gen_ow_clue_decals.py (Saved/OderWarthe/clues/decals).
Only the Clues folder is written; the kit MIs, the Ore decal master, the phytophage and the pack props are reused as is.

Blueprints (not placed in any map):
  BP_OW_Clue_QuarantineWagon  origin = the OW rail yard loading ramp's origin (RailYard_Ramp; ow_rail.ramp_transform()
                              x, y, yaw; z = the ramp actor's z). Ramp-local frame: track centre X -260 along Y, buffer
                              stop face Y +775, ground z 0, deck 125, side platform from X -70 outward (+X = the RTS
                              camera side). Wagon coupled against the buffer, rail top z 37.
  BP_OW_Clue_SamplingCamp     origin = world (-7000, -5400, 0): the Mittelsee waterline (water z 0) on the west shore by
                              the island causeway's south end. Prop heights come from the map heightmap at that spot;
                              moving the camp needs the heights redone (ground_z here).
Each has a ChildActorComponent "PortraitTrigger" (BP_ScriptTrigger_PortraitLine, announcer portrait, no voice line yet)
on ground units reach, and a SceneComponent "TriggerSlot" marking the same spot.
"""
import json
import math
import os
import sys

import unreal

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Clues/'
DECAL_SRC = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/clues/decals/'
OWT = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/terrain/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Clues'
KIT = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit'
ORE_DECALS = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Decals/'
PHYTO = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Phytophage/MI_Phytophage_BlackPlum_Solid'
TRIGGER_BP = '/Game/RTS_Survival/Blueprints/Missions/TriggerAreas/BP_ScriptTrigger_PortraitLine'
MANIFEST = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/clues/ow_clues.json'

GRAINBAG_1 = '/Game/Modular_MedievalTown_WI/Meshes/Props/SM_GrainBag_1'
GRAINBAG_2 = '/Game/Modular_MedievalTown_WI/Meshes/Props/SM_GrainBag_2'
CRATE_SMALL = '/Game/MilitaryBaseCamp/Meshes/SM_CrateSmall'
BARREL = '/Game/MilitaryBaseCamp/Meshes/SM_Barrel'
BOAT = '/Game/Greek_island/Assets/Boat/SM_Boat_02'

DECALS = ['Quarantine', 'Grain', 'WagonNo', 'RoofX', 'Notice', 'WaterWarn', 'NoWater', 'Station', 'Crate']
MESHES = ['Teplushka', 'GrainSpill', 'Barrier', 'SignBoard', 'SignPost', 'RidgeTent', 'FieldTable', 'JarRack',
          'JarsLoose', 'Carboy', 'StaffGauge', 'FloatLine', 'FirePit', 'AlgaePuddle', 'Oar']
COLLIDE = {'Teplushka', 'RidgeTent', 'SignBoard'}         # keep their UCX hulls; the rest are walk-through dressing
NAV = {'Teplushka', 'RidgeTent'}

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
SMS = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
LOG = []
MAN = {}


def log(*a):
    line = ' '.join(str(x) for x in a)
    LOG.append(line)
    unreal.log_warning('OWCLUE ' + line)


def save(asset):
    ok = EAL.save_loaded_asset(asset, False)
    if not ok:
        log('SAVE FAILED', asset.get_path_name())
    return ok


# ---- textures + decal materials --------------------------------------------------------------------------------------
def import_texture(name, srgb):
    task = unreal.AssetImportTask()
    task.filename = DECAL_SRC + name + '.png'
    task.destination_path = HOME + '/Decals/Textures'
    task.destination_name = name
    task.automated, task.save, task.replace_existing = True, False, True
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(HOME + '/Decals/Textures/' + name)
    tex.set_editor_property('srgb', srgb)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7 if srgb else
                            unreal.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    save(tex)
    return tex


def step_textures():
    parent = unreal.load_asset(ORE_DECALS + 'Materials/M_OreCliffDecal')
    flat_n = unreal.load_asset(ORE_DECALS + 'Textures/T_OreCliffDecalV2_Flat_N')
    made = []
    for name in DECALS:
        bca = import_texture('T_OWClue_%s_BCA' % name, True)
        rmao = import_texture('T_OWClue_%s_RMAO' % name, False)
        mi_name = 'MI_OWClueDecal_' + name
        path = HOME + '/Decals/' + mi_name
        mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
            mi_name, HOME + '/Decals', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, parent)
        MEL.set_material_instance_texture_parameter_value(mi, 'BaseColour_Opacity', bca)
        MEL.set_material_instance_texture_parameter_value(mi, 'Normal', flat_n)
        MEL.set_material_instance_texture_parameter_value(mi, 'RMAO', rmao)
        MEL.set_material_instance_scalar_parameter_value(mi, 'OpacityLerpB', 1.0)
        MEL.set_material_instance_scalar_parameter_value(mi, 'BaseColour_OpacityScale', 1.45)
        MEL.set_material_instance_scalar_parameter_value(mi, 'BaseColour_Brightness', 0.9)
        MEL.set_material_instance_scalar_parameter_value(mi, 'AngleFadeAmount', 0.0)
        MEL.update_material_instance(mi)
        save(mi)
        made.append(path)
    log('decals', made)


# ---- surface materials ----------------------------------------------------------------------------------------------
def step_materials():
    cfgs = json.load(open(SRC + 'clue_materials.json'))
    kit_master = unreal.load_asset(KIT + '/Materials/M_OW_Kit')
    out = []
    for name, cfg in cfgs.items():
        path = HOME + '/Materials/' + name
        mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
            name, HOME + '/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        if name == 'MI_OWClue_AlgaeSample':
            # the Fitofag sample: the phytophage solid look (wet purple, fibre hairs), tinted to the Mittelsee algae
            # (MI_OderWarthe_AlgaeWater Color_1 0.22/0.12/0.24, shore paint PripAlgae_Tint 0.34/0.20/0.40)
            MEL.set_material_instance_parent(mi, unreal.load_asset(PHYTO))
            MEL.set_material_instance_vector_parameter_value(mi, 'FibreTint', unreal.LinearColor(0.17, 0.07, 0.20, 1))
            MEL.set_material_instance_vector_parameter_value(mi, 'DarkTint', unreal.LinearColor(0.035, 0.012, 0.045, 1))
            MEL.set_material_instance_scalar_parameter_value(mi, 'WetRoughness', 0.08)
            MEL.set_material_instance_scalar_parameter_value(mi, 'DryRoughness', 0.35)
        else:
            MEL.set_material_instance_parent(mi, kit_master)
            tex_path = cfg.get('ue_tex') or (KIT + '/Textures/' + cfg['tex'])
            tex = unreal.load_asset(tex_path)
            if tex is None:
                raise RuntimeError('missing texture ' + tex_path)
            MEL.set_material_instance_texture_parameter_value(mi, 'BaseTex', tex)
            t = cfg.get('ue_tint') or cfg['tint']
            MEL.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(t[0], t[1], t[2], 1.0))
            MEL.set_material_instance_scalar_parameter_value(mi, 'Roughness', cfg['rough'])
            MEL.set_material_instance_scalar_parameter_value(mi, 'Metallic', cfg['metal'])
            MEL.set_material_instance_scalar_parameter_value(mi, 'Tiling', 1.0)
            if name == 'MI_OWClue_Glass':
                MEL.set_material_instance_scalar_parameter_value(mi, 'Specular', 0.9)
                MEL.set_material_instance_scalar_parameter_value(mi, 'MacroDark', 0.95)
        MEL.update_material_instance(mi)
        save(mi)
        out.append(path)
    log('materials', out)


def mats_by_name():
    out = {}
    for name in ('MI_OW_Wood', 'MI_OW_RustSteel', 'MI_OW_DarkSteel', 'MI_OW_Void', 'MI_OW_Granite'):
        out[name] = unreal.load_asset(KIT + '/Materials/' + name)
    for name in json.load(open(SRC + 'clue_materials.json')):
        out[name] = unreal.load_asset(HOME + '/Materials/' + name)
    return out


# ---- meshes ---------------------------------------------------------------------------------------------------------
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


def step_meshes():
    mats = mats_by_name()
    rows = {}
    for key in MESHES:
        name = 'SM_OW_Clue_' + key
        task = unreal.AssetImportTask()
        task.filename = SRC + 'FBX/' + name + '.fbx'
        task.destination_path = HOME + '/Meshes'
        task.destination_name = name
        task.automated, task.save, task.replace_existing, task.options = True, False, True, fbx_ui()
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
        try:
            TOOLS.import_asset_tasks([task])
        finally:
            unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
        mesh = unreal.load_asset(HOME + '/Meshes/' + name)
        if mesh is None:
            raise RuntimeError('import failed ' + name)
        ns = mesh.get_editor_property('nanite_settings')
        ns.enabled = False
        mesh.set_editor_property('nanite_settings', ns)
        slots = [str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('static_materials')]
        for i, sn in enumerate(slots):
            k = next((m for m in sorted(mats, key=len, reverse=True) if sn.startswith(m)), None)
            if k is None:
                raise RuntimeError('%s: slot %s has no material' % (name, sn))
            mesh.set_material(i, mats[k])
        mesh.set_editor_property('light_map_coordinate_index', 1)
        if key not in COLLIDE:
            SMS.remove_collisions(mesh)
        body = mesh.get_editor_property('body_setup')
        if body is not None:
            body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
        save(mesh)
        b = mesh.get_bounding_box()
        ag = body.get_editor_property('agg_geom') if body else None
        rows[name] = dict(bounds=[[round(b.min.x), round(b.min.y), round(b.min.z)], [round(b.max.x), round(b.max.y),
                                                                                    round(b.max.z)]],
                          tris=mesh.get_num_triangles(0), slots=slots,
                          convex=len(ag.get_editor_property('convex_elems')) if ag else 0)
        log('mesh', name, json.dumps(rows[name]))
    MAN['meshes'] = rows


# ---- heights for the camp -------------------------------------------------------------------------------------------
_H = {}


def ground_z(x, y):
    """Landscape height (cm) from the map generator's heightmap, bilinear (as ow_map_build.ground_z)."""
    if not _H:
        lay = json.load(open(OWT + 'OW_layout.json'))['landscape']
        import array
        _H['lay'] = lay
        _H['h'] = array.array('H', open(OWT + 'OW_height.raw', 'rb').read())
    lay, H = _H['lay'], _H['h']
    vx, vy = lay['verts_x'], lay['verts_y']
    fx = min(max((x - lay['corner'][0]) / lay['quad_cm'], 0.0), vx - 1.001)
    fy = min(max((y - lay['corner'][1]) / lay['quad_cm'], 0.0), vy - 1.001)
    ix, iy = int(math.floor(fx)), int(math.floor(fy))
    tx, ty = fx - ix, fy - iy

    def h(i, j):
        return (H[j * vx + i] - 32768.0) * 100.0 / 128.0

    return (h(ix, iy) * (1 - tx) * (1 - ty) + h(ix + 1, iy) * tx * (1 - ty) + h(ix, iy + 1) * (1 - tx) * ty +
            h(ix + 1, iy + 1) * tx * ty)


CAMP_ORIGIN = (-7000.0, -5400.0)


def cz(x, y):
    return round(max(0.0, ground_z(CAMP_ORIGIN[0] + x, CAMP_ORIGIN[1] + y)), 1)


# ---- blueprint plumbing ---------------------------------------------------------------------------------------------
def _named(bp):
    out = {}
    for handle in SDS.k2_gather_subobject_data_for_blueprint(bp):
        data = SDL.get_data(handle)
        template = SDL.get_object(data)
        if template is None or isinstance(template, unreal.Actor):
            out.setdefault('__actor__', (handle, template))
            continue
        out[str(SDL.get_variable_name(data))] = (handle, template)
        if isinstance(template, unreal.SceneComponent) and SDL.is_root_component(data):
            out['__root__'] = (handle, template)
    return out


def _add(bp, parent_handle, cls, name):
    handle, fail = SDS.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=cls,
                                                                      blueprint_context=bp))
    if not fail.is_empty():
        raise RuntimeError('%s: adding %s failed: %s' % (bp.get_name(), name, fail))
    SDS.rename_subobject(handle, unreal.Text(name))
    return handle


def fresh_blueprint(name):
    """Create the Blueprint, or empty an existing one down to its root (the builder owns every component)."""
    path = HOME + '/' + name
    if EAL.does_asset_exist(path):
        bp = unreal.load_asset(path)
    else:
        f = unreal.BlueprintFactory()
        f.set_editor_property('parent_class', unreal.Actor)
        bp = TOOLS.create_asset(name, HOME, unreal.Blueprint, f)
    named = _named(bp)
    actor_h = named['__actor__'][0]
    if '__root__' not in named:
        _add(bp, actor_h, unreal.SceneComponent, 'DefaultSceneRoot')
        named = _named(bp)
    root_h, root = named['__root__']
    for k, (h, t) in list(named.items()):
        if k in ('__actor__', '__root__') or t == root:
            continue
        try:
            SDS.delete_subobject(actor_h, h, bp)
        except Exception as e:  # noqa: BLE001
            log('delete failed', k, e)
    return bp, root_h


def _place(t, loc, rot=(0, 0, 0), scale=(1, 1, 1)):
    t.set_editor_property('relative_location', unreal.Vector(*loc))
    t.set_editor_property('relative_rotation', unreal.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]))
    t.set_editor_property('relative_scale3d', unreal.Vector(*scale))


def add_mesh(bp, root_h, name, mesh_path, loc, rot=(0, 0, 0), scale=(1, 1, 1), collide=False, nav=False):
    h = _add(bp, root_h, unreal.StaticMeshComponent, name)
    t = SDL.get_object(SDL.get_data(h))
    mesh = unreal.load_asset(mesh_path)
    if mesh is None:
        raise RuntimeError('missing mesh ' + mesh_path)
    t.set_editor_property('static_mesh', mesh)
    _place(t, loc, rot, scale)
    t.set_editor_property('mobility', unreal.ComponentMobility.STATIC)
    if not collide:
        t.set_collision_profile_name('NoCollision')
        t.set_editor_property('can_ever_affect_navigation', False)
    else:
        t.set_collision_profile_name('BlockAll')
        t.set_editor_property('can_ever_affect_navigation', nav)
    t.set_editor_property('cast_shadow', True)
    return t


def add_decal(bp, root_h, name, decal, loc, yaw, w, h, depth=40.0, down=False, sort=3):
    """Wall decal: projects along its X into a wall whose outward normal is -X(yaw) ; roll 90 stands the texture up
    (Ore convention: size = depth, h/2, w/2). down=True: projects straight down (size = depth, w/2, h/2)."""
    hd = _add(bp, root_h, unreal.DecalComponent, name)
    t = SDL.get_object(SDL.get_data(hd))
    t.set_editor_property('decal_material', unreal.load_asset(HOME + '/Decals/MI_OWClueDecal_' + decal))
    if down:
        _place(t, loc, (0.0, -90.0, yaw))
        t.set_editor_property('decal_size', unreal.Vector(depth, w / 2, h / 2))
    else:
        _place(t, loc, (90.0, 0.0, yaw))
        t.set_editor_property('decal_size', unreal.Vector(depth, h / 2, w / 2))
    t.set_editor_property('sort_order', sort)
    return t


def add_trigger(bp, root_h, loc, radius):
    slot = _add(bp, root_h, unreal.SceneComponent, 'TriggerSlot')
    _place(SDL.get_object(SDL.get_data(slot)), loc)
    h = _add(bp, slot, unreal.ChildActorComponent, 'PortraitTrigger')
    t = SDL.get_object(SDL.get_data(h))
    cls = EAL.load_blueprint_class(TRIGGER_BP)
    t.set_editor_property('child_actor_class', cls)
    info = {'class': TRIGGER_BP, 'radius_cm': radius, 'slot': list(loc)}
    try:
        tmpl = t.get_editor_property('child_actor_template')
        if tmpl is not None:
            tmpl.set_editor_property('TriggerRadius', float(radius))
            info['radius_set'] = True
            try:
                tmpl.set_editor_property('PortraitType', unreal.RTSPortraitTypes.GERMAN_ANNOUNCER)
                info['portrait'] = 'GERMAN_ANNOUNCER'
            except Exception as e:  # noqa: BLE001
                info['portrait_error'] = str(e)
        else:
            info['template'] = None
    except Exception as e:  # noqa: BLE001
        info['template_error'] = str(e)
    log('trigger', bp.get_name(), json.dumps(info))
    return info


def finish_bp(bp, info):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(bp)
    comps = sorted(k for k in _named(bp) if not k.startswith('__'))
    info['components'] = comps
    log('bp', bp.get_name(), len(comps), 'components')
    return info


# ---- the two Blueprints ---------------------------------------------------------------------------------------------
M = HOME + '/Meshes/SM_OW_Clue_'
RAIL_TOP = 37.0
DECK = 125.0
WAGON_X, WAGON_Y = -260.0, 775.0 - 519.0 - 5.0     # coupled against the buffer stop face (ramp-local)
SHOW_X = WAGON_X + 155.0                             # the wagon's stencil side (outer planks), facing +X


def wagon_bp():
    bp, root = fresh_blueprint('BP_OW_Clue_QuarantineWagon')
    # mesh +X along the track; its stencil side is mesh +Y (Blender -Y): yaw -90 turns it to the platform (+X)
    add_mesh(bp, root, 'Wagon', M + 'Teplushka', (WAGON_X, WAGON_Y, RAIL_TOP), (0, 0, -90), collide=True, nav=True)
    zc = RAIL_TOP + DECK + 255.0 * 0.62
    add_decal(bp, root, 'Decal_Quarantine', 'Quarantine', (SHOW_X, WAGON_Y + 260.0, zc), 180.0, 290.0, 145.0)
    add_decal(bp, root, 'Decal_Grain', 'Grain', (SHOW_X, WAGON_Y - 260.0, zc), 180.0, 290.0, 145.0)
    add_decal(bp, root, 'Decal_WagonNo', 'WagonNo', (SHOW_X, WAGON_Y + 330.0, RAIL_TOP + DECK + 60.0), 180.0, 130.0,
              65.0)
    add_decal(bp, root, 'Decal_Notice', 'Notice', (SHOW_X + 8.0, WAGON_Y + 40.0, RAIL_TOP + DECK + 150.0), 180.0, 45.0,
              60.0, depth=25.0, sort=4)
    add_decal(bp, root, 'Decal_RoofX', 'RoofX', (WAGON_X, WAGON_Y, RAIL_TOP + 380.0 + 38.0 + 20.0), 0.0, 260.0, 260.0,
              depth=60.0, down=True)
    # sacks half unloaded onto the platform deck; one burst with mouldy grain
    for k, (x, y, yaw) in enumerate(((45.0, 445.0, 10.0), (60.0, 360.0, -25.0), (135.0, 455.0, 40.0),
                                     (40.0, -180.0, 5.0))):
        add_mesh(bp, root, 'Sack_%d' % k, GRAINBAG_2, (x, y, DECK), (0, 0, yaw))
    add_mesh(bp, root, 'Sack_Lying', GRAINBAG_2, (120.0, -250.0, DECK + 30.0), (0, 82.0, 60.0))
    add_mesh(bp, root, 'Sack_Burst', GRAINBAG_1, (150.0, -40.0, DECK + 28.0), (75.0, 0.0, 110.0))
    add_mesh(bp, root, 'GrainSpill', M + 'GrainSpill', (195.0, 15.0, DECK), (0, 0, 30.0))
    # a barrier across the top of the ramp slope and a quarantine board at the platform edge (facing the camera, +X)
    add_mesh(bp, root, 'Barrier', M + 'Barrier', (140.0, -400.0, DECK), (0, 0, 0))
    add_mesh(bp, root, 'QuarantineBoard', M + 'SignBoard', (300.0, -250.0, DECK), (0, 0, -90.0))
    add_decal(bp, root, 'Decal_Board', 'Quarantine', (304.0, -250.0, DECK + 170.0 - 32.5), 180.0, 126.0, 63.0,
              depth=12.0)
    info = {'origin': 'RailYard_Ramp actor transform (ow_rail.ramp_transform())', 'wagon': [WAGON_X, WAGON_Y,
                                                                                               RAIL_TOP]}
    info['trigger'] = add_trigger(bp, root, (800.0, 250.0, 100.0), 1300.0)
    return finish_bp(bp, info)


def camp_bp():
    bp, root = fresh_blueprint('BP_OW_Clue_SamplingCamp')
    P = lambda x, y, dz=0.0: (x, y, cz(x, y) + dz)   # noqa: E731
    add_mesh(bp, root, 'Tent', M + 'RidgeTent', P(-200.0, -1160.0), (0, 0, 90.0), collide=True, nav=True)
    add_mesh(bp, root, 'Table', M + 'FieldTable', P(60.0, -830.0), (0, 0, 0))
    tz = cz(60.0, -830.0) + 78.0
    add_mesh(bp, root, 'JarRack', M + 'JarRack', (30.0, -838.0, tz), (0, 0, 0))
    add_mesh(bp, root, 'JarsLoose', M + 'JarsLoose', (115.0, -812.0, tz), (0, 0, 30.0))
    for k, (x, y) in enumerate(((220.0, -740.0), (285.0, -700.0), (335.0, -765.0))):
        add_mesh(bp, root, 'Carboy_%d' % k, M + 'Carboy', P(x, y), (0, 0, 17.0 * k))
    for k, (x, y, yaw) in enumerate(((240.0, -920.0, 15.0), (255.0, -1000.0, -10.0))):
        add_mesh(bp, root, 'Crate_%d' % k, CRATE_SMALL, P(x, y), (0, 0, yaw), scale=(1.15, 1.15, 1.15))
    add_decal(bp, root, 'Decal_Crate', 'Crate', P(240.0, -920.0, 40.0), 15.0, 76.0, 36.0, depth=30.0, down=True)
    add_mesh(bp, root, 'Drum', BARREL, P(60.0, -1420.0), (0, 0, 40.0))
    add_mesh(bp, root, 'FirePit', M + 'FirePit', P(-450.0, -1450.0), (0, 0, 0))
    # the sampling boat pulled up on the mud, bow to the water; an oar; a purple spill where samples were poured off
    add_mesh(bp, root, 'Boat', BOAT, P(250.0, 180.0, 44.0), (2.0, -4.0, 8.0), scale=(0.6, 0.45, 0.55))
    add_mesh(bp, root, 'Oar', M + 'Oar', P(80.0, 60.0, 1.0), (0, 0, 23.0))
    add_mesh(bp, root, 'AlgaeSpill', M + 'AlgaePuddle', P(170.0, -60.0, 0.5), (0, 0, 35.0))
    # in the lake: the staff gauge and the float line (pivots at the waterline, z 0)
    add_mesh(bp, root, 'StaffGauge', M + 'StaffGauge', (450.0, 830.0, 0.0), (0, 0, -90.0))
    add_mesh(bp, root, 'FloatLine', M + 'FloatLine', (300.0, 900.0, 0.0), (0, 0, 31.0), scale=(0.97, 1, 1))
    # signs facing the RTS camera (-X); decals project +X into the board fronts
    for name, mesh, decal, x, y, post_h, w, h in (
            ('Sign_WaterWarn', 'SignBoard', 'WaterWarn', -400.0, -600.0, 170.0, 130.0, 65.0),
            ('Sign_Station', 'SignBoard', 'Station', -450.0, -1060.0, 170.0, 130.0, 65.0),
            ('Sign_NoWater', 'SignPost', 'NoWater', -150.0, 250.0, 145.0, 110.0, 55.0)):
        z = cz(x, y)
        add_mesh(bp, root, name, M + mesh, (x, y, z), (0, 0, 90.0), collide=(mesh == 'SignBoard'))
        add_decal(bp, root, 'Decal_' + decal, decal, (x - 4.0, y, z + post_h - h / 2), 0.0, w - 4.0, h - 2.0,
                  depth=12.0)
    info = {'origin_world': list(CAMP_ORIGIN) + [0.0], 'water_z': 0.0,
            'heights': {k: cz(*v) for k, v in {'tent': (-200, -1160), 'table': (60, -830), 'boat': (250, 180),
                                                 'sign_nowater': (-150, 250)}.items()}}
    info['trigger'] = add_trigger(bp, root, P(0.0, -600.0, 100.0), 1200.0)
    return finish_bp(bp, info)


def step_blueprints():
    MAN['wagon'] = wagon_bp()
    MAN['camp'] = camp_bp()


STEPS = {'textures': step_textures, 'materials': step_materials, 'meshes': step_meshes, 'blueprints': step_blueprints}

if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    steps = args[0].split(',') if args else list(STEPS)
    for s in steps:
        STEPS[s]()
    os.makedirs(os.path.dirname(MANIFEST), exist_ok=True)
    old = json.load(open(MANIFEST)) if os.path.exists(MANIFEST) else {}
    old.update(MAN)
    old['log'] = LOG
    json.dump(old, open(MANIFEST, 'w'), indent=1)
    log('DONE', steps)
