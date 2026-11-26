"""Read-only: what the world map wall is made of now, and what the cinematic Ural Shield kit offers."""
import os
import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldMapWall")
LINES = []


def log(text):
    LINES.append(str(text))
    unreal.log("URALWALL " + str(text))


def mesh_line(path):
    mesh = unreal.load_asset(path)
    if mesh is None:
        log("MISSING " + path)
        return
    box = mesh.get_bounding_box()
    mats = [str(m.material_interface.get_path_name()) if m.material_interface else "None" for m in mesh.static_materials]
    log("MESH %s tris=%d lods=%d nanite=%s box (%.0f %.0f %.0f)-(%.0f %.0f %.0f) mats=%s" % (
        path, mesh.get_num_triangles(0), mesh.get_num_lods(), mesh.get_editor_property("nanite_settings").enabled,
        box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z, mats))


try:
    kit = "/Game/RTS_Survival/Cinematics/Intro1976/UralShield/"
    for name in ("SM_UralShield_Segment", "SM_UralShield_Far", "SM_UralShield_GateFrame", "SM_UralShield_GateLeaf_L",
                 "SM_UralShield_GateLeaf_R"):
        mesh_line(kit + name)
    old = "/Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/Wall/"
    mesh_line(old + "WallOnly/SM_Global_SovietWall")
    mesh_line(old + "NewWallgate/SM_Global_SovietWallWithGate2")
    mesh_line("/Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/Prototypes/Wall/SM_WorldPiece_EnemyWall")
    for path in unreal.EditorAssetLibrary.list_assets(old, recursive=True, include_folder=False):
        log("ASSET " + path)
    material = unreal.load_asset("/Game/RTS_Survival/Cinematics/Intro1976/Materials/MI_UralShield")
    log("MI_UralShield parent %s" % material.get_editor_property("parent").get_path_name())

    unreal.EditorLoadingAndSavingUtils.load_map("/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign")
    world = unreal.EditorLevelLibrary.get_editor_world()
    count = 0
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh = component.get_editor_property("static_mesh")
            if mesh is None or "Wall" not in mesh.get_name():
                continue
            t = component.get_world_transform()
            count += 1
            log("USE %s | %s (%s) folder=%s | %s | loc (%.1f %.1f %.1f) yaw %.1f scale (%.3f %.3f %.3f) mats=%s" % (
                actor.get_actor_label(), actor.get_class().get_name(), component.get_class().get_name(),
                actor.get_folder_path(), mesh.get_name(), t.translation.x, t.translation.y, t.translation.z,
                t.rotation.rotator().yaw, t.scale3d.x, t.scale3d.y, t.scale3d.z,
                [m.get_name() if m else "None" for m in component.get_materials()]))
    log("wall components %d" % count)
except Exception:
    import traceback
    log(traceback.format_exc())
os.makedirs(OUT, exist_ok=True)
open(os.path.join(OUT, "dump.txt"), "w", encoding="utf-8").write("\n".join(LINES))
