"""Which renderer class the world controller spawns, the plane's UV layout and usable ground textures.

Log: Saved/WorldMapPieces/road2.txt
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import piece_lib as lib  # noqa: E402

registry = unreal.AssetRegistryHelpers.get_asset_registry()
for data in registry.get_assets_by_path("/Game/RTS_Survival/WorldCampaign", recursive=True):
    if str(data.asset_class_path.asset_name) != "Blueprint":
        continue
    blueprint = data.get_asset()
    generated = blueprint.generated_class() if blueprint else None
    if generated is None:
        continue
    defaults = unreal.get_default_object(generated)
    try:
        value = defaults.get_editor_property("m_connection_spline_renderer_class")
    except Exception:
        continue
    lib.log("%s -> ConnectionSplineRendererClass %s" % (data.package_name, value))

mesh = unreal.load_asset("/Engine/BasicShapes/Plane")
description = mesh.get_static_mesh_description(0)
for index in range(description.get_vertex_instance_count()):
    instance_id = unreal.VertexInstanceID(index)
    position = description.get_vertex_position(description.get_vertex_instance_vertex(instance_id))
    uv = description.get_vertex_instance_uv(instance_id, 0)
    lib.log("plane corner x %.0f y %.0f -> u %.2f v %.2f" % (position.x, position.y, uv.x, uv.y))

for word in ("Asphalt", "Dirt", "Mud", "Gravel", "Road"):
    found = 0
    for data in registry.get_assets_by_path("/Game", recursive=True):
        if str(data.asset_class_path.asset_name) != "Texture2D" or word.lower() not in str(data.asset_name).lower():
            continue
        lib.log("%s: %s" % (word, data.package_name))
        found += 1
        if found >= 14:
            break
lib.write_log("road2.txt")
