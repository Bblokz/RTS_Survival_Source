"""Read-only render inventory of one or more maps (run headless, see run_inventory.sh).

Per map it writes Saved/PerfAudit/inventory_<MapName>.json with:
  meshes:    every static mesh drawn (plain, ISM/HISM, foliage, inside Blueprints), instance count, LOD0 triangles,
             LOD count, Nanite flag, total triangles drawn at LOD0, the actors that use it
  skeletal:  skeletal meshes with LOD0 triangles and use count
  materials: every material interface on those components (+ landscape, decals), with the base material,
             blend mode, shading model, two sided, and shader statistics (VS/PS instructions, samplers)
  landscape: landscape proxies with material, component count, layer count
  niagara:   Niagara systems placed in the map
Usage: perf_inventory.py <map package> [<map package> ...]
"""
import json
import os
import sys
import unreal

OUT_DIR = unreal.Paths.project_saved_dir() + "PerfAudit/"
TAG = "RTS_PERF_INVENTORY"


def log(msg):
    unreal.log_warning("%s %s" % (TAG, msg))


def path_of(obj):
    return obj.get_path_name() if obj else None


def mesh_tris(mesh, lod):
    try:
        return mesh.get_num_triangles(lod)
    except Exception:
        return -1


def mesh_info(mesh):
    info = {"lods": mesh.get_num_lods(), "tris": [], "nanite": False}
    for lod in range(info["lods"]):
        info["tris"].append(mesh_tris(mesh, lod))
    try:
        info["nanite"] = bool(mesh.get_editor_property("nanite_settings").enabled)
    except Exception:
        pass
    try:
        sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        info["verts0"] = sub.get_number_verts(mesh, 0)
    except Exception:
        info["verts0"] = -1
    try:
        sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        info["screen_sizes"] = [round(v, 4) for v in sub.get_lod_screen_sizes(mesh)]
    except Exception as exc:
        info["screen_sizes"] = str(exc)
    try:
        info["min_lod"] = mesh.get_editor_property("min_lod").default
    except Exception:
        pass
    try:
        bounds = mesh.get_bounds()
        info["radius"] = round(bounds.sphere_radius, 1)
    except Exception:
        pass
    return info


MATERIAL_CACHE = {}


def material_info(mi):
    key = path_of(mi)
    if key in MATERIAL_CACHE:
        return MATERIAL_CACHE[key]
    info = {"path": key, "class": mi.get_class().get_name()}
    try:
        base = mi.get_base_material()
        info["base"] = path_of(base)
        info["blend"] = str(base.get_editor_property("blend_mode")).split(".")[-1]
        info["shading"] = str(base.get_editor_property("shading_model")).split(".")[-1]
        info["two_sided"] = bool(base.get_editor_property("two_sided"))
        info["used_nanite"] = bool(base.get_editor_property("used_with_nanite"))
    except Exception as exc:
        info["error"] = str(exc)
    if isinstance(mi, unreal.MaterialInstance):
        try:
            info["parent"] = path_of(mi.get_editor_property("parent"))
            sw = []
            try:
                for name in unreal.MaterialEditingLibrary.get_static_switch_parameter_names(mi):
                    sw.append("%s=%s" % (name, unreal.MaterialEditingLibrary.get_material_instance_static_switch_parameter_value(mi, name)))
            except Exception:
                pass
            info["switches"] = sw
        except Exception:
            pass
    try:
        base = mi.get_base_material()
        info["nodes"] = unreal.MaterialEditingLibrary.get_num_material_expressions(base)
    except Exception:
        pass
    try:
        info["textures"] = len(unreal.MaterialEditingLibrary.get_used_textures(mi))
    except Exception:
        pass
    try:
        stats = unreal.MaterialEditingLibrary.get_statistics(mi)
        info["vs"] = stats.num_vertex_shader_instructions
        info["ps"] = stats.num_pixel_shader_instructions
        info["samplers"] = stats.num_samplers
        info["ps_tex"] = stats.num_pixel_texture_samples
        info["vs_tex"] = stats.num_vertex_texture_samples
        info["vt"] = stats.num_virtual_texture_samples
        info["interp"] = stats.num_interpolator_scalars
    except Exception as exc:
        info["stats_error"] = str(exc)
    MATERIAL_CACHE[key] = info
    return info


def survey(map_path):
    MATERIAL_CACHE.clear()
    log("loading %s" % map_path)
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if not world:
        log("FAILED to load %s" % map_path)
        return
    # load every streaming sublevel too
    try:
        for lvl in world.get_streaming_levels():
            lvl.set_should_be_loaded(True)
            lvl.set_should_be_visible(True)
        unreal.EditorLevelLibrary.get_editor_world()
    except Exception:
        pass
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    log("%s actors=%d" % (map_path, len(actors)))

    meshes = {}
    skeletal = {}
    materials_use = {}
    landscapes = []
    niagara = {}
    decals = {}
    grid = {}
    CELL = 6000.0

    def add_grid(comp, tris, count):
        try:
            if isinstance(comp, unreal.InstancedStaticMeshComponent) and count > 1:
                for i in range(min(count, 400)):
                    t = comp.get_instance_transform(i, True)
                    key = (int(t.translation.x // CELL), int(t.translation.y // CELL))
                    cell = grid.setdefault(key, [0, 0, 0.0])
                    cell[0] += tris * count / min(count, 400)
                    cell[1] += 1
                    cell[2] = max(cell[2], t.translation.z)
                return
            loc = comp.get_world_location()
            key = (int(loc.x // CELL), int(loc.y // CELL))
            cell = grid.setdefault(key, [0, 0, 0.0])
            cell[0] += tris
            cell[1] += 1
            cell[2] = max(cell[2], loc.z)
        except Exception:
            pass

    def use_material(mi, count, where):
        if not mi:
            return
        key = path_of(mi)
        entry = materials_use.setdefault(key, {"uses": 0, "instances": 0, "where": {}})
        entry["uses"] += 1
        entry["instances"] += count
        entry["where"][where] = entry["where"].get(where, 0) + count
        material_info(mi)

    for actor in actors:
        aclass = actor.get_class().get_name()
        label = actor.get_actor_label() if hasattr(actor, "get_actor_label") else actor.get_name()
        if isinstance(actor, unreal.LandscapeProxy):
            try:
                comps = actor.get_components_by_class(unreal.LandscapeComponent)
                lm = actor.get_editor_property("landscape_material")
                landscapes.append({"actor": label, "class": aclass, "material": path_of(lm), "components": len(comps)})
                use_material(lm, len(comps), "Landscape")
            except Exception as exc:
                landscapes.append({"actor": label, "error": str(exc)})
            continue
        for comp in actor.get_components_by_class(unreal.PrimitiveComponent):
            hidden = False
            try:
                hidden = bool(comp.get_editor_property("hidden_in_game")) or not comp.is_visible()
            except Exception:
                pass
            if isinstance(comp, unreal.StaticMeshComponent):
                mesh = comp.get_editor_property("static_mesh")
                if not mesh:
                    continue
                count = 1
                if isinstance(comp, unreal.InstancedStaticMeshComponent):
                    count = comp.get_instance_count()
                    if count == 0:
                        continue
                key = path_of(mesh)
                entry = meshes.get(key)
                if entry is None:
                    entry = {"instances": 0, "hidden": 0, "actors": {}, "shadow": 0, "comps": 0}
                    entry.update(mesh_info(mesh))
                    meshes[key] = entry
                entry["instances"] += count
                entry["comps"] += 1
                if hidden:
                    entry["hidden"] += count
                try:
                    if comp.get_editor_property("cast_shadow"):
                        entry["shadow"] += count
                except Exception:
                    pass
                entry["actors"][aclass] = entry["actors"].get(aclass, 0) + count
                if not hidden:
                    add_grid(comp, entry["tris"][0] if entry["tris"] else 0, count)
                try:
                    for mi in comp.get_materials():
                        use_material(mi, count, key)
                except Exception:
                    pass
            elif isinstance(comp, unreal.SkinnedMeshComponent):
                try:
                    mesh = comp.get_editor_property("skeletal_mesh_asset")
                except Exception:
                    try:
                        mesh = comp.get_editor_property("skeletal_mesh")
                    except Exception:
                        mesh = None
                if not mesh:
                    continue
                key = path_of(mesh)
                entry = skeletal.setdefault(key, {"uses": 0, "actors": {}})
                entry["uses"] += 1
                entry["actors"][aclass] = entry["actors"].get(aclass, 0) + 1
                try:
                    for mi in comp.get_materials():
                        use_material(mi, 1, key)
                except Exception:
                    pass
            elif isinstance(comp, unreal.DecalComponent):
                mi = comp.get_decal_material()
                decals[path_of(mi)] = decals.get(path_of(mi), 0) + 1
                use_material(mi, 1, "Decal")
            elif comp.get_class().get_name() == "NiagaraComponent":
                try:
                    asset = comp.get_asset()
                    niagara[path_of(asset)] = niagara.get(path_of(asset), 0) + 1
                except Exception:
                    pass
            else:
                try:
                    for mi in comp.get_materials():
                        use_material(mi, 1, comp.get_class().get_name())
                except Exception:
                    pass

    for key, entry in meshes.items():
        tris0 = entry["tris"][0] if entry["tris"] else 0
        entry["total_tris0"] = tris0 * entry["instances"]
    name = map_path.split("/")[-1]
    out = {"map": map_path, "actors": len(actors), "meshes": meshes, "skeletal": skeletal,
           "materials": {k: dict(MATERIAL_CACHE.get(k, {}), **v) for k, v in materials_use.items()},
           "landscape": landscapes, "niagara": niagara, "decals": decals,
           "grid_cell": CELL,
           "grid": sorted([[k[0], k[1], int(v[0]), v[1], round(v[2])] for k, v in grid.items()], key=lambda c: -c[2])}
    os.makedirs(OUT_DIR, exist_ok=True)
    path = OUT_DIR + "inventory_%s.json" % name
    with open(path, "w") as fh:
        json.dump(out, fh, indent=1)
    log("wrote %s meshes=%d skeletal=%d materials=%d" % (path, len(meshes), len(skeletal), len(materials_use)))


for arg in sys.argv[1:]:
    try:
        survey(arg)
    except Exception as exc:
        log("ERROR %s %s" % (arg, exc))
log("DONE")
