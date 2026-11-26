"""Exports the kit hosts' variant tank and turret Blueprints as T3D, because their overridden components (turret
class, collision mesh) live in the inheritable component handler, which python's subobject gather does not show.
Also writes the sockets of a few extra meshes. Throwaway headless editor, nothing saved (see dump_kit_hosts.py).
Output: Saved/FieldKits/T3D/Variants/*.t3d and Saved/FieldKits/extra_meshes.json
"""
import json
import os

import unreal

OUT = "D:/UE5Projects/RTS_Survival/Saved/FieldKits"
VEH = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle"
TUR = "/Game/RTS_Survival/Blueprints/Turrets/German"
SEARCH_NAMES = [
    "Ger_Turret_PZIII_M_Overcharged", "Ger_Turret_PZIII_M_SpacedArmor", "Ger_Turret_Pz38tRockets_IncendiarySalvo",
    "Ger_Turret_Pz38tRockets_PrecisionMissile", "Ger_Turret_Jaguar_Flanker", "Ger_Turret_PzIVG_DoubleTurret_Front",
    "Ger_Turret_PanzerIV_H_Ostwind", "Ger_Turret_PanzerIV_H_StandardPlus", "Ger_Turret_PZ38T_ATAmmoStock",
    "Ger_Turret_PZ38T_Rocketeer", "Ger_Turret_PZ38T_Rail_Overcharge", "Ger_Turret_PZ38T_Rail_Sniper",
    "Ger_Turret_PanzerWerfer",
    "Ger_Vehicle_PanzerIII_M_Overcharged", "Ger_Vehicle_PanzerIII_M_SpacedArmor",
    "Ger_Vehicle_Panzer38T_R_IncendiarySalvo", "Ger_Vehicle_Panzer38T_R_PrecisionMissile",
    "Ger_Vehicle_Jaguar_Flanker", "Ger_Vehicle_PanzerIV_G_DoubleTurret", "Ger_Vehicle_PanzerIV_H_Ostwind",
    "Ger_Vehicle_PanzerIV_H_StandardPlus", "Ger_Vehicle_Panzer38T_ATAmmoStock", "Ger_Vehicle_Panzer38T_Rocketeer",
    "Ger_Vehicle_Panzer38_RAIL_Overcharge", "Ger_Vehicle_Panzer38_RAIL_Sniper",
]
EXTRA_MESHES = ["SM_Collision_PzIVF1_Spotter_Turret"]


def find_assets(names):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = {}
    for root in (VEH, TUR):
        for data in registry.get_assets_by_path(root, recursive=True):
            name = str(data.asset_name)
            if name in names:
                found[name] = str(data.package_name)
    return found


def export(path, folder):
    task = unreal.AssetExportTask()
    task.object = unreal.load_asset(path)
    task.filename = f"{folder}/{path.split('/')[-1]}.t3d"
    task.exporter = unreal.ObjectExporterT3D()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    return unreal.Exporter.run_asset_export_task(task)


def main():
    folder = f"{OUT}/T3D/Variants"
    os.makedirs(folder, exist_ok=True)
    found = find_assets(set(SEARCH_NAMES) | set(EXTRA_MESHES))
    result = {"missing": [name for name in SEARCH_NAMES + EXTRA_MESHES if name not in found], "meshes": {},
              "exported": []}
    for name in SEARCH_NAMES:
        if name in found:
            result["exported"].append(f"{name} {export(found[name], folder)}")
    for name in EXTRA_MESHES:
        if name not in found:
            continue
        mesh = unreal.load_asset(found[name])
        box = mesh.get_bounding_box()
        result["meshes"][found[name]] = {
            "bounds": [[box.min.x, box.min.y, box.min.z], [box.max.x, box.max.y, box.max.z]],
            "sockets": [[str(s.get_editor_property("socket_name")),
                         [s.get_editor_property("relative_location").x, s.get_editor_property("relative_location").y,
                          s.get_editor_property("relative_location").z]] for s in mesh.get_sockets_by_tag("")]}
    with open(f"{OUT}/extra_meshes.json", "w") as handle:
        json.dump(result, handle, indent=1)
    unreal.log(f"KIT_VARIANT_DUMP done exported={len(result['exported'])} missing={result['missing']}")


try:
    main()
finally:
    unreal.SystemLibrary.quit_editor()
