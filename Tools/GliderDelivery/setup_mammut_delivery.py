"""Creates BP_GliderTankDelivery_Mammut and the GA_GerE100Drop glider template (DebugGame-Cmd -run=pythonscript).

UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
Pass MAMMUT_MESH_FOLDER in the environment to point the Blueprint at another mesh set (greybox by default).
"""
import os

import unreal

MESH_FOLDER = os.environ.get('MAMMUT_MESH_FOLDER', '/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/Mammut/Greybox')
BP_FOLDER = '/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/Mammut'
BP_NAME = 'BP_GliderTankDelivery_Mammut'
DA_PATH = '/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem'
E100_CLASS = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/E100/Ger_Vehicle_E100.Ger_Vehicle_E100_C'
DUST = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/NS_Track_Smoke'
CABLE_MESH = '/Engine/BasicShapes/Cylinder'
CABLE_MATERIAL = '/Engine/BasicShapes/BasicShapeMaterial'
STRIP_MATERIAL = '/Game/RTS_Survival/Materials/AimMaterials/TargetAreas/M_TargetArea_Recon'
ICON = '/Game/RTS_Survival/Textures/GameUI/GlobalAbilities/T_GlobalAbility_GerMammutE100Drop'
FALLBACK_ICON = '/Game/RTS_Survival/Textures/GameUI/GlobalAbilities/T_GlobalAbility_GerSpotterPzIIIDrop'
# Fixed-length landing run: the strip is the skid, from touchdown to stop.
SKID_LENGTH = 7000.0
STRIP_HALF_WIDTH = 1600.0


def log(msg):
    unreal.log_warning('MAMMUT_SETUP ' + msg)


def load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError('missing asset ' + path)
    return asset


def component_templates(bp):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    found = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        obj = lib.get_object_for_blueprint(data, bp)
        if obj is not None:
            found[obj.get_name()] = obj
    return found


def make_blueprint():
    path = BP_FOLDER + '/' + BP_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        bp = load(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.GliderTankDelivery)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(BP_NAME, BP_FOLDER, unreal.Blueprint, factory)
    templates = component_templates(bp)
    log('templates ' + ', '.join(sorted(templates)))
    meshes = {'GliderBody': 'SM_Mammut_Body', 'GliderNose': 'SM_Mammut_Nose', 'TugBody': 'SM_He111Z_Tug'}
    for comp_name, mesh_name in meshes.items():
        templates[comp_name].set_editor_property('static_mesh', load(MESH_FOLDER + '/' + mesh_name))
    cable = templates['TowCable']
    cable.set_editor_property('static_mesh', load(CABLE_MESH))
    cable.set_material(0, load(CABLE_MATERIAL))
    dust = templates['SkidDust']
    dust.set_editor_property('asset', load(DUST))
    dust.set_editor_property('relative_scale3d', unreal.Vector(3.0, 3.0, 3.0))

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('m_tug_prop_mesh', load(MESH_FOLDER + '/SM_He111Z_Prop'))
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    log('blueprint saved ' + path)
    return bp


def fresh_copy(struct_value):
    copy = type(struct_value)()
    copy.import_text(struct_value.export_text())
    return copy


def find_enum(enum_type, cpp_name):
    wanted = cpp_name.replace('_', '').lower()
    for name in dir(enum_type):
        if name.replace('_', '').lower() == wanted:
            return getattr(enum_type, name)
    raise RuntimeError('no enum value ' + cpp_name)


def add_ability(bp):
    da = load(DA_PATH)
    templates = da.get_editor_property('m_ability_templates_by_type')
    # The EGlobalAbility enum shares its Python name with the UGlobalAbility class; take its type from a key.
    ability_enum = type(next(iter(templates.keys())))
    e100_key = find_enum(ability_enum, 'GA_GerE100Drop')
    strip_source = templates[find_enum(ability_enum, 'GA_GerAirMinefield')]
    drop_source = templates[find_enum(ability_enum, 'GA_GerPzIIIFlak41Drop')]

    ability = unreal.new_object(unreal.GA_GliderTankDrop, outer=da, name='GA_GliderTankDrop_GerE100')
    ability.set_editor_property('m_ability_type', e100_key)
    ability.set_editor_property('m_ability_tier', unreal.GlobalAbilityTier.TIER_IV)

    costs = fresh_copy(drop_source.get_editor_property('m_ability_costs'))
    costs.set_editor_property('cool_down_time', 300)
    ability.set_editor_property('m_ability_costs', costs)

    aim = fresh_copy(strip_source.get_editor_property('m_aim_settings'))
    area = aim.get_editor_property('area_aim')
    area.set_editor_property('area_material', load(STRIP_MATERIAL))
    area.set_editor_property('area_radius', STRIP_HALF_WIDTH)
    area.set_editor_property('min_strip_length', SKID_LENGTH)
    area.set_editor_property('max_strip_length', SKID_LENGTH)
    area.set_editor_property('length_before_anchor', SKID_LENGTH)
    area.set_editor_property('drop_point_mesh', None)
    aim.set_editor_property('area_aim', area)
    ability.set_editor_property('m_aim_settings', aim)

    ability.set_editor_property('m_ability_marker', fresh_copy(drop_source.get_editor_property('m_ability_marker')))
    ability.set_editor_property('m_ability_sound_settings',
                                fresh_copy(drop_source.get_editor_property('m_ability_sound_settings')))

    ui = fresh_copy(drop_source.get_editor_property('m_ui_settings'))
    icon_path = ICON if unreal.EditorAssetLibrary.does_asset_exist(ICON) else FALLBACK_ICON
    ui.set_editor_property('ability_icon', load(icon_path))
    ui.set_editor_property('title', '<Text_Title>Mammut: E-100 Delivery</>')
    ui.set_editor_property('description',
                           'An He 111 Z tows a Mammut glider in. Draw its landing run: it touches down at the first '
                           'click, skids to a stop at the second and an E-100 drives out of its nose.')
    ability.set_editor_property('m_ui_settings', ui)

    settings = unreal.GliderTankDropSettings()
    settings.set_editor_property('delivery_class', bp.generated_class())
    settings.set_editor_property('tank_subtype', find_enum(unreal.TankSubtype, 'Tank_E100'))
    settings.set_editor_property('tank_class_override', unreal.EditorAssetLibrary.load_blueprint_class(E100_CLASS[:E100_CLASS.rfind('.')]))
    ability.set_editor_property('m_glider_drop_settings', settings)

    templates[e100_key] = ability
    da.set_editor_property('m_ability_templates_by_type', templates)
    ok = unreal.EditorAssetLibrary.save_loaded_asset(da, False)
    log('data asset saved=%s entries=%d' % (ok, len(templates)))
    return ok


# A live editor that holds DA_GlobalAbilitySystem can exec this file with MAMMUT_SKIP_MAIN set and call add_ability.
if not globals().get('MAMMUT_SKIP_MAIN'):
    bp = make_blueprint()
    add_ability(bp)
    log('DONE')
