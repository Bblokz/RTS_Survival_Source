"""Entry point of the showcase map build. Run inside a DebugGame editor (the map holds ARTSShowcaseCamera actors):

    import sys; sys.path.append("D:/UE5Projects/RTS_Survival/Source/Tools/Showcase")
    import sc_build; sc_build.run(["StrikeDivision"])      # rebuild some stages
    sc_build.run()                                          # rebuild every stage

The map is a copy of RTS_ToCopySmall (game mode, managers, enemy controller), gitignored. Each stage lives far from
the others with its own landscape, lighting set and cameras; see Source/Docs/Showcase/Showcase.md.
"""
import importlib

import unreal

import sc_common
import sc_stages

MAP_PATH = "/Game/RTS_Survival/Maps/Showcase/RTS_Showcase"
TEMPLATE_PATH = "/Game/RTS_Survival/Maps/ToCopy/RTS_ToCopySmall"
# Template actors that would show up in every stage or run a game-play scenario.
TEMPLATE_REMOVE_CLASSES = ("Rus_Squad_HazmatEngineers_C", "Resource_Radixite_Crystal_NoDecal_C")
TEMPLATE_LIGHT_CLASSES = ("DirectionalLight", "SkyLight", "ExponentialHeightFog", "PostProcessVolume")


def ensure_map():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        # Duplicating the map in the editor and then loading the copy crashed (old world not garbage collected):
        # copy Content/RTS_Survival/Maps/ToCopy/RTS_ToCopySmall.umap to Maps/Showcase/RTS_Showcase.umap before launch.
        raise RuntimeError("copy %s.umap to %s.umap on disk first" % (TEMPLATE_PATH, MAP_PATH))
    current = sc_common.editor_world().get_path_name()
    if not current.startswith(MAP_PATH):
        level_editor.load_level(MAP_PATH)
    prepare_template_actors()


def prepare_template_actors():
    actors = sc_common.actor_subsystem()
    for actor in actors.get_all_level_actors():
        class_name = actor.get_class().get_name()
        if class_name in TEMPLATE_REMOVE_CLASSES:
            actors.destroy_actor(actor)
            continue
        if class_name in TEMPLATE_LIGHT_CLASSES and not actor.get_actor_label().startswith(sc_common.PREFIX):
            # The template's own sun and sky belong to "stage" Template, switched off for every showcase stage.
            tag = unreal.Name(sc_common.LIGHT_TAG_PREFIX + "Template")
            if tag not in list(actor.tags):
                actor.tags = list(actor.tags) + [tag]


UNITS_PATH = MAP_PATH.rsplit("/", 1)[0] + "/Units"
# Aircraft with no *_Airborne Blueprint: gitignored child copies that start in the air (the flag is defaults-only).
AIRBORNE_COPIES = {
    "Showcase_Rus_PE2_Airborne": "/Game/RTS_Survival/Blueprints/Aircraft/RusAircraft/PE2/Rus_PE2_Aircraft_3x250",
    "Showcase_Rus_Sturmovic_Airborne": "/Game/RTS_Survival/Blueprints/Aircraft/RusAircraft/Sturmovic/Rus_Aircraft_Sturmovic",
    "Showcase_Rus_PE8_Airborne": "/Game/RTS_Survival/Blueprints/Aircraft/RusAircraft/PE8Bomber/Rus_PE8_Aircraft_8x500",
}


def ensure_airborne_copies():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for name, parent_path in AIRBORNE_COPIES.items():
        path = "%s/%s" % (UNITS_PATH, name)
        blueprint = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if blueprint is None:
            parent = unreal.EditorAssetLibrary.load_blueprint_class(parent_path)
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", parent)
            blueprint = tools.create_asset(name, UNITS_PATH, unreal.Blueprint, factory)
        defaults = unreal.get_default_object(blueprint.generated_class())
        settings = defaults.get_editor_property("m_aircraft_movement_settings")
        settings.set_editor_property("m_started_as_landed", False)
        defaults.set_editor_property("m_aircraft_movement_settings", settings)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)


CLEAN_SNOW_MI = "MI_Showcase_Landscape_CleanSnow"


def ensure_clean_snow_material():
    """Brushify's snow scatters dark rocks and pebbles that read as leopard-spot mud in close hero shots."""
    path = "%s/%s" % (MAP_PATH.rsplit("/", 1)[0], CLEAN_SNOW_MI)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return
    factory = unreal.MaterialInstanceConstantFactoryNew()
    instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(CLEAN_SNOW_MI, MAP_PATH.rsplit("/", 1)[0],
                                                                       unreal.MaterialInstanceConstant, factory)
    instance.set_editor_property("parent", unreal.load_asset("/Game/Brushify/Materials/Landscape/MI_Landscape"))
    library = unreal.MaterialEditingLibrary
    library.set_material_instance_static_switch_parameter_value(instance, "Enable SnowRocks", False)
    library.set_material_instance_static_switch_parameter_value(instance, "Enable Snow Pebbles", False)
    library.set_material_instance_static_switch_parameter_value(instance, "ReduceTiling", True)
    library.update_material_instance(instance)
    unreal.EditorAssetLibrary.save_loaded_asset(instance, False)


def body_instance_source():
    for actor in sc_common.actor_subsystem().get_all_level_actors():
        if actor.get_class().get_name() == "Landscape" and not actor.get_actor_label().startswith(sc_common.PREFIX):
            return actor.get_editor_property("body_instance")
    return None


def run(stage_names=None, save=True):
    importlib.reload(sc_common)
    importlib.reload(sc_stages)
    ensure_map()
    ensure_airborne_copies()
    ensure_clean_snow_material()
    body = body_instance_source()
    names = stage_names or list(sc_stages.STAGES.keys())
    for name in names:
        spec = sc_stages.STAGES[name]
        removed = sc_common.remove_stage(name)
        sc_common.ensure_stage_level(name)
        terrain = sc_common.Terrain(spec)
        sc_common.build_landscape(spec, terrain, spec["weights"], body)
        stage = sc_common.Stage(spec, terrain)
        spec["build"](stage)
        sc_common.log("stage %s: removed %d, placed %d actors" % (name, removed, stage.count))
    sc_common.set_current_level(sc_common.PERSISTENT_LEVEL)
    if save:
        saved = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_all_dirty_levels()
        sc_common.log("saved level: %s" % saved)
