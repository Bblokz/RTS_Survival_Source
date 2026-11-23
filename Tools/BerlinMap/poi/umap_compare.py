# Commandlet (read-only): load two copies of Ger_Berlin from /Game/_TmpCompare and dump what they hold, to explain a
# file-size change. Writes Saved/Berlin/logs/umap_compare.json.
import collections, json
import unreal

OUT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/logs/umap_compare.json"
res = {}
for name in ("Ger_Berlin_Old", "Ger_Berlin_New"):
    path = "/Game/_TmpCompare/" + name
    world = unreal.load_asset(path)
    if world is None:
        res[name] = "load failed"
        continue
    by_class = collections.Counter()
    inst = collections.Counter()
    comps = collections.Counter()
    pkg = path
    objs = collections.Counter()
    for o in unreal.ObjectIterator(unreal.Object):
        if o.get_outermost().get_name() == pkg:
            objs[o.get_class().get_name()] += 1
    res.setdefault("objects", {})[name] = dict(objs.most_common(40))
    for a in unreal.ObjectIterator(unreal.Actor):
        if a.get_outermost().get_name() != pkg:
            continue
        by_class[a.get_class().get_name()] += 1
        for c in a.get_components_by_class(unreal.ActorComponent):
            comps[c.get_class().get_name()] += 1
            if isinstance(c, unreal.InstancedStaticMeshComponent):
                inst[a.get_actor_label()[:24]] += c.get_instance_count()
    res[name] = {"actors": sum(by_class.values()), "by_class": dict(by_class.most_common(40)),
                 "components": dict(comps.most_common(40)), "instances_total": sum(inst.values()),
                 "instances_top": dict(inst.most_common(25))}
json.dump(res, open(OUT, "w"), indent=1)
print("UMAP_COMPARE written", OUT)
