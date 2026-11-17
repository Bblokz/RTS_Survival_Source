"""Headless, once: recompile/resave BP_MissionManager (the old victory/defeat widget class properties are gone from C++)
and delete the old widget Blueprints BP_W_Victory / BP_W_Defeat."""
import unreal

EAL = unreal.EditorAssetLibrary


def log(*a):
    unreal.log_warning("DEBRIEF_RETIRE " + " ".join(str(x) for x in a))


mm_path = "/Game/RTS_Survival/Blueprints/Missions/BP_MissionManager"
mm = unreal.load_asset(mm_path)
unreal.BlueprintEditorLibrary.compile_blueprint(mm)
log("saved BP_MissionManager", EAL.save_loaded_asset(mm, only_if_is_dirty=False))
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for pkg in ("/Game/RTS_Survival/Blueprints/Missions/MissionUI/BP_W_Victory", "/Game/RTS_Survival/Blueprints/Missions/MissionUI/BP_W_Defeat"):
    refs = [str(r) for r in (ar.get_referencers(pkg, unreal.AssetRegistryDependencyOptions(True, True, True, True, True)) or [])]
    refs = [r for r in refs if r != mm_path]
    if refs:
        log("NOT deleting", pkg, "still referenced by", refs)
        continue
    log("deleted", pkg, EAL.delete_asset(pkg))
log("done")
