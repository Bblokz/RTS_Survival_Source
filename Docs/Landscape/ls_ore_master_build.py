"""Ger_2ndOreMountains has its own landscape master since 2026-11-17 (user decision): a copy of the shared DryCastle
master with the three mineral layers of ls_ore_layers_build.py (IronOxide, Ochre, SlateScree).

    /Game/RTS_Survival/Materials/ground/Landscape/OreMountains/
        M_RTS_Landscape_OreMountains        duplicate of M_RTS_Landscape_DryCastle (calls the same shared MF_RTS_LS_*)
        MI_RTS_Landscape_OreMountains_Base  duplicate of MI_RTS_Landscape_DryCastle_Inst, parent = the copy
    CastleGrassDesertDry/MI_RTS_Landscape_DryCastle_OreMountains   the map's instance, reparented to the base copy

The shared master and its instances are not touched. Later changes to the shared master's own graph do not reach
this map; changes inside the shared functions still do.

    import sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Docs/Landscape")
    import ls_ore_master_build as M; M.run()      # creates what is missing, builds the layers, saves when clean
"""
import importlib

import unreal

import ls_ore_layers_build as B

importlib.reload(B)
SHARED = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/"
ORE_DIR = "/Game/RTS_Survival/Materials/ground/Landscape/OreMountains"
MASTER = ORE_DIR + "/M_RTS_Landscape_OreMountains"
BASE = ORE_DIR + "/MI_RTS_Landscape_OreMountains_Base"
MAP_INSTANCE = SHARED + "MI_RTS_Landscape_DryCastle_OreMountains"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def ensure_copies():
    made = []
    if not eal.does_asset_exist(MASTER):
        if eal.duplicate_asset(SHARED + "M_RTS_Landscape_DryCastle", MASTER) is None:
            raise RuntimeError("could not duplicate the master")
        made.append("master")
    if not eal.does_asset_exist(BASE):
        if eal.duplicate_asset(SHARED + "MI_RTS_Landscape_DryCastle_Inst", BASE) is None:
            raise RuntimeError("could not duplicate the base instance")
        made.append("base")
    master, base, instance = unreal.load_asset(MASTER), unreal.load_asset(BASE), unreal.load_asset(MAP_INSTANCE)
    if base.get_editor_property("parent") != master:
        mel.set_material_instance_parent(base, master)
        mel.update_material_instance(base)
        made.append("base parent")
    if instance.get_editor_property("parent") != base:
        mel.set_material_instance_parent(instance, base)
        mel.update_material_instance(instance)
        made.append("map instance parent")
    return made


def run(save=True):
    shared_stats = B.stats([SHARED + "M_RTS_Landscape_DryCastle", SHARED + "MI_RTS_Landscape_DryCastle_Inst"])
    made = ensure_copies()
    B.MAT_PATH = MASTER
    failures = B.run()
    stats = B.stats([MASTER, BASE, MAP_INSTANCE])
    print("made", made)
    print("shared stats", shared_stats)
    print("ore stats   ", stats)
    measured = shared_stats["M_RTS_Landscape_DryCastle"][0] > 0          # a commandlet reports 0 everywhere
    if measured and stats[MASTER.split("/")[-1]][:2] != shared_stats["M_RTS_Landscape_DryCastle"][:2]:
        failures.append("the copy does not compile like the shared master")
    if measured and stats[MAP_INSTANCE.split("/")[-1]][0] <= 0:
        failures.append("the map instance does not compile")
    if failures or not save:
        print("NOT SAVED", failures)
        return failures
    saved = [eal.save_loaded_asset(unreal.load_asset(path), False) for path in (MASTER, BASE, MAP_INSTANCE)]
    print("SAVED", saved)
    if not all(saved):
        failures.append("save failed %s" % saved)
    return failures
