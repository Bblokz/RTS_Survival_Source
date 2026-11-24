"""Headless import of the mission debrief stage assets exported from the approved browser mock-up.

Run: UnrealEditor-Cmd RTS_Survival.uproject -run=pythonscript -script=<this> -AllowCommandletRendering
Env DEBRIEF_EXPORT_DIR: folder with the exported .glb / .png / timeline.json (export_scene.html via run_export.py).

Each .glb is imported with Interchange (glTF pipeline, static meshes combined into one mesh per file) into its own
folder under /Game/RTS_Survival/Blueprints/Missions/MissionDebrief/Imported/, then renamed. PNGs become textures.
"""
import json
import os

import unreal

SRC = os.environ.get("DEBRIEF_EXPORT_DIR", "")
ROOT = "/Game/RTS_Survival/Blueprints/Missions/MissionDebrief"
LOG_PATH = os.path.join(SRC, "import_log.txt")
LOG = open(LOG_PATH, "w", encoding="utf-8")


def log(*a):
    LOG.write(" ".join(str(x) for x in a) + "\n")
    LOG.flush()
    unreal.log("DEBRIEF_IMPORT " + " ".join(str(x) for x in a))


EAL = unreal.EditorAssetLibrary
MESHES = {
    "set_common": "SM_Debrief_Set",
    "stamp_body": "SM_Debrief_StampBody",
    "tin_victory": "SM_Debrief_TinPad_Victory",
    "tin_defeat": "SM_Debrief_TinPad_Defeat",
    "face_victory_mirrored": "SM_Debrief_Face_Victory_Mirrored",
    "face_victory_readable": "SM_Debrief_Face_Victory_Readable",
    "face_defeat_mirrored": "SM_Debrief_Face_Defeat_Mirrored",
    "face_defeat_readable": "SM_Debrief_Face_Defeat_Readable",
    "paper": "SM_Debrief_Paper",
    "soot": "SM_Debrief_Soot",
}
# name -> (asset name, sRGB, compression, wrap)
TEXTURES = {
    "ink_victory": ("T_Debrief_Ink_Victory", False, unreal.TextureCompressionSettings.TC_DEFAULT),
    "ink_defeat": ("T_Debrief_Ink_Defeat", False, unreal.TextureCompressionSettings.TC_DEFAULT),
    "inkheight_victory": ("T_Debrief_InkHeight_Victory", False, unreal.TextureCompressionSettings.TC_GRAYSCALE),
    "inkheight_defeat": ("T_Debrief_InkHeight_Defeat", False, unreal.TextureCompressionSettings.TC_GRAYSCALE),
    "keep_defeat": ("T_Debrief_Keep_Defeat", False, unreal.TextureCompressionSettings.TC_GRAYSCALE),
    "noise": ("T_Debrief_Noise", False, unreal.TextureCompressionSettings.TC_GRAYSCALE),
    "soot_alpha": ("T_Debrief_SootAlpha", False, unreal.TextureCompressionSettings.TC_GRAYSCALE),
    "sprite_dot": ("T_Debrief_Sprite_Dot", True, unreal.TextureCompressionSettings.TC_DEFAULT),
    "sprite_flame": ("T_Debrief_Sprite_Flame", True, unreal.TextureCompressionSettings.TC_DEFAULT),
    "sprite_smoke": ("T_Debrief_Sprite_Smoke", True, unreal.TextureCompressionSettings.TC_DEFAULT),
}


def combine_pipeline():
    path = ROOT + "/Imported/P_Debrief_GLTFAssets"
    if EAL.does_asset_exist(path):
        p = unreal.load_asset(path)
    else:
        p = EAL.duplicate_loaded_asset(unreal.load_asset("/Interchange/Pipelines/DefaultGLTFAssetsPipeline"), path)
    mp = p.get_editor_property("mesh_pipeline")
    mp.set_editor_property("combine_static_meshes", True)  # instanced sub-object: editing it edits the pipeline
    EAL.save_loaded_asset(p)
    return p


def import_glb(file_key, asset_name, pipe):
    dest = f"{ROOT}/Imported/{asset_name}"
    if EAL.does_directory_exist(dest):
        EAL.delete_directory(dest)
    src = unreal.InterchangeManager.create_source_data(os.path.join(SRC, file_key + ".glb"))
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines = [unreal.SoftObjectPath(pipe.get_path_name()),
                                 unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline")]
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    res = mgr.import_asset(dest, src, params)
    meshes = [r for r in res if "StaticMesh'" in str(r)]
    log(file_key, "imported", len(res), "assets,", len(meshes), "meshes")
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    sms = [a for a in ar.get_assets_by_path(dest, recursive=True) if str(a.asset_class_path.asset_name) == "StaticMesh"]
    if len(sms) != 1:
        log(file_key, "WARNING expected one combined mesh, got", [str(a.asset_name) for a in sms])
    if not sms:
        return None
    old = str(sms[0].package_name)
    new = f"{dest}/{asset_name}"
    if old != new:
        EAL.rename_asset(old, new)
    m = unreal.load_asset(new)
    b = m.get_bounds()
    log(asset_name, "origin", round(b.origin.x, 2), round(b.origin.y, 2), round(b.origin.z, 2),
        "extent", round(b.box_extent.x, 2), round(b.box_extent.y, 2), round(b.box_extent.z, 2),
        "slots", len(m.static_materials))
    EAL.save_directory(dest, only_if_is_dirty=False, recursive=True)
    return m


def import_png(file_key, asset_name, srgb, comp):
    dest = f"{ROOT}/Textures"
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SRC, file_key + ".png")
    task.destination_path = dest
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    path = f"{dest}/{asset_name}"
    t = unreal.load_asset(path)
    if t is None:
        log(asset_name, "IMPORT FAILED")
        return
    t.set_editor_property("srgb", srgb)
    t.set_editor_property("compression_settings", comp)
    t.set_editor_property("never_stream", True)
    EAL.save_loaded_asset(t)
    log(asset_name, "texture", t.blueprint_get_size_x(), t.blueprint_get_size_y())


def burn_field_asset():
    """The CPU copy of the burn field (for placing flames) goes into a data asset of the stage's own class, if built."""
    data = json.load(open(os.path.join(SRC, "timeline.json")))
    out = {}
    for st in ("victory", "defeat"):
        g = data[st]["burnGrid"]
        out[st] = (g["w"], g["h"], g["v"])
    json.dump({k: {"w": v[0], "h": v[1]} for k, v in out.items()}, open(os.path.join(SRC, "burn_grid_dims.json"), "w"))


pipe = combine_pipeline()
ONLY = [k for k in os.environ.get('DEBRIEF_ONLY', '').split(',') if k]
for key, name in MESHES.items():
    if ONLY and key not in ONLY:
        continue
    try:
        import_glb(key, name, pipe)
    except Exception as e:  # noqa
        log(key, "ERROR", e)
for key, (name, srgb, comp) in TEXTURES.items():
    if ONLY and key not in ONLY:
        continue
    try:
        import_png(key, name, srgb, comp)
    except Exception as e:  # noqa
        log(key, "ERROR", e)
burn_field_asset()
log("done")
LOG.close()
