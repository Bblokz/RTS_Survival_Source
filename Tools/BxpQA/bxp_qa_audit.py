"""German building expansion QA audit (headless).

Run: UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
Writes Source/Tools/BxpQA/bxp_qa_audit.json + .txt: per German BXP class, components (inherited included), static
meshes used, damage-swap meshes referenced, WallHoleDamage presence, DT_Items row (brush / name / description /
tooltip), async spawner registration. Re-run after fixes: this is the acceptance test.
"""
import json
import os
import unreal

ROOT = "D:/UE5Projects/RTS_Survival/Source/Tools/BxpQA"
LIST = ROOT + "/bxp_list.txt"
DT_PATH = "/Game/RTS_Survival/Blueprints/GameUI/BuildingUI/ExpansionItems/DT_Items"
SPAWNER = "/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner"
DAMAGE_TOKENS = ("At50", "At33", "_Damaged", "Destroyed")


def log(msg):
    unreal.log("BXPQA " + msg)


def components_of(bp):
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = sub.k2_gather_subobject_data_for_blueprint(bp)
    out = []
    for h in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is None or isinstance(obj, unreal.Actor):
            continue
        entry = {"name": obj.get_name(), "class": obj.get_class().get_name()}
        if isinstance(obj, unreal.StaticMeshComponent):
            sm = obj.get_editor_property("static_mesh")
            entry["mesh"] = sm.get_path_name() if sm else None
            entry["materials"] = [m.get_path_name() if m else None for m in obj.get_editor_property("override_materials")]
        out.append(entry)
    return out


def referenced_packages(path):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
                                                 include_hard_package_references=True)
    deps = reg.get_dependencies(path, opts) or []
    return [str(d) for d in deps]


def main():
    dt = unreal.load_asset(DT_PATH)
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt))
    rows_by_name = {r.get("Name"): r for r in rows}

    spawner_cls = unreal.load_object(None, SPAWNER + ".RTS_BP_RTSAsyncSpawner_C")
    cdo = unreal.get_default_object(spawner_cls)
    bxp_map = cdo.get_editor_property("BuildingExpansionMap")
    preview_map = cdo.get_editor_property("BxpPreviewMeshMap")
    spawn_keys = {str(k).split(".")[-1]: str(v) for k, v in bxp_map.items()}
    preview_keys = {str(k).split(".")[-1]: (v.get_path_name() if v else None) for k, v in preview_map.items()}

    results = []
    for line in open(LIST, encoding="utf-8"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        type_name, bp_path = line.split()
        entry = {"type": type_name, "bp": bp_path}
        bp = unreal.load_asset(bp_path)
        if bp is None:
            entry["error"] = "missing"
            results.append(entry)
            continue
        comps = components_of(bp)
        entry["components"] = comps
        entry["wall_holes"] = [c["name"] for c in comps if "WallHole" in c["class"]]
        deps = referenced_packages(bp_path.rsplit(".", 1)[0])
        entry["damage_refs"] = sorted(d for d in deps if any(t in d for t in DAMAGE_TOKENS))
        row = rows_by_name.get(type_name)
        if row is not None:
            row = {k: (str(v)[:300] if v is not None else None) for k, v in row.items()}
        entry["dt_row"] = row
        enum_key = type_name.upper()
        entry["spawner"] = next((v for k, v in spawn_keys.items() if k.upper() == enum_key), None)
        entry["preview"] = next((v for k, v in preview_keys.items() if k.upper() == enum_key), None)
        results.append(entry)
        log("%s holes=%d dmg=%d row=%s spawn=%s" % (type_name, len(entry["wall_holes"]), len(entry["damage_refs"]),
                                                    row is not None, entry["spawner"] is not None))

    with open(ROOT + "/bxp_qa_audit.json", "w", encoding="utf-8") as f:
        json.dump({"rows_all": rows, "spawn_keys": spawn_keys, "results": results}, f, indent=1)
    log("done %d" % len(results))


main()
