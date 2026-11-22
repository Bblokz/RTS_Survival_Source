"""Zeltpodest (concept C, picked 2026-11-22) on Rus_Building_LargeTent: plank deck + muster porch where a queue of
stand-in soldiers shows WHICH squad the tent trains (UEnemyTrainingSquadDisplayComponent).

Run headless (never in the shared editor), after ue_largetent_ai_training.py:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
      -unattended -nosplash -NoLiveCoding -DisablePlugins=Fab,BpGeneratorUltimate
Needs an editor build that contains UEnemyTrainingSquadDisplayComponent.

Adds to the PARENT only (the _Soviet child inherits), all under BuildingMeshComponent in the tent's mesh space:
  - Podest_Deck (deck ring 1.2 m past the walls, 6.7 m porch at the +X exit gable, one step), Podest_Rack + 6 tagged
    rack slots, two kit tables, the A-frame board with two icon quads (tag AITrainingSquadBoardIcon);
  - 6 queue figures (tag AITrainingSquadFigure, hidden; the display component dresses them), an NCO with a rifle behind
    the tables (tag AITrainingActivity: shown while training by the trainer);
  - Podest_FootprintBounds: a box that blocks nothing (query only, every channel ignored) so survival bases, which
    size a building by its colliding parts, keep the porch clear;
  - AITrainingSquadDisplay (the C++ display) and AITrainingExit moved to the porch foot.
Meshes come from _AssetSource/LargeTentPlateau/build_podest.py. Idempotent: components are found by name and updated.
SQUAD_ICONS is filled from the trainer test's spawner audit (RTS_EBT audit_icon lines).
"""
import json
import math
import os
import traceback

import unreal

RUS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings'
PARENT_BP = RUS + '/Rus_Building_LargeTent'
CHILD_BP = RUS + '/Rus_Building_LargeTent_Soviet'
HOME = RUS + '/Tent/Podest'
SRC = 'D:/UE5Projects/RTS_Survival/_AssetSource/LargeTentPlateau/Model/'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/EnemyBuildingTrainer/LargeTent/podest_result.json'

OW_KIT = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit/Materials/MI_OW_Wood'  
ICON_DIR = '/Game/RTS_Survival/Textures/GameUI/SquadWeaponIcons/'
SOLDIER = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/SK_RussianSoldier'
RIFLE = '/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/Russian/Mosin/SM_InfWeapon_Mosin'
IDLE = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/Idle/'
IDLE_ANIMS = [IDLE + 'Idle_Rifle_Hip', IDLE + 'Idle_Rifle_Hip_Break1', IDLE + 'Idle_Rifle_Hip_Break2']
ACTIVITY = 'AITrainingActivity'
FIGURE_TAG = 'AITrainingSquadFigure'
RACK_TAG = 'AITrainingRackSlot'
ICON_TAG = 'AITrainingSquadBoardIcon'
PARENT = 'BuildingMeshComponent'

# Layout (cm, tent mesh space) — must match build_podest.py.
DECK_Z = 15.0
EXIT_LOCATION = (1720.0, -100.0, 0.0)
RACK = (1165.0, -390.0)            # rack yaw 180: its back rail faces the porch edge (-Y)
RACK_SLOTS_X = [-195.0, -105.0, -15.0, 75.0, 165.0, 235.0]
RACK_SLOT_Z = 16.0
TABLES = [(1000.0, 330.0), (1240.0, 330.0)]
BOARD = (1445.0, 210.0)
BOARD_W, BOARD_H, BOARD_LEAN = 220.0, 150.0, 22.0
ICON_SCALE = 0.95                  # icon quad is 200 x 100 cm
QUEUE = [(880.0 + 90.0 * i, 200.0) for i in range(6)]
NCO = (1120.0, 395.0)
FOOTPRINT = ((-830.0 + 1605.0) / 2.0, 0.0, 60.0, (1605.0 + 830.0) / 2.0, 440.0, 60.0)  # centre xyz, half extents
# Infantry master meshes face +Y: mesh yaw = facing yaw - 90.
QUEUE_MESH_YAW = 0.0               # queue faces +Y, towards the tables
NCO_MESH_YAW = -180.0              # NCO faces -Y, towards the queue
WEAPON_LOCATION = unreal.Vector(-16.25, 4.16, 2.2)
WEAPON_ROTATION = unreal.Rotator(roll=2.0, pitch=-1.81, yaw=-179.92)

TUNE = {
    'planks_tint': (0.92, 0.84, 0.74),
    'beams_tint': (0.46, 0.38, 0.30),
    'red_tint': (1.25, 0.20, 0.14),
    'icon_colour': (0.86, 0.82, 0.70),
    'icon_thickness_uv': 0.018,
}
# Board icon per squad (ESquadWeaponIcon names). From the spawner audit of 2026-11-22 (the squad's healthbar icon):
# Okhotnik PTRS, RedHammer PPSH, RedHamerPTRS PTRS, ToxicGuard AshmakerSmall, GhostsOfStalingrad PTRS (its spawner row is
# the Okhotnik placeholder), Kvarc77 SpekterLaser, Tucha12T Fedrov, CortexOfficer AutomatedPTRS.
# Healthbar shows None, icon picked by their main weapon: HazmatBazooka, Dyakonov, ROKS3_PPSH, LargePTRS, KorovinAssault.
# Not listed (rifles, engineers, crews) -> the fallback SKS rifle icon.
SQUAD_ICONS = {
    'Squad_Rus_Okhotnik': 'PTRS', 'Squad_Rus_RedHammer': 'PPSH', 'Squad_Rus_RedHamerPTRS': 'PTRS',
    'Squad_Rus_ToxicGuard': 'AshmakerSmall', 'Squad_Rus_GhostsOfStalingrad': 'PTRS', 'Squad_Rus_Kvarc77': 'SpekterLaser',
    'Squad_Rus_Tucha12T': 'Fedrov', 'Squad_Rus_CortexOfficer': 'AutomatedPTRS',
    'Squad_Rus_HazmatBazooka': 'PanzerShrek', 'Squad_Rus_Dyakonov': 'GrenadeRifle', 'Squad_Rus_ROKS3_PPSH': 'AshmakerSmall',
    'Squad_Rus_LargePTRS': 'PTRS', 'Squad_Rus_KorovinAssault': 'PPSH',
}

SUBS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
LIB = unreal.SubobjectDataBlueprintFunctionLibrary
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
R = {'errors': [], 'created': [], 'updated': [], 'log': []}


def log(msg):
    R['log'].append(msg)
    unreal.log('[Podest] ' + msg)


def V(x, y, z):
    return unreal.Vector(x, y, z)


def lin(c):
    return unreal.LinearColor(c[0], c[1], c[2], 1.0)


# ---------------------------------------------------------------- assets
def import_meshes():
    EAL.make_directory(HOME)
    tasks = []
    for name in ('SM_RusTentPodest', 'SM_RusTentPodest_Rack', 'SM_RusTentPodest_Table', 'SM_RusTentPodest_Board',
                 'SM_RusTentPodest_BoardIcon'):
        o = unreal.FbxImportUI()
        o.import_mesh = True
        o.import_materials = False
        o.import_textures = False
        o.import_as_skeletal = False
        o.static_mesh_import_data.import_uniform_scale = 1.0
        o.static_mesh_import_data.combine_meshes = True
        o.static_mesh_import_data.generate_lightmap_u_vs = False
        o.static_mesh_import_data.auto_generate_collision = False
        t = unreal.AssetImportTask()
        t.filename = SRC + name + '.fbx'
        t.destination_path = HOME
        t.automated = True
        t.replace_existing = True
        t.save = True
        t.options = o
        tasks.append(t)
    TOOLS.import_asset_tasks(tasks)
    for t in tasks:
        log('imported %s' % list(t.imported_object_paths))


def make_mi(name, parent_path, vectors, scalars, textures=None):
    path = HOME + '/' + name
    mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else None
    if mi is None:
        mi = TOOLS.create_asset(name, HOME, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        R['created'].append(path)
    MEL.set_material_instance_parent(mi, unreal.load_asset(parent_path))
    for key, value in vectors.items():
        MEL.set_material_instance_vector_parameter_value(mi, key, lin(value))
    for key, value in scalars.items():
        MEL.set_material_instance_scalar_parameter_value(mi, key, value)
    for key, value in (textures or {}).items():
        MEL.set_material_instance_texture_parameter_value(mi, key, unreal.load_asset(value))
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi, False)
    return mi


ICON_HLSL = '''float Alpha = Texture2DSample(Tex, TexSampler, UV).a;
for (int Ring = 1; Ring <= 3; ++Ring)
{
    for (int Step = 0; Step < 16; ++Step)
    {
        float Angle = Step * 0.392699;
        float2 Offset = float2(cos(Angle), sin(Angle) * 2.0) * Thickness * Ring / 3.0;
        Alpha = max(Alpha, Texture2DSample(Tex, TexSampler, UV + Offset).a);
    }
}
return Alpha;'''


def build_icon_master():
    """Masked two-sided icon: the healthbar icon's thin line art thickened by a 49-tap dilation of its alpha."""
    path = HOME + '/M_RusTentPodest_BoardIcon'
    if EAL.does_asset_exist(path):
        mat = unreal.load_asset(path)
        MEL.delete_all_material_expressions(mat)
    else:
        mat = TOOLS.create_asset('M_RusTentPodest_BoardIcon', HOME, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('opacity_mask_clip_value', 0.4)
    tex = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, -900, 0)
    tex.set_editor_property('parameter_name', 'IconTexture')
    tex.set_editor_property('texture', unreal.load_asset(ICON_DIR + 'T_SquadWeaponIcon_SKS'))
    uv = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, 200)
    thick = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 300)
    thick.set_editor_property('parameter_name', 'IconThickness')
    thick.set_editor_property('default_value', TUNE['icon_thickness_uv'])
    custom = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -500, 100)
    custom.set_editor_property('code', ICON_HLSL)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs = []
    for n in ('Tex', 'UV', 'Thickness'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        inputs.append(ci)
    custom.set_editor_property('inputs', inputs)
    MEL.connect_material_expressions(tex, '', custom, 'Tex')
    MEL.connect_material_expressions(uv, '', custom, 'UV')
    MEL.connect_material_expressions(thick, '', custom, 'Thickness')
    visible = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 300)
    visible.set_editor_property('parameter_name', 'IconVisible')
    visible.set_editor_property('default_value', 0.0)
    mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 200)
    MEL.connect_material_expressions(custom, '', mul, 'A')
    MEL.connect_material_expressions(visible, '', mul, 'B')
    MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_OPACITY_MASK)
    colour = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, -200)
    colour.set_editor_property('parameter_name', 'IconColour')
    colour.set_editor_property('default_value', lin(TUNE['icon_colour']))
    MEL.connect_material_property(colour, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, -50)
    rough.set_editor_property('r', 0.9)
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat, False)
    R['created'].append(path)
    return mat


def make_materials():
    planks = make_mi('MI_RusTentPodest_Planks', OW_KIT, {'Tint': TUNE['planks_tint']}, {'Tiling': 1.0})
    beams = make_mi('MI_RusTentPodest_Beams', OW_KIT, {'Tint': TUNE['beams_tint']}, {'Tiling': 1.0})
    red = make_mi('MI_RusTentPodest_BoardRed', OW_KIT, {'Tint': TUNE['red_tint']}, {'Tiling': 1.0})
    master = build_icon_master()
    icon = make_mi('MI_RusTentPodest_BoardIcon', master.get_path_name(), {}, {})
    return {'MI_RusTentPodest_Planks': planks, 'MI_RusTentPodest_Beams': beams, 'MI_RusTentPodest_BoardRed': red,
            'MI_RusTentPodest_BoardIcon': icon}


def assign_mesh_materials(mats):
    for name in ('SM_RusTentPodest', 'SM_RusTentPodest_Rack', 'SM_RusTentPodest_Table', 'SM_RusTentPodest_Board',
                 'SM_RusTentPodest_BoardIcon'):
        mesh = unreal.load_asset(HOME + '/' + name)
        slots = mesh.get_editor_property('static_materials')
        for slot in slots:
            slot_name = str(slot.get_editor_property('material_slot_name'))
            match = next((m for key, m in mats.items() if slot_name.startswith(key)), None)
            if match is None:
                R['errors'].append('%s slot %s has no material' % (name, slot_name))
                continue
            slot.set_editor_property('material_interface', match)
        mesh.set_editor_property('static_materials', slots)
        EAL.save_loaded_asset(mesh, False)
        log('%s slots %s' % (name, [str(s.get_editor_property('material_slot_name')) for s in slots]))


# ---------------------------------------------------------------- subobjects
def handles_by_name(bp):
    out = {}
    for handle in SUBS.k2_gather_subobject_data_for_blueprint(bp):
        out[str(LIB.get_variable_name(LIB.get_data(handle)))] = handle
    return out


def actor_handle(bp):
    return SUBS.k2_gather_subobject_data_for_blueprint(bp)[0]


def ensure(bp, name, cls, parent_name):
    names = handles_by_name(bp)
    if name in names:
        handle = names[name]
        R['updated'].append(name)
    else:
        parent = names[parent_name] if parent_name else actor_handle(bp)
        params = unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls, blueprint_context=bp)
        handle, fail = SUBS.add_new_subobject(params)
        if not LIB.is_handle_valid(handle):
            raise RuntimeError('add %s failed: %s' % (name, fail))
        if not SUBS.rename_subobject(handle, unreal.Text(name)):
            raise RuntimeError('rename to %s failed' % name)
        R['created'].append(name)
        handle = handles_by_name(bp)[name]
    template = LIB.get_object_for_blueprint(LIB.get_data(handle), bp)
    if template is None or template.get_class() != cls.static_class():
        raise RuntimeError('%s template %s is not %s' % (name, template, cls))
    return template


def place(comp, location, yaw=0.0, pitch=0.0, scale=(1.0, 1.0, 1.0)):
    comp.set_editor_property('relative_location', V(*location))
    comp.set_editor_property('relative_rotation', unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    comp.set_editor_property('relative_scale3d', V(*scale))


def looks_only(comp, shadow=True, tags=(), visible=True):
    comp.set_collision_profile_name('NoCollision')
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('generate_overlap_events', False)
    comp.set_editor_property('cast_shadow', shadow)
    comp.set_editor_property('component_tags', [unreal.Name(t) for t in tags])
    set_visible(comp, visible)


def set_visible(comp, value):
    try:
        comp.set_editor_property('visible', value)
    except Exception:
        comp.set_visibility(value, False)


def prop(bp, name, mesh, location, yaw=0.0, parent=PARENT, pitch=0.0, scale=(1.0, 1.0, 1.0), tags=(), shadow=True):
    comp = ensure(bp, name, unreal.StaticMeshComponent, parent)
    comp.set_editor_property('static_mesh', unreal.load_asset(HOME + '/' + mesh))
    comp.set_editor_property('override_materials', [])
    place(comp, location, yaw, pitch, scale)
    looks_only(comp, shadow, tags)
    return comp


def build_props(bp):
    deck = prop(bp, 'Podest_Deck', 'SM_RusTentPodest', (0.0, 0.0, 0.0), shadow=False)
    deck.set_editor_property('receives_decals', True)
    prop(bp, 'Podest_Rack', 'SM_RusTentPodest_Rack', (RACK[0], RACK[1], DECK_Z), yaw=180.0)
    for i, x in enumerate(RACK_SLOTS_X):
        slot = ensure(bp, 'AITraining_RackSlot%d' % i, unreal.SceneComponent, 'Podest_Rack')
        place(slot, (x, 0.0, RACK_SLOT_Z))
        slot.set_editor_property('component_tags', [unreal.Name(RACK_TAG)])
    for i, (x, y) in enumerate(TABLES):
        prop(bp, 'Podest_Table%d' % i, 'SM_RusTentPodest_Table', (x, y, DECK_Z))
    prop(bp, 'Podest_Board', 'SM_RusTentPodest_Board', (BOARD[0], BOARD[1], DECK_Z))
    lean = math.radians(BOARD_LEAN)
    d, h = BOARD_H * math.sin(lean), BOARD_H * math.cos(lean)
    for suffix, sign in (('PX', 1.0), ('NX', -1.0)):
        # Centre of the outer face plus 1.5 cm along its normal (cos lean, 0, sin lean) mirrored for -X.
        cx = sign * (d + 1.0) / 2.0 + sign * 1.5 * math.cos(lean)
        cz = h / 2.0 + 1.5 * math.sin(lean)
        prop(bp, 'AITraining_BoardIcon_' + suffix, 'SM_RusTentPodest_BoardIcon', (cx, 0.0, cz),
             yaw=0.0 if sign > 0 else 180.0, pitch=BOARD_LEAN, parent='Podest_Board',
             scale=(1.0, ICON_SCALE, ICON_SCALE), tags=(ICON_TAG,), shadow=False)


def footprint_box(bp):
    box = ensure(bp, 'Podest_FootprintBounds', unreal.BoxComponent, PARENT)
    cx, cy, cz, ex, ey, ez = FOOTPRINT
    place(box, (cx, cy, cz))
    box.set_editor_property('box_extent', V(ex, ey, ez))
    box.set_collision_profile_name('Custom')
    box.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
    box.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    box.set_editor_property('can_ever_affect_navigation', False)
    box.set_editor_property('generate_overlap_events', False)
    box.set_editor_property('hidden_in_game', True)
    set_visible(box, False)


# ---------------------------------------------------------------- soldiers
def socket_transform(skeletal_mesh, anim, time):
    socket = skeletal_mesh.find_socket('GunSocket')
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh', skeletal_mesh)
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, time, options)
    bone = unreal.AnimPoseExtensions.get_bone_pose(pose, socket.get_editor_property('bone_name'),
                                                   unreal.AnimPoseSpaces.WORLD)
    relative = unreal.Transform(socket.get_editor_property('relative_location'),
                                socket.get_editor_property('relative_rotation'),
                                socket.get_editor_property('relative_scale'))
    return relative * bone


def single_node(comp, anim):
    comp.set_editor_property('skeletal_mesh_asset', unreal.load_asset(SOLDIER))
    comp.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    data = unreal.SingleAnimationPlayData()
    data.set_editor_property('anim_to_play', anim)
    data.set_editor_property('saved_looping', True)
    data.set_editor_property('saved_playing', True)
    data.set_editor_property('saved_position', 0.0)
    data.set_editor_property('saved_play_rate', 1.0)
    comp.set_editor_property('animation_data', data)
    comp.set_editor_property('visibility_based_anim_tick_option',
                             unreal.VisibilityBasedAnimTickOption.ONLY_TICK_POSE_WHEN_RENDERED)
    comp.set_editor_property('enable_update_rate_optimizations', True)


def remove_if_present(bp, names):
    for name in names:
        handle = handles_by_name(bp).get(name)
        if handle is None:
            continue
        SUBS.delete_subobject(actor_handle(bp), handle, bp)
        R['updated'].append('removed ' + name)


def build_soldiers(bp):
    idle = unreal.load_asset(IDLE_ANIMS[0])
    for i, (x, y) in enumerate(QUEUE):
        fig = ensure(bp, 'AITraining_QueueFigure%d' % i, unreal.SkeletalMeshComponent, PARENT)
        single_node(fig, idle)
        place(fig, (x, y, DECK_Z), yaw=QUEUE_MESH_YAW)
        looks_only(fig, True, (FIGURE_TAG,), visible=False)
    # The trainer only plays tagged skeletal meshes, it shows tagged scene components (with their children): the NCO
    # and his rifle hang untagged under a tagged group, as the barracks drill squad does.
    remove_if_present(bp, ['AITraining_QueueNCORifle', 'AITraining_QueueNCO'])
    group = ensure(bp, 'AITraining_NCOGroup', unreal.SceneComponent, PARENT)
    place(group, (0.0, 0.0, 0.0))
    group.set_editor_property('component_tags', [unreal.Name(ACTIVITY)])
    set_visible(group, False)
    nco = ensure(bp, 'AITraining_NCO', unreal.SkeletalMeshComponent, 'AITraining_NCOGroup')
    single_node(nco, idle)
    place(nco, (NCO[0], NCO[1], DECK_Z), yaw=NCO_MESH_YAW)
    looks_only(nco, True, (), visible=True)
    weapon = unreal.Transform(WEAPON_LOCATION, WEAPON_ROTATION, V(1.0, 1.0, 1.0))
    rifle_xf = weapon * socket_transform(unreal.load_asset(SOLDIER), idle, 0.0)
    rifle = ensure(bp, 'AITraining_NCORifle', unreal.StaticMeshComponent, 'AITraining_NCO')
    rifle.set_editor_property('static_mesh', unreal.load_asset(RIFLE))
    rifle.set_editor_property('override_materials', [])
    rifle.set_editor_property('relative_location', rifle_xf.translation)
    rifle.set_editor_property('relative_rotation', rifle_xf.rotation.rotator())
    rifle.set_editor_property('relative_scale3d', V(1.0, 1.0, 1.0))
    looks_only(rifle, True, (), visible=True)


def enum_value(enum_type, cpp_name):
    """Python enum names split camel case (Squad_Rus_RedHammer -> SQUAD_RUS_RED_HAMMER): match without underscores."""
    wanted = cpp_name.replace('_', '').upper()
    for attr in dir(enum_type):
        if attr.replace('_', '').upper() == wanted and not attr.startswith('__'):
            return getattr(enum_type, attr)
    raise RuntimeError('%s has no %s' % (enum_type, cpp_name))


def display_and_exit(bp):
    display = ensure(bp, 'AITrainingSquadDisplay', unreal.EnemyTrainingSquadDisplayComponent, None)
    display.set_editor_property('figure_idle_animations', [unreal.load_asset(p) for p in IDLE_ANIMS])
    display.set_editor_property('fallback_icon_texture', unreal.load_asset(ICON_DIR + 'T_SquadWeaponIcon_SKS'))
    icons = unreal.Map(unreal.SquadSubtype, unreal.SquadWeaponIcon)
    for squad, icon in SQUAD_ICONS.items():
        icons[enum_value(unreal.SquadSubtype, squad)] = enum_value(unreal.SquadWeaponIcon, icon)
    display.set_editor_property('squad_weapon_icons', icons)
    exit_point = ensure(bp, 'AITrainingExit', unreal.SceneComponent, PARENT)
    place(exit_point, EXIT_LOCATION)


# ---------------------------------------------------------------- verify
def dump_instance(bp_path, location):
    cls = EAL.load_blueprint_class(bp_path)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, V(*location), unreal.Rotator(0.0, 0.0, 0.0))
    rows = {}
    for comp in actor.get_components_by_class(unreal.ActorComponent):
        name = comp.get_name()
        if not (name.startswith('AITraining') or name.startswith('Podest')):
            continue
        row = {'class': comp.get_class().get_name(), 'tags': [str(t) for t in comp.component_tags]}
        if isinstance(comp, unreal.SceneComponent):
            loc = comp.get_world_location() - V(*location)
            row['loc'] = [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)]
            row['visible'] = comp.is_visible()
        if isinstance(comp, unreal.PrimitiveComponent):
            row['collision'] = str(comp.get_collision_enabled())
        if isinstance(comp, unreal.BoxComponent):
            row['responses'] = sorted({str(comp.get_collision_response_to_channel(ch)) for ch in (
                unreal.CollisionChannel.ECC_WORLD_STATIC, unreal.CollisionChannel.ECC_WORLD_DYNAMIC,
                unreal.CollisionChannel.ECC_PAWN, unreal.CollisionChannel.ECC_VISIBILITY,
                unreal.CollisionChannel.ECC_CAMERA, unreal.CollisionChannel.ECC_PHYSICS_BODY,
                unreal.CollisionChannel.ECC_VEHICLE, unreal.CollisionChannel.ECC_DESTRUCTIBLE)})
        rows[name] = row
    actor.destroy_actor()
    return rows


def main():
    import_meshes()
    mats = make_materials()
    assign_mesh_materials(mats)
    bp = unreal.load_asset(PARENT_BP)
    build_props(bp)
    footprint_box(bp)
    build_soldiers(bp)
    display_and_exit(bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not EAL.save_loaded_asset(bp, False):
        raise RuntimeError('saving the parent failed')
    child = unreal.load_asset(CHILD_BP)
    unreal.BlueprintEditorLibrary.compile_blueprint(child)
    if not EAL.save_loaded_asset(child, False):
        raise RuntimeError('saving the child failed')
    parent_rows = dump_instance(PARENT_BP, (0.0, 0.0, 0.0))
    child_rows = dump_instance(CHILD_BP, (0.0, 8000.0, 0.0))
    R['parent'] = parent_rows
    R['child_mismatch'] = sorted(k for k in parent_rows if child_rows.get(k) != parent_rows[k])
    R['dirty_after'] = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]


try:
    main()
except Exception:
    R['errors'].append(traceback.format_exc())
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(R, open(OUT, 'w'), indent=1, default=str)
print('LARGETENT_PODEST errors=%d created=%d' % (len(R['errors']), len(R['created'])))
