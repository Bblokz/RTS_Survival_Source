# Commandlet (read-only): which actor owns the Enemy* components in each copy.
import json
import unreal
out = {}
for name in ("Ger_Berlin_Old", "Ger_Berlin_New"):
    pkg = "/Game/_TmpCompare/" + name
    unreal.load_asset(pkg)
    rows = []
    for a in unreal.ObjectIterator(unreal.Actor):
        if a.get_outermost().get_name() != pkg:
            continue
        cn = a.get_class().get_name()
        if "Enemy" in cn or "Controller" in cn:
            rows.append([a.get_name(), cn, a.get_actor_label(), [c.get_class().get_name() for c in a.get_components_by_class(unreal.ActorComponent)]])
    out[name] = rows
json.dump(out, open("D:/UE5Projects/RTS_Survival/Saved/Berlin/logs/umap_compare_enemy.json", "w"), indent=1)
