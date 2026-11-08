"""Sets AMissionManager::PlayerGlobalAbilitySettings (tier cap + free costs) on the campaign missions the user set
(2026-11-05 caps, 2026-11-07 struct + free costs):
  Ger_FirstMission       (first German mission)        Tier I only, global abilities cost no resources
  Ger_MarderStugMission  (the StuG / Marder mission)    up to Tier II, normal costs
Every other mission keeps the default (Tier IV, normal costs). Loads each map, sets the property on its mission manager instance
(BP_MissionManager), saves the map and checks the file kept its size (a navmesh rebuild during the save would shrink
it; the script runs a blocking navmesh build first). Idempotent: a map that already has the cap is not saved. Lines prefixed [Round5Caps].
Run on the DebugGame build (hold the build lock); nothing else may have the maps open. Not as a commandlet (it
crashes saving the landscape); start a full editor that runs the script and quits:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject <mission map> -ExecutePythonScript=<this file>
      -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -unattended
once per mission map (the map is the editor's startup map; the script waits, sets, saves and quits).
"""
import os
import shutil
import time

import unreal

MAPS = [
    ("/Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission", "TierI", True),
    ("/Game/RTS_Survival/Maps/GerMissions/Ger_MarderStugMission", "TierII", False),
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


def load_and_check(map_path, tier_name, free_costs):
    """Loads the map; returns the job for save_cap, or None when nothing has to be saved."""
    file_path = disk_path(map_path)
    job = {"map": map_path, "file": file_path, "size": os.path.getsize(file_path),
           "mtime": os.path.getmtime(file_path), "tier": tier(tier_name), "free": free_costs}
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if world is None:
        FAILURES.append("load " + map_path)
        log("LOAD FAILED " + map_path)
        return None
    managers = mission_managers(world)
    log("%s: %d mission manager(s), file %d bytes" % (map_path, len(managers), job["size"]))
    if len(managers) != 1:
        FAILURES.append("%s has %d mission managers" % (map_path, len(managers)))
        return None
    manager = managers[0]
    settings = manager.get_editor_property("player_global_ability_settings")
    current = settings.get_editor_property("max_allowed_tier")
    current_free = settings.get_editor_property("disable_resource_costs")
    log("  %s %s: cap %s -> %s, free costs %s -> %s, hide panel %s" % (
        manager.get_name(), manager.get_class().get_name(), current, job["tier"], current_free, free_costs,
        manager.get_editor_property("hide_global_abilities_for_player")))
    if current == job["tier"] and current_free == free_costs:
        log("  already set, not saving")
        return None
    job["world"] = world
    job["manager"] = manager
    return job


def save_cap(job):
    world, manager, file_path, map_path = job["world"], job["manager"], job["file"], job["map"]
    navmeshes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh)
    log("  %d navmesh actor(s), nav being built: %s" % (
        len(navmeshes), unreal.NavigationSystemV1.is_navigation_being_built(world)))
    # The editor starts rebuilding the navmesh when the map opens; saving mid-build drops the tiles (the file halves).
    # RebuildNavigation is the blocking Build Paths (UNavigationSystemV1::Build waits for every nav data).
    started = time.time()
    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    log("  RebuildNavigation took %.1f s, nav being built: %s" % (
        time.time() - started, unreal.NavigationSystemV1.is_navigation_being_built(world)))
    os.makedirs(BACKUP_DIR, exist_ok=True)
    backup = os.path.join(BACKUP_DIR, os.path.basename(file_path) + time.strftime(".%H%M%S.bak"))
    shutil.copy2(file_path, backup)
    log("  backup %s" % backup)
    settings = manager.get_editor_property("player_global_ability_settings")
    manager.modify()
    settings.set_editor_property("max_allowed_tier", job["tier"])
    settings.set_editor_property("disable_resource_costs", job["free"])
    manager.set_editor_property("player_global_ability_settings", settings)
    saved = unreal.EditorLoadingAndSavingUtils.save_map(world, map_path)
    size_after = os.path.getsize(file_path)
    reached_disk = os.path.getmtime(file_path) > job["mtime"]
    log("  save=%s reached_disk=%s size %d -> %d (%.3f)" % (saved, reached_disk, job["size"], size_after,
                                                             size_after / float(job["size"])))
    if not saved or not reached_disk:
        FAILURES.append("save " + map_path)
    elif size_after < job["size"] * MIN_SIZE_RATIO:
        FAILURES.append("%s shrank on save (%d -> %d): restored the backup" % (map_path, job["size"], size_after))
        shutil.copy2(file_path, backup + ".shrunk")
        shutil.copy2(backup, file_path)
        return
    with open(file_path, "rb") as handle:
        has_property = b"PlayerGlobalAbilitySettings" in handle.read()
    log("  property tag on disk: %s" % has_property)
    if not has_property:
        FAILURES.append("no property tag in " + map_path)


def finish():
    log("DONE OK" if not FAILURES else "DONE WITH FAILURES " + "; ".join(FAILURES))


def check_open_world(world):
    """Full editor started on one of the mission maps: returns its job, or None when nothing has to be saved."""
    map_path = world.get_outermost().get_name()
    for path, cap, free in MAPS:
        if path == map_path:
            file_path = disk_path(path)
            job = {"map": path, "file": file_path, "size": os.path.getsize(file_path),
                   "mtime": os.path.getmtime(file_path), "tier": tier(cap), "free": free, "world": world}
            managers = mission_managers(world)
            log("%s: %d mission manager(s), file %d bytes" % (path, len(managers), job["size"]))
            if len(managers) != 1:
                FAILURES.append("%s has %d mission managers" % (path, len(managers)))
                return None
            job["manager"] = managers[0]
            settings = job["manager"].get_editor_property("player_global_ability_settings")
            log("  %s: cap %s -> %s, free costs %s -> %s, hide panel %s" % (
                job["manager"].get_name(), settings.get_editor_property("max_allowed_tier"), job["tier"],
                settings.get_editor_property("disable_resource_costs"), free,
                job["manager"].get_editor_property("hide_global_abilities_for_player")))
            if settings.get_editor_property("max_allowed_tier") == job["tier"]                     and settings.get_editor_property("disable_resource_costs") == free:
                log("  already set, not saving")
                return None
            return job
    FAILURES.append("editor opened %s, which is not a mission in MAPS" % map_path)
    return None


IS_COMMANDLET = "-run=pythonscript" in unreal.SystemLibrary.get_command_line().lower()
# Ticks the editor runs after the map opened before it is saved, so async loads and compiles settle first.
SETTLE_TICKS = 120

if IS_COMMANDLET:
    # Commandlets crash in landscape collision PreSave (no renderer to finish the landscape material); kept for
    # maps without a landscape only.
    for path, cap, free in MAPS:
        try:
            loaded_job = load_and_check(path, cap, free)
            if loaded_job is not None:
                save_cap(loaded_job)
        except Exception as error:
            FAILURES.append("%s: %s" % (path, error))
            log("ERROR %s: %s" % (path, error))
    finish()
else:
    # One map per editor run, given as the editor's startup map (it is open when the script runs; the editor quits
    # once the script returns, so this runs synchronously).
    try:
        open_job = check_open_world(unreal.EditorLevelLibrary.get_editor_world())
        if open_job is not None:
            save_cap(open_job)
    except Exception as error:
        FAILURES.append("editor run: %s" % error)
    finish()
    unreal.SystemLibrary.quit_editor()
