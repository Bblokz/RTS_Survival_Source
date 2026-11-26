"""Writes bounds, sockets and vertices of the static meshes named in MESHES to Saved/FieldKits/mesh_vertices.json.
Throwaway headless editor (see dump_kit_hosts.py); nothing is saved."""
import json

import unreal

MESHES = ["SM_Collision_PzIVG_DT_FrontTurret"]


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    result = {}
    for data in registry.get_assets_by_path("/Game/RTS_Survival", recursive=True):
        if str(data.asset_name) not in MESHES:
            continue
        mesh = unreal.load_asset(str(data.package_name))
        description = mesh.get_static_mesh_description(0)
        box = mesh.get_bounding_box()
        result[str(data.package_name)] = {
            "bounds": [[box.min.x, box.min.y, box.min.z], [box.max.x, box.max.y, box.max.z]],
            "sockets": [str(s.get_editor_property("socket_name")) for s in mesh.get_sockets_by_tag("")],
            "vertices": [[round(v.x, 2), round(v.y, 2), round(v.z, 2)] for v in
                         (description.get_vertex_position(unreal.VertexID(i))
                          for i in range(description.get_vertex_count()))]}
    with open("D:/UE5Projects/RTS_Survival/Saved/FieldKits/mesh_vertices.json", "w") as handle:
        json.dump(result, handle)
    unreal.log(f"KIT_MESH_DUMP done {list(result)}")


try:
    main()
finally:
    unreal.SystemLibrary.quit_editor()
