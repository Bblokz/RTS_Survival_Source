"""Dumps the structure of the existing world map piece Blueprints and the source meshes for the story mission pieces.

Run headless: UnrealEditor-Win64-DebugGame.exe <uproject> -nullrhi -ExecutePythonScript=<this file>
Output: Saved/WorldMapPieces/dump.txt
"""
import os
import traceback

import unreal

OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "WorldMapPieces")
OUT_PATH = os.path.join(OUT_DIR, "dump.txt")
LINES = []

MAP_OBJECTS = "/Game/RTS_Survival/WorldCampaign/MapObjects/"
BLUEPRINTS = [
    MAP_OBJECTS + "Missions/BP_MissionWorldObject",
    MAP_OBJECTS + "Missions/ME410/BP_Mission_ME410",
    MAP_OBJECTS + "Missions/FirstRoadAndBuild/BP_Mission_FirstRoad",
    MAP_OBJECTS + "Player/BP_WorldPlayerObject",
    MAP_OBJECTS + "Player/HQ/BP_PlayerHQ",
    "/Game/RTS_Survival/Blueprints/Environment/Berlin/ReichstagRuin/BP_ReichstagRuin_B2",
]
MESHES = [
    "/Game/RTS_Survival/Blueprints/Environment/Berlin/FlakTower/SM_BF_FlakTower_Damaged",
    "/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Werke/SM_OW_Werk_Peilturm",
    "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/SM_PzIV_H_TrainingPreview",
]


def log(text):
    LINES.append(str(text))


def describe_mesh(mesh, indent="  "):
    if mesh is None:
        log(indent + "mesh: None")
        return
    bounds = mesh.get_bounds()
    log("%smesh %s origin=%s extent=%s tris=%d nanite=%s" % (
        indent, mesh.get_path_name(), bounds.origin, bounds.box_extent,
        mesh.get_num_triangles(0), mesh.get_editor_property("nanite_settings").enabled))
    for slot_index, static_material in enumerate(mesh.static_materials):
        interface = static_material.material_interface
        log("%s  slot %d %s -> %s" % (indent, slot_index, static_material.material_slot_name,
                                      interface.get_path_name() if interface else None))


def describe_component(component, indent="  "):
    log("%s%s (%s)" % (indent, component.get_name(), component.get_class().get_name()))
    if isinstance(component, unreal.SceneComponent):
        log("%s  loc=%s rot=%s scale=%s parent=%s" % (
            indent, component.relative_location, component.relative_rotation, component.relative_scale3d,
            component.get_attach_parent().get_name() if component.get_attach_parent() else None))
    if isinstance(component, unreal.PrimitiveComponent):
        log("%s  collision=%s enabled=%s customdepth=%s shadow=%s" % (
            indent, component.get_collision_profile_name(), component.get_collision_enabled(),
            component.render_custom_depth, component.cast_shadow))
    if isinstance(component, unreal.StaticMeshComponent):
        describe_mesh(component.static_mesh, indent + "  ")
        for material_index in range(component.get_num_materials()):
            material = component.get_material(material_index)
            log("%s    comp material %d -> %s" % (indent, material_index,
                                                  material.get_path_name() if material else None))
    if isinstance(component, unreal.ChildActorComponent):
        log("%s  child class=%s" % (indent, component.child_actor_class))


def describe_blueprint(path):
    log("")
    log("=== " + path)
    blueprint = unreal.load_asset(path)
    if blueprint is None:
        log("  NOT FOUND")
        return
    generated_class = blueprint.generated_class()
    log("  generated=%s" % generated_class.get_path_name())
    parent = generated_class
    chain = []
    for _ in range(8):
        parent = unreal.SystemLibrary.get_class_display_name(parent) if False else parent
        chain.append(parent.get_name())
        break
    log("  parent chain start=%s" % chain)
    try:
        log("  parent=%s" % blueprint.get_editor_property("parent_class").get_path_name())
    except Exception as error:
        log("  parent read failed: %s" % error)

    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    for handle in handles:
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if template is None or not isinstance(template, unreal.ActorComponent):
            continue
        inherited = unreal.SubobjectDataBlueprintFunctionLibrary.is_inherited_component(data)
        native = unreal.SubobjectDataBlueprintFunctionLibrary.is_native_component(data)
        log("  -- inherited=%s native=%s" % (inherited, native))
        describe_component(template, "  ")

    default_object = unreal.get_default_object(generated_class)
    for property_name in ("m_map_item_ui_data", "m_primary_reward", "m_secondary_reward"):
        try:
            value = default_object.get_editor_property(property_name)
            log("  %s = %s" % (property_name, value.export_text()))
        except Exception as error:
            log("  %s read failed: %s" % (property_name, error))


def main():
    for path in BLUEPRINTS:
        try:
            describe_blueprint(path)
        except Exception:
            log(traceback.format_exc())
    log("")
    log("=== source meshes")
    for path in MESHES:
        try:
            describe_mesh(unreal.load_asset(path))
        except Exception:
            log(traceback.format_exc())
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for folder in ("/Game/RTS_Survival/Blueprints/Environment/Berlin/ReichstagRuin/Mesh",):
        for asset_data in registry.get_assets_by_path(folder, recursive=False):
            try:
                describe_mesh(asset_data.get_asset())
            except Exception:
                log(traceback.format_exc())


try:
    main()
except Exception:
    log(traceback.format_exc())
os.makedirs(OUT_DIR, exist_ok=True)
with open(OUT_PATH, "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
unreal.SystemLibrary.quit_editor()
