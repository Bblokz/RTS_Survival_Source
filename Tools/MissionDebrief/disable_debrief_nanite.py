"""Headless: the glTF import made the debrief stage meshes Nanite; the stage needs regular meshes (lighting channel 1,
translucent contact shadows, imported glTF materials without the Nanite usage flag)."""
import unreal

ROOT = "/Game/RTS_Survival/Blueprints/Missions/MissionDebrief/Imported"
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for a in ar.get_assets_by_path(ROOT, recursive=True):
    if str(a.asset_class_path.asset_name) != "StaticMesh":
        continue
    mesh = unreal.load_asset(str(a.package_name))
    settings = mesh.get_editor_property("nanite_settings")
    if settings.get_editor_property("enabled"):
        settings.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", settings)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        unreal.log_warning("DEBRIEF_NANITE disabled on " + str(a.asset_name))
unreal.log_warning("DEBRIEF_NANITE done")
