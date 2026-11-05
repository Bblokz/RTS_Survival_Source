"""Sets AMissionManager::MaxPlayerGlobalAbilityTier on the two campaign missions the user capped (2026-11-05):
  Ger_FirstMission       (first German mission)        Tier I only
  Ger_MarderStugMission  (the StuG / Marder mission)    up to Tier II
Every other mission keeps the default (Tier IV). Loads each map, sets the property on its mission manager instance
(BP_MissionManager), saves the map and checks the file kept its size (a navmesh rebuild during the save would shrink
it). Idempotent: a map that already has the cap is not saved. Lines prefixed [Round5Caps].
Run headless on the DebugGame build (hold the build lock); nothing else may have the maps open:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
"""
import os
import shutil
import time

import unreal

MAPS = [
    ("/Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission", "TierI"),
    ("/Game/RTS_Survival/Maps/GerMissions/Ger_MarderStugMission", "TierII"),
]
LOG = "[Round5Caps] "
FAILURES = []
# A navmesh dropped during the save shows up as a much smaller file; the map is then restored from the backup.
MIN_SIZE_RATIO = 0.97
BACKUP_DIR = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                          "Round5FixesTest", "MapBackups")


def log(text):
    print(LOG + text)


def tier(name):
    wanted = name.replace("_", "").lower()
    for attr in dir(unreal.GlobalAbilityTier):
        if attr.replace("_", "").lower() == wanted:
            return getattr(unreal.GlobalAbilityTier, attr)
    raise RuntimeError("no GlobalAbilityTier " + name)


def disk_path(map_path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, map_path[len("/Game/"):] + ".umap")


def mission_managers(world):
    managers = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MissionManager):
        managers.append(actor)
    return managers


def set_cap(map_path, tier_name):
    file_path = disk_path(map_path)
    size_before = os.path.getsize(file_path)
    mtime_before = os.path.getmtime(file_path)
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if world is None:
        FAILURES.append("load " + map_path)
        log("LOAD FAILED " + map_path)
        return
    managers = mission_managers(world)
    log("%s: %d mission manager(s), file %d bytes" % (map_path, len(managers), size_before))
    if len(managers) != 1:
        FAILURES.append("%s has %d mission managers" % (map_path, len(managers)))
        return
    manager = managers[0]
    wanted = tier(tier_name)
    current = manager.get_editor_property("max_player_global_ability_tier")
    log("  %s %s: cap %s -> %s" % (manager.get_name(), manager.get_class().get_name(), current, wanted))
    if current == wanted:
        log("  already set, not saving")
        return
    os.makedirs(BACKUP_DIR, exist_ok=True)
    backup = os.path.join(BACKUP_DIR, os.path.basename(file_path) + time.strftime(".%H%M%S.bak"))
    shutil.copy2(file_path, backup)
    log("  backup %s" % backup)
    manager.modify()
    manager.set_editor_property("max_player_global_ability_tier", wanted)
    saved = unreal.EditorLoadingAndSavingUtils.save_map(world, map_path)
    size_after = os.path.getsize(file_path)
    reached_disk = os.path.getmtime(file_path) > mtime_before
    log("  save=%s reached_disk=%s size %d -> %d (%.3f)" % (saved, reached_disk, size_before, size_after,
                                                             size_after / float(size_before)))
    if not saved or not reached_disk:
        FAILURES.append("save " + map_path)
    elif size_after < size_before * MIN_SIZE_RATIO:
        FAILURES.append("%s shrank on save (%d -> %d): restored the backup" % (map_path, size_before, size_after))
        shutil.copy2(backup, file_path)
        return
    with open(file_path, "rb") as handle:
        has_property = b"MaxPlayerGlobalAbilityTier" in handle.read()
    log("  property tag on disk: %s" % has_property)
    if not has_property:
        FAILURES.append("no property tag in " + map_path)


for path, cap in MAPS:
    try:
        set_cap(path, cap)
    except Exception as error:
        FAILURES.append("%s: %s" % (path, error))
        log("ERROR %s: %s" % (path, error))
log("DONE OK" if not FAILURES else "DONE WITH FAILURES " + "; ".join(FAILURES))
