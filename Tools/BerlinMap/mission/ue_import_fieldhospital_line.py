"""Commandlet: import new Berlin announcer lines as SoundWaves (new assets only) with the cinematic sound class.
Launched by run_import_fieldhospital_line.py. Copy of ue_import_berlin_lines.py for the field hospital line."""
import json
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = r"D:\Blender\BlenderProjects\BlenderTanks\VoiceLines\Announcer\AAAGerMissions\Berlin"
DEST = "/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/Berlin"
SCLASS = "/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_CinematicAndTransmissions"
NAMES = ["Berlin_Cinematic_FieldHospital"]

sclass = unreal.load_asset(SCLASS)
rep = {"sound_class": str(sclass.get_path_name()) if sclass else None, "items": []}
tools = unreal.AssetToolsHelpers.get_asset_tools()
for name in NAMES:
    wav = os.path.join(SRC, name + ".wav")
    path = f"{DEST}/{name}"
    item = {"asset": f"{path}.{name}"}
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        item["skipped"] = "exists"
    else:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", wav)
        task.set_editor_property("destination_path", DEST)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        asset = unreal.load_asset(path)
        if asset is None or sclass is None:
            item["error"] = "import failed"
        else:
            asset.set_editor_property("sound_class_object", sclass)
            item["saved"] = bool(unreal.EditorAssetLibrary.save_loaded_asset(asset, False))
    asset = unreal.load_asset(path)
    if asset:
        item.update({"duration": round(asset.get_editor_property("duration"), 3),
                     "sound_class": str(asset.get_editor_property("sound_class_object").get_path_name()),
                     "source": list(asset.get_editor_property("asset_import_data").extract_filenames())})
    rep["items"].append(item)
json.dump(rep, open(os.path.join(HERE, "ue_import_fieldhospital_line_report.json"), "w"), indent=1)
