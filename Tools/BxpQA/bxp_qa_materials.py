"""Lists the material slots (slot name -> material path, base material) of every static mesh used by the audited BXPs,
plus sockets and bounds. Reads bxp_qa_audit.json; writes bxp_qa_materials.json."""
import json
import unreal

ROOT = "D:/UE5Projects/RTS_Survival/Source/Tools/BxpQA"
data = json.load(open(ROOT + "/bxp_qa_audit.json", encoding="utf-8"))
out = {}
for r in data["results"]:
    for c in r.get("components", []):
        path = c.get("mesh")
        if not path or path in out:
            continue
        mesh = unreal.load_asset(path)
        if mesh is None:
            continue
        slots = []
        for sm in mesh.get_editor_property("static_materials"):
            mat = sm.get_editor_property("material_interface")
            base = mat.get_base_material() if mat else None
            slots.append([str(sm.get_editor_property("material_slot_name")), mat.get_path_name() if mat else None,
                          base.get_path_name() if base else None])
        box = mesh.get_bounding_box()
        sockets = {}
        out[path] = {"slots": slots, "min": [box.min.x, box.min.y, box.min.z], "max": [box.max.x, box.max.y, box.max.z],
                     "tris": mesh.get_num_triangles(0), "sockets": sockets}
json.dump(out, open(ROOT + "/bxp_qa_materials.json", "w", encoding="utf-8"), indent=1)
unreal.log("BXPQA materials done %d" % len(out))
