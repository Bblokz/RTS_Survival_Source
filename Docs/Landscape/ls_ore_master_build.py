"""Ger_2ndOreMountains has its own landscape master since 2026-11-17 (user decision): a copy of the shared DryCastle
master with the three mineral layers of ls_ore_layers_build.py (IronOxide, Ochre, SlateScree).

    /Game/RTS_Survival/Materials/ground/Landscape/OreMountains/
        M_RTS_Landscape_OreMountains        duplicate of M_RTS_Landscape_DryCastle (calls the same shared MF_RTS_LS_*)
        MI_RTS_Landscape_OreMountains_Base  duplicate of MI_RTS_Landscape_DryCastle_Inst, parent = the copy
    CastleGrassDesertDry/MI_RTS_Landscape_DryCastle_OreMountains   the map's instance, reparented to the base copy

The shared master and its instances are not touched. Later changes to the shared master's own graph do not reach
this map; changes inside the shared MF_RTS_LS_* functions still do.

Texture budget (2026-11-21): with the three mineral layers the copy bound 45 material textures + 21 engine SRVs = 66
in the Nanite base pass with the volumetric-lightmap lighting policy (the D3D12 limit is 64). The grey default
material then replaced the ground in -game and Movie Render Queue. The editor did not show it. UE binds a texture once
per (parameter name, texture, sampler source). The shared Beach, Desert and Rock_Desert layer functions bind the same
sand and rock textures under up to four parameter names. So the copy calls Ore-local forks of those three functions
(OreMountains/Layers/*_Ore). In the forks, every texture parameter that holds the same texture as another one carries
one shared name. The look is unchanged, because each merged name resolved to that same texture on the map's instance.
Later edits to the shared MF_RTS_Layer_Beach / _Desert / _Rock_Desert do not reach this map. fork_layers() never
refreshes an existing fork. To take such an edit (UNTESTED): repoint the copy's three calls back to the shared
functions, then delete the forks, then run fork_layers(). Deleting a fork first leaves empty calls, and the run stops
with "no call to ...". The margin is unmeasured: a clean compile leaves no shader dump, so the only known figure is
64 or fewer. Check the budget before adding a texture layer here.

    import sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Docs/Landscape")
    import ls_ore_master_build as M; M.run()      # creates what is missing, builds the layers, saves when clean
    M.fork_layers()                               # only the layer forks + repoint; saves when clean
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
    failures += fork_layers(save=False)
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
    saved = [eal.save_loaded_asset(unreal.load_asset(path), False)
             for path in (MASTER, BASE, MAP_INSTANCE) + tuple(fork_path(name) for name in FORKED_LAYERS)]
    print("SAVED", saved)
    if not all(saved):
        failures.append("save failed %s" % saved)
    return failures


# --- Ore-local layer forks (texture budget, see the module doc) -------------------------------------------------------
LAYER_DIR = SHARED + "Layers/"
FORK_DIR = ORE_DIR + "/Layers/"
FORKED_LAYERS = ("MF_RTS_Layer_Beach", "MF_RTS_Layer_Desert", "MF_RTS_Layer_Rock_Desert")
TEXTURE_PARAMETER_CLASSES = ("MaterialExpressionTextureObjectParameter", "MaterialExpressionTextureSampleParameter2D")


def fork_path(name):
    return FORK_DIR + name + "_Ore"


def own_expressions(owner):
    return [e for e in unreal.ObjectIterator(unreal.MaterialExpression)
            if e.get_outer() == owner and unreal.SystemLibrary.is_valid(e)]


def function_calls(material):
    """{call expression: its function's asset name} for every function call in the material's own graph."""
    calls = {}
    for e in own_expressions(material):
        if e.get_class().get_name() != "MaterialExpressionMaterialFunctionCall":
            continue
        function = e.get_editor_property("material_function")
        if function is not None:
            calls[e] = function.get_name()
    return calls


def wiring(material):
    """Every input link in the material's own graph, by expression object name (Python cannot read the attributes pin)."""
    links = {}
    for e in own_expressions(material):
        sources = mel.get_inputs_for_material_expression(material, e)
        names = []
        for source in sources:
            if source is None:
                names.append(None)
                continue
            names.append((source.get_name(), str(mel.get_input_node_output_name_for_material_expression(e, source))))
        links[e.get_name()] = names
    return links


def ensure_forks():
    made = []
    for name in FORKED_LAYERS:
        if not eal.does_asset_exist(fork_path(name)):
            if eal.duplicate_asset(LAYER_DIR + name, fork_path(name)) is None:
                raise RuntimeError("could not fork " + name)
            made.append(name)
        fork = unreal.load_asset(fork_path(name))
        try:
            fork.set_editor_property("expose_to_library", False)
        except Exception as error:          # cosmetic only: keeps the forks out of the function palette
            print("expose_to_library not set on %s: %s" % (name, error))
    return made


def texture_parameters(function):
    return [e for e in own_expressions(function) if e.get_class().get_name() in TEXTURE_PARAMETER_CLASSES]


def merge_texture_names(instance, failures):
    """In the forks, give every texture parameter that holds the same texture (and sampler type) one shared name: the
    shortest of its names. Refuses a merge when the map's instance resolves any merged name to another texture."""
    groups = {}
    for name in FORKED_LAYERS:
        for e in texture_parameters(unreal.load_asset(fork_path(name))):
            texture = e.get_editor_property("texture")
            if texture is None:
                continue
            key = (texture.get_path_name(), str(e.get_editor_property("sampler_type")))
            groups.setdefault(key, []).append(e)
    renamed = []
    for (texture_path, _), nodes in sorted(groups.items()):
        names = sorted({str(e.get_editor_property("parameter_name")) for e in nodes}, key=lambda n: (len(n), n))
        if len(names) < 2:
            continue
        resolved = {n: mel.get_material_instance_texture_parameter_value(instance, n) for n in names}
        wrong = [n for n, value in resolved.items() if value is None or value.get_path_name() != texture_path]
        if wrong:
            failures.append("not merged (instance resolves %s elsewhere): %s" % (wrong, texture_path))
            continue
        for e in nodes:
            if str(e.get_editor_property("parameter_name")) != names[0]:
                e.set_editor_property("parameter_name", names[0])
        renamed.append((texture_path.split(".")[-1], names[0], names[1:]))
    return renamed


def check_unique_names(material, failures):
    """After the merge, one parameter name may only ever hold one texture anywhere in the copy and its functions."""
    owners, queue, textures = [material], [material], {}
    while queue:
        owner = queue.pop()
        for e in own_expressions(owner):
            cls = e.get_class().get_name()
            if cls == "MaterialExpressionMaterialFunctionCall":
                function = e.get_editor_property("material_function")
                if function is not None and function not in owners:
                    owners.append(function)
                    queue.append(function)
            elif cls in TEXTURE_PARAMETER_CLASSES and e.get_editor_property("texture") is not None:
                name = str(e.get_editor_property("parameter_name"))
                textures.setdefault(name, set()).add(e.get_editor_property("texture").get_path_name())
    for name, paths in textures.items():
        if len(paths) > 1:
            failures.append("parameter %s holds %d textures: %s" % (name, len(paths), sorted(paths)))


def fork_layers(save=True):
    failures = []
    master, instance = unreal.load_asset(MASTER), unreal.load_asset(MAP_INSTANCE)
    made = ensure_forks()
    renamed = merge_texture_names(instance, failures)
    before = wiring(master)
    repointed = []
    for call, function_name in function_calls(master).items():
        if function_name in FORKED_LAYERS:
            call.set_editor_property("material_function", unreal.load_asset(fork_path(function_name)))
            repointed.append(function_name)
    after = wiring(master)
    if before != after:
        changed = sorted(k for k in set(before) | set(after) if before.get(k) != after.get(k))
        failures.append("wiring changed on %s" % changed)
    called = set(function_calls(master).values())
    missing = [name for name in FORKED_LAYERS if name + "_Ore" not in called]
    stale = sorted(called & set(FORKED_LAYERS))
    if stale:
        failures.append("the copy still calls the shared %s" % stale)
    if missing:
        failures.append("no call to %s in the copy" % missing)
    check_unique_names(master, failures)
    for name in FORKED_LAYERS:
        mel.update_material_function(unreal.load_asset(fork_path(name)), None)
    mel.recompile_material(master)
    print("forks made", made)
    print("merged names", renamed)
    print("calls repointed", repointed)
    if failures or not save:
        print("fork_layers", "NOT SAVED" if failures else "not saved (save=False)", failures)
        return failures
    paths = (MASTER,) + tuple(fork_path(name) for name in FORKED_LAYERS)
    saved = [eal.save_loaded_asset(unreal.load_asset(path), False) for path in paths]
    print("SAVED", list(zip(paths, saved)))
    if not all(saved):
        failures.append("save failed %s" % saved)
    return failures
