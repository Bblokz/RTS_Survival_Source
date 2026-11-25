"""Lists the piece components of the five story mission Blueprints (headless check). Log: Saved/WorldMapPieces/check.txt"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import piece_lib as lib  # noqa: E402

for name in ("OreMountains", "Berlin", "OderWarthe", "Pripyat", "FrozenLake"):
    blueprint = unreal.load_asset("%s/%s/BP_Mission_%s" % (lib.STORY_ROOT, name, name))
    lib.log("=== " + name)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    seen = set()
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if template is None or template.get_name() in seen or not isinstance(template, unreal.SceneComponent):
            continue
        seen.add(template.get_name())
        mesh = template.static_mesh if isinstance(template, unreal.StaticMeshComponent) else None
        parent = template.get_attach_parent()
        lib.log("  %s parent=%s loc=(%.1f %.1f %.1f) yaw=%.1f scale=(%.4f %.4f %.4f) mesh=%s mat0=%s" % (
            template.get_name(), parent.get_name() if parent else None,
            template.relative_location.x, template.relative_location.y, template.relative_location.z,
            template.relative_rotation.yaw, template.relative_scale3d.x, template.relative_scale3d.y,
            template.relative_scale3d.z, mesh.get_name() if mesh else None,
            template.get_material(0).get_name() if mesh and template.get_material(0) else None))
lib.write_log("check.txt")

lib.LINES.clear()
for asset_path in sorted(unreal.EditorAssetLibrary.list_assets(lib.STORY_ROOT, recursive=True, include_folder=False)):
    texture = unreal.load_asset(asset_path.split(".")[0])
    if isinstance(texture, unreal.Texture2D):
        lib.log("%s srgb=%s compression=%s flip_green=%s size=%dx%d" % (
            texture.get_name(), texture.srgb, str(texture.compression_settings).split(".")[-1].split(":")[0],
            texture.flip_green_channel, texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))
lib.write_log("check_textures.txt")
