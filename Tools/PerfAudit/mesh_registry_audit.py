"""Project-wide static/skeletal mesh triangle audit from asset registry tags (no asset loading).
Writes Saved/PerfAudit/mesh_registry.json: [{path, class, tris, verts, lods, nanite, materials}]."""
import json
import os
import unreal

OUT = unreal.Paths.project_saved_dir() + "PerfAudit/mesh_registry.json"
reg = unreal.AssetRegistryHelpers.get_asset_registry()
rows = []
for cls in ("StaticMesh", "SkeletalMesh"):
    flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/Engine", cls)], package_paths=["/Game"], recursive_paths=True)
    for ad in reg.get_assets(flt):
        def tag(name):
            try:
                v = ad.get_tag_value(name)
                return str(v) if v is not None else None
            except Exception:
                return None
        rows.append({"path": str(ad.package_name), "class": cls, "tris": tag("Triangles"), "verts": tag("Vertices"),
                     "lods": tag("LODs"), "nanite": tag("NaniteEnabled") or tag("HasNaniteData"), "materials": tag("Materials")})
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w") as fh:
    json.dump(rows, fh)
unreal.log_warning("RTS_MESH_AUDIT wrote %d rows to %s" % (len(rows), OUT))
