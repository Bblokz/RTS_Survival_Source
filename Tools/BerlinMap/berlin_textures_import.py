"""Import the Ger_Berlin landscape ground textures into Unreal (run inside the editor's Python).

Source PNGs: <project>/Saved/Berlin/textures/T_Berlin_<Layer>_{D,N}.png (made by berlin_textures_gen.py)
Destination: /Game/RTS_Survival/Materials/ground/Landscape/Berlin/Textures/

  _D: sRGB on, default compression (keeps the roughness alpha), LOD group World
  _N: TC_NORMALMAP, sRGB off, LOD group WorldNormalMap (DirectX green, no flip)

Usage in the editor Python console:
  import importlib, sys; sys.path.append(r"<project>/Source/Tools/BerlinMap")
  import berlin_textures_import as bti; importlib.reload(bti); bti.run()
"""
import os

import unreal

LAYERS = ["BerlinRubble", "BerlinAsh", "BerlinCobble", "BerlinGlass", "BerlinScrub", "BerlinMud"]
DEST = "/Game/RTS_Survival/Materials/ground/Landscape/Berlin/Textures"


def _src_dir():
    proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    return os.path.join(proj, "Saved", "Berlin", "textures")


def _log(msg):
    unreal.log("[BerlinTextures] " + msg)


def _import(files):
    tasks = []
    for path, name in files:
        t = unreal.AssetImportTask()
        t.filename = path
        t.destination_path = DEST
        t.destination_name = name
        t.automated = True
        t.replace_existing = True
        t.save = True
        tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)


def _configure(name, is_normal):
    asset_path = DEST + "/" + name
    tex = unreal.EditorAssetLibrary.load_asset(asset_path)
    if tex is None:
        _log("FAILED to load " + asset_path)
        return False
    if is_normal:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    else:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    ok = unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False)
    _log("%s %s (%s)" % ("saved" if ok else "SAVE FAILED", asset_path, "normal" if is_normal else "albedo+rough"))
    return ok


def run():
    src = _src_dir()
    files, kinds = [], []
    for layer in LAYERS:
        for suffix, is_normal in (("D", False), ("N", True)):
            name = "T_Berlin_%s_%s" % (layer, suffix)
            path = os.path.join(src, name + ".png")
            if not os.path.isfile(path):
                _log("missing source, skipped: " + path)
                continue
            files.append((path, name))
            kinds.append((name, is_normal))
    if not files:
        _log("nothing to import from " + src)
        return 0
    _import(files)
    done = 0
    for name, is_normal in kinds:
        try:
            if _configure(name, is_normal):
                done += 1
        except Exception as exc:  # keep going so one bad asset does not stop the other eleven
            _log("ERROR configuring %s: %s" % (name, exc))
    _log("%d / %d textures imported and configured into %s" % (done, len(kinds), DEST))
    return done


if __name__ == "__main__":
    run()
