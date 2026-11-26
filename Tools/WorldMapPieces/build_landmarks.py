"""Imports the tall mission landmarks and stands each beside its mission piece (headless, -nullrhi).

A landmark is about 300 to 400 units tall, so its top stands above the world map fog (top at about 160) and shows
where a scripted mission is while the mission itself is still hidden. Models and picks:
Saved/WorldMapUIDesigns/MissionLandmarks/<Folder>/ (pick.md, model/HANDOVER.md).

What this does per mission:
  - imports model/SM_WorldLandmark_<Name>.glb with its PBR textures into
    /Game/RTS_Survival/WorldCampaign/MapObjects/Missions/Landmarks/<Name>;
  - adds one mesh component 'LandmarkMesh' to the mission's Blueprint, with the component tag WorldLandmark.
    AWorldMissionObject keeps a component with that tag visible while the rest of the piece is hidden by fog.
The component name does not start with 'Piece', so build_pieces.py leaves it alone when it rebuilds a piece.
Missions come from the environment variable WORLDPIECES (comma separated, default: all with a model on disk).
Log: Saved/WorldMapPieces/build_landmarks.txt
"""
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import piece_lib as lib  # noqa: E402
import concrete_lib  # noqa: E402
os.environ["WORLDPIECES_IMPORT_ONLY"] = "1"
import build_pieces  # noqa: E402  (helpers only: the line above stops its own build)

ROOT = "/Game/RTS_Survival/WorldCampaign/MapObjects/Missions/Landmarks"
MISSIONS = "/Game/RTS_Survival/WorldCampaign/MapObjects/Missions/"
DESIGNS = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                       "WorldMapUIDesigns", "MissionLandmarks")
LANDMARK_TAG = "WorldLandmark"
COMPONENT_NAME = "LandmarkMesh"
# The map camera looks along +X: +X is behind the piece, +Y is to its right.
DEFAULT_OFFSET = (110.0, 170.0)
# Offsets are from each model/HANDOVER.md (given there as right, back).
# name: (design folder, short code of the slot and texture names, mission Blueprint or None, (behind, right) cm)
LANDMARKS = {
    "OreMountains": ("OreMountains", "Ore", MISSIONS + "Story/OreMountains/BP_Mission_OreMountains", (60.0, 195.0)),
    "Berlin": ("Berlin", "Berlin", MISSIONS + "Story/Berlin/BP_Mission_Berlin", (100.0, 150.0)),
    "OderWarthe": ("OderWarthe", "OW", MISSIONS + "Story/OderWarthe/BP_Mission_OderWarthe", (46.0, 178.0)),
    "Pripyat": ("Pripyat", "Pripyat", MISSIONS + "Story/Pripyat/BP_Mission_Pripyat", (48.0, 212.0)),
    "FrozenLake": ("LakeVygozero", "FL", MISSIONS + "Story/FrozenLake/BP_Mission_FrozenLake", (96.0, 172.0)),
    "Moscow": ("Moscow", "Moscow", MISSIONS + "E100Maus/BP_Mission_E100Maus", (60.0, 150.0)),
    "SvarogLine": ("SvarogLine", "Svarog", MISSIONS + "KingTigerBreakWall/BP_MissionKIngTiger", (70.0, 215.0)),
    # No Blueprint of its own yet: the landmark is its piece (BP_Mission_FreezeEngine is made here).
    "FreezeEngine": ("FreezeEngine", "Freeze", None, (0.0, 0.0)),
}
FREEZE_BLUEPRINT = ("/Game/RTS_Survival/WorldCampaign/MapObjects/Missions/Story/FreezeEngine", "BP_Mission_FreezeEngine")


def ensure_emissive_master():
    """M_WorldPiece_PBREmissive: the PBR master plus an emissive texture times EmissiveStrength."""
    material, is_new = lib._new_material("M_WorldPiece_PBREmissive")
    if not is_new:
        return material
    library = unreal.MaterialEditingLibrary
    sampler = unreal.MaterialSamplerType
    lib._tinted_base_color(material, unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
    normal = lib._texture(material, "Normal", sampler.SAMPLERTYPE_NORMAL,
                          unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"), 420)
    library.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    masks = lib._texture(material, "ORM", sampler.SAMPLERTYPE_MASKS,
                         unreal.load_asset(lib.SHARED_FOLDER + "/T_WorldPiece_DefaultORM"), 700)
    library.connect_material_property(masks, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    library.connect_material_property(masks, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(masks, "B", unreal.MaterialProperty.MP_METALLIC)
    glow = lib._texture(material, "Emissive", sampler.SAMPLERTYPE_COLOR,
                        unreal.load_asset("/Engine/EngineResources/Black"), 980)
    strength = lib._scalar(material, "EmissiveStrength", 8.0, 1240)
    emissive = library.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, 1050)
    library.connect_material_expressions(glow, "RGB", emissive, "A")
    library.connect_material_expressions(strength, "", emissive, "B")
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.recompile_material(material)
    lib.save(material)
    return material


def landmark_materials(masters, glb_path, texture_dir, folder, code):
    prefix, texture_prefix = "M_WL_%s_" % code, "T_WL_%s_" % code
    slot_materials, keep_paths = build_pieces.glb_slot_materials(
        masters, glb_path, texture_dir, folder, prefix, texture_prefix)
    concrete = None
    for slot_name in list(slot_materials):
        short = slot_name[len(prefix):] if slot_name.startswith(prefix) else slot_name
        emissive_path = os.path.join(texture_dir, texture_prefix + short + "_E.png")
        if "Concrete" in short:
            if concrete is None:
                concrete = concrete_lib.concrete_instance(
                    masters["concrete"], folder, "MI_WL_%s_CastConcrete" % code, **build_pieces.CONCRETE_LOOK)
                keep_paths.add(folder + "/MI_WL_%s_CastConcrete" % code)
            slot_materials[slot_name] = concrete
        elif os.path.exists(emissive_path):
            textures = build_pieces.texture_set(texture_dir, folder, texture_prefix + short, flip_green=True)
            textures["Emissive"] = lib.import_texture(emissive_path, folder, texture_prefix + short + "_E", "color")
            instance_name = "MI_" + slot_name[2:] + "_Glow"
            slot_materials[slot_name] = lib.make_instance(folder, instance_name, masters["emissive"],
                                                          textures=textures)
            keep_paths.add(folder + "/" + instance_name)
            keep_paths.add(folder + "/" + texture_prefix + short + "_E")
    return slot_materials, keep_paths


def set_landmark_component(blueprint, mesh, offset):
    """Adds or replaces the LandmarkMesh component under the Blueprint's scene root."""
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    root_handle = handles[0]
    scene_root_handle = None
    for handle in handles:
        template = lib._subobject_object(subsystem, handle)
        if template is None:
            continue
        if template.get_name().startswith(COMPONENT_NAME):
            subsystem.delete_subobject(root_handle, handle, blueprint)
        elif template.get_name().startswith("DefaultSceneRoot"):
            scene_root_handle = handle
    parameters = unreal.AddNewSubobjectParams(
        parent_handle=scene_root_handle or root_handle, new_class=unreal.StaticMeshComponent,
        blueprint_context=blueprint)
    handle, fail_reason = subsystem.add_new_subobject(parameters)
    if not fail_reason.is_empty():
        lib.log("ADD COMPONENT FAILED: %s" % fail_reason)
        return False
    subsystem.rename_subobject(handle, unreal.Text(COMPONENT_NAME))
    component = lib._subobject_object(subsystem, handle)
    component.set_editor_property("relative_location", unreal.Vector(offset[0], offset[1], 0.0))
    # The models have their front on +Y after import; the map camera looks along +X.
    component.set_editor_property("relative_rotation", unreal.Rotator(pitch=0.0, yaw=lib.FRONT_TO_CAMERA_YAW,
                                                                      roll=0.0))
    component.set_static_mesh(mesh)
    # No collision: an unseen mission must not be found or clicked through its landmark.
    component.set_collision_profile_name("NoCollision")
    component.set_editor_property("component_tags", [LANDMARK_TAG])
    component.set_editor_property("lighting_channels", unreal.LightingChannels(
        channel0=True, channel1=True, channel2=False))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    lib.save(blueprint)
    return True


def build_landmark(masters, name):
    design_folder, code, blueprint_path, offset = LANDMARKS[name]
    model_dir = os.path.join(DESIGNS, design_folder, "model")
    mesh_name = "SM_WorldLandmark_" + ("FrozenLake" if name == "FrozenLake" else name)
    glb_path = os.path.join(model_dir, mesh_name + ".glb")
    if not os.path.exists(glb_path):
        lib.log("NO MODEL " + glb_path)
        return
    folder = ROOT + "/" + name
    slot_materials, keep_paths = landmark_materials(masters, glb_path, os.path.join(model_dir, "textures"), folder,
                                                    code)
    mesh, created = lib.import_mesh(glb_path, folder, mesh_name)
    if mesh is None:
        lib.log("IMPORT FAILED " + glb_path)
        return
    lib.finish_mesh(mesh, slot_materials, created, keep_paths)
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(mesh)
    lib.save(mesh)

    # The game stands the landmark beside its mission itself (UWorldFogHintSettings::MissionLandmarks,
    # AWorldMissionObject::CreateLandmark), so no Blueprint is touched unless WORLDPIECES_LANDMARK_BLUEPRINTS=1.
    if os.environ.get("WORLDPIECES_LANDMARK_BLUEPRINTS", "") != "1":
        lib.log("imported %s (no Blueprint step)" % mesh_name)
        return
    if blueprint_path is None:
        blueprint_folder, blueprint_name = FREEZE_BLUEPRINT
        path = blueprint_folder + "/" + blueprint_name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            blueprint = unreal.load_asset(path)
        else:
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", unreal.load_object(None, lib.MISSION_PARENT_CLASS_PATH))
            blueprint = lib.asset_tools.create_asset(blueprint_name, blueprint_folder, unreal.Blueprint, factory)
    else:
        blueprint = unreal.load_asset(blueprint_path)
    if blueprint is None:
        lib.log("NO BLUEPRINT " + str(blueprint_path))
        return
    if set_landmark_component(blueprint, mesh, offset):
        lib.log("landmark %s on %s at behind %.0f right %.0f" % (mesh_name, blueprint.get_name(), offset[0],
                                                                 offset[1]))


def main():
    wanted = [name.strip() for name in os.environ.get("WORLDPIECES", "").split(",") if name.strip()] or list(LANDMARKS)
    masters = lib.ensure_master_materials(lib.OUT_DIR)
    masters["concrete"] = concrete_lib.ensure_concrete_material()
    masters["emissive"] = ensure_emissive_master()
    for name in wanted:
        lib.log("=== " + name)
        try:
            build_landmark(masters, name)
        except Exception:
            lib.log(traceback.format_exc())


lib.LINES.clear()
try:
    main()
except Exception:
    lib.log(traceback.format_exc())
lib.write_log("build_landmarks.txt")
