"""Fills the cactus pass of the survival start content data asset (USurvivalStartContentSettings::Cacti).

The C++ constructor already gives a new property its defaults, so an asset saved before the cactus pass normally picks
them up on load. This is the safety net: when `cacti.cactus_actors` is empty it writes the same defaults as
`USurvivalStartContentSettings::InitDefaultCacti` and saves the asset. Needs an editor built with the cactus C++.

Run in an editor (UECP execute_python, or `-ExecCmds="py <this file>"`):

    import importlib, sys
    sys.path.insert(0, r"D:/UE5Projects/RTS_Survival/Source/RTS_Survival/SurvivalMode/Docs/LevelTools")
    import survival_cacti_setup as scs; importlib.reload(scs)
    scs.ensure_cacti_defaults()            # DA_SurvivalStartContent (SM_Desert_01)
    scs.report(scs.KESSEL_ASSET)           # Kessel gets the same constructor defaults
    scs.disable_cacti(scs.KESSEL_ASSET)    # only if the Kessel starts should have no cacti

Run as a file it does ensure_cacti_defaults() + report(KESSEL_ASSET); with CACTI_SETUP_QUIT=1 it then quits the editor.
Log lines start with CACTISETUP.
"""
import os

import unreal

DESERT_ASSET = "/Game/RTS_Survival/SurvivalMode/Data/DA_SurvivalStartContent"
KESSEL_ASSET = "/Game/RTS_Survival/SurvivalMode/Data/DA_SurvivalStartContent_Kessel"

DESERT_PROPS = "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Desert/"
CACTUS_COVER = "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/NaturalCover/Cactus/"

# Same numbers as USurvivalStartContentSettings::InitDefaultCacti (SurvivalStartContentSettings.cpp).
CLUSTER_COUNT = 12
MIN_CACTI_PER_CLUSTER = 2
MAX_CACTI_PER_CLUSTER = 5
CLUSTER_RADIUS = 1000.0
MIN_DISTANCE_FROM_BUILD_EDGE = 500.0
MAX_DISTANCE_FROM_BUILD_EDGE = 9000.0
MIN_SPACING = 450.0
MAX_GROUND_SLOPE_DEGREES = 25.0
DESTRUCTIBLE_CACTUS = (DESERT_PROPS + "BP_Desert_Dest_Cactus", 3.0, 150.0)
BUSH_SIZES = (("Large", 280.0), ("Medium", 200.0), ("Small", 140.0))
BUSH_COLOURS = (("Green", 0.22), ("Blue", 0.11))
BUSH_VARIANTS = (1, 2, 3)


def log(text):
    unreal.log_warning("CACTISETUP " + str(text))


def default_entries():
    """(blueprint package path, weight, footprint radius) in the C++ order."""
    entries = [DESTRUCTIBLE_CACTUS]
    for size, footprint in BUSH_SIZES:
        for colour, weight in BUSH_COLOURS:
            for variant in BUSH_VARIANTS:
                entries.append((CACTUS_COVER + "BP_Cover_CactusBush_%s_%s_%02d" % (size, colour, variant), weight, footprint))
    return entries


def make_entry(path, weight, footprint):
    actor_class = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if actor_class is None:
        log("MISSING blueprint %s" % path)
        return None
    entry = unreal.SurvivalWeightedActorClass()
    # TSoftClassPtr properties take a loaded class in Python. Upright + random yaw are the struct defaults already.
    entry.set_editor_property("actor_class", actor_class)
    entry.set_editor_property("weight", weight)
    entry.set_editor_property("footprint_radius", footprint)
    return entry


def load_settings(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset is None:
        raise RuntimeError("could not load %s" % asset_path)
    try:
        cacti = asset.get_editor_property("cacti")
    except Exception as error:  # the editor DLL predates the cactus pass
        raise RuntimeError("%s has no 'cacti' property (rebuild the editor with the cactus C++): %s" % (asset_path, error))
    return asset, cacti


def save(asset, asset_path):
    ok = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    log("saved %s ok=%s" % (asset_path, ok))
    return ok


def report(asset_path=DESERT_ASSET):
    _, cacti = load_settings(asset_path)
    actors = list(cacti.get_editor_property("cactus_actors"))
    log("%s clusters=%d per_cluster=%d-%d radius=%.0f band=%.0f-%.0f spacing=%.0f slope=%.0f actors=%d" % (
        asset_path, cacti.get_editor_property("cluster_count"), cacti.get_editor_property("min_cacti_per_cluster"),
        cacti.get_editor_property("max_cacti_per_cluster"), cacti.get_editor_property("cluster_radius"),
        cacti.get_editor_property("min_distance_from_build_edge"), cacti.get_editor_property("max_distance_from_build_edge"),
        cacti.get_editor_property("min_spacing"), cacti.get_editor_property("max_ground_slope_degrees"), len(actors)))
    return len(actors)


def ensure_cacti_defaults(asset_path=DESERT_ASSET, force=False):
    """Writes the default cactus settings when the actor list is empty (or always with force=True). Returns the
    number of cactus actor entries on the asset afterwards."""
    asset, cacti = load_settings(asset_path)
    existing = list(cacti.get_editor_property("cactus_actors"))
    if existing and not force:
        log("%s already has %d cactus actors; nothing changed" % (asset_path, len(existing)))
        return len(existing)
    entries = [make_entry(*spec) for spec in default_entries()]
    entries = [entry for entry in entries if entry is not None]
    if not entries:
        raise RuntimeError("no cactus blueprint could be loaded")
    # Struct getters return copies: fill the copy, then write the whole struct back onto the asset.
    cacti.set_editor_property("cluster_count", CLUSTER_COUNT)
    cacti.set_editor_property("min_cacti_per_cluster", MIN_CACTI_PER_CLUSTER)
    cacti.set_editor_property("max_cacti_per_cluster", MAX_CACTI_PER_CLUSTER)
    cacti.set_editor_property("cluster_radius", CLUSTER_RADIUS)
    cacti.set_editor_property("min_distance_from_build_edge", MIN_DISTANCE_FROM_BUILD_EDGE)
    cacti.set_editor_property("max_distance_from_build_edge", MAX_DISTANCE_FROM_BUILD_EDGE)
    cacti.set_editor_property("min_spacing", MIN_SPACING)
    cacti.set_editor_property("max_ground_slope_degrees", MAX_GROUND_SLOPE_DEGREES)
    cacti.set_editor_property("cactus_actors", entries)
    asset.modify()
    asset.set_editor_property("cacti", cacti)
    save(asset, asset_path)
    log("%s filled with %d cactus actors" % (asset_path, len(entries)))
    return report(asset_path)


def disable_cacti(asset_path=KESSEL_ASSET):
    """Empties the cactus pass on an asset (cluster count 0, no actors) and saves it."""
    asset, cacti = load_settings(asset_path)
    cacti.set_editor_property("cluster_count", 0)
    cacti.set_editor_property("cactus_actors", [])
    asset.modify()
    asset.set_editor_property("cacti", cacti)
    save(asset, asset_path)
    return report(asset_path)


def _run_as_file():
    try:
        ensure_cacti_defaults()
        report(KESSEL_ASSET)
        log("DONE")
    except Exception as error:
        log("FAILED %s" % error)
    if os.environ.get("CACTI_SETUP_QUIT", "0") == "1":
        unreal.SystemLibrary.quit_editor()


if __name__ != "survival_cacti_setup":
    _run_as_file()
