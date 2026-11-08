"""Adds one reduced LOD to skeletal meshes through a shared LOD settings asset (headless).

Usage: skel_lod.py apply <pct> <screen size> <mesh> [...]   e.g. apply 0.12 0.15 /Game/.../SK_Ger_HeavilyArmored
       skel_lod.py applyas <settings asset name> <pct> <screen size> <mesh> [...]   (SKLOD_RTS_Vehicle: 0.3 0.5)
       skel_lod.py info <mesh> [...]
       skel_lod.py remove <mesh> [...]                       back to LOD0 only (LOD settings cleared)
LOD0 is never changed (generate_base_lod=False), so ragdoll, corpse baking and close-ups keep full detail.
The settings asset is /Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/SKLOD_RTS_Infantry.
"""
import sys
import unreal

TAG = "RTS_SKEL_LOD"
SETTINGS_PATH = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons"
SETTINGS_NAME = "SKLOD_RTS_Infantry"
SUB = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)


def log(msg):
    unreal.log_warning("%s %s" % (TAG, msg))


def lod_group(screen_size, pct):
    group = unreal.SkeletalMeshLODGroupSettings()
    ss = unreal.PerPlatformFloat()
    ss.import_text("(Default=%f)" % screen_size)
    group.set_editor_property("screen_size", ss)
    rs = group.get_editor_property("reduction_settings")
    rs.set_editor_property("termination_criterion", unreal.SkeletalMeshTerminationCriterion.SMTC_NUM_OF_TRIANGLES)
    rs.set_editor_property("num_of_triangles_percentage", pct)
    rs.set_editor_property("num_of_vert_percentage", 1.0)
    group.set_editor_property("reduction_settings", rs)
    return group


def get_settings(pct, screen_size, name=SETTINGS_NAME):
    full = SETTINGS_PATH + "/" + name
    asset = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else None
    if asset is None:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, SETTINGS_PATH,
                                                                         unreal.SkeletalMeshLODSettings, None)
    asset.set_editor_property("lod_groups", [lod_group(1.0, 1.0), lod_group(screen_size, pct)])
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    return asset


def info(mesh):
    n = SUB.get_lod_count(mesh)
    infos = mesh.get_editor_property("lod_info")
    parts = []
    for i in range(n):
        ss = infos[i].get_editor_property("screen_size").default if i < len(infos) else -1
        parts.append("L%d verts=%d ss=%.3f" % (i, SUB.get_num_verts(mesh, i), ss))
    log("info %s %s" % (mesh.get_name(), " | ".join(parts)))


def main(argv):
    action = argv[0]
    if action in ("apply", "applyas"):
        if action == "applyas":
            name, argv = argv[1], argv[1:]
        else:
            name = SETTINGS_NAME
        pct, screen_size = float(argv[1]), float(argv[2])
        settings = get_settings(pct, screen_size, name)
        for path in argv[3:]:
            mesh = unreal.load_asset(path)
            mesh.set_editor_property("lod_settings", settings)
            ok = SUB.regenerate_lod(mesh, 2, False, False)
            saved = unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
            log("apply %s regenerated=%s saved=%s" % (mesh.get_name(), ok, saved))
            info(mesh)
    elif action == "setss":
        # LOD1 screen size set directly on the mesh LOD info (regenerating does not resync it from a new settings asset)
        screen_size = float(argv[1])
        for path in argv[2:]:
            mesh = unreal.load_asset(path)
            infos = mesh.get_editor_property("lod_info")
            ss = unreal.PerPlatformFloat()
            ss.import_text("(Default=%f)" % screen_size)
            infos[1].set_editor_property("screen_size", ss)
            mesh.set_editor_property("lod_info", infos)
            saved = unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
            log("setss %s saved=%s" % (mesh.get_name(), saved))
            info(mesh)
    elif action == "remove":
        for path in argv[1:]:
            mesh = unreal.load_asset(path)
            mesh.set_editor_property("lod_settings", None)
            ok = SUB.regenerate_lod(mesh, 1, True, False)
            saved = unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
            log("remove %s ok=%s saved=%s" % (mesh.get_name(), ok, saved))
            info(mesh)
    else:
        for path in argv[1:]:
            info(unreal.load_asset(path))
    log("DONE")


main(sys.argv[1:])
