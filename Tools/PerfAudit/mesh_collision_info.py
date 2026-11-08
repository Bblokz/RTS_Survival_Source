"""Logs collision setup of static meshes: trace flag, LOD used for complex collision, simple shape counts."""
import sys
import unreal
for p in sys.argv[1:]:
    m = unreal.load_asset(p)
    if not m:
        unreal.log_warning("RTS_COLL missing %s" % p); continue
    bs = m.get_editor_property("body_setup")
    flag = str(bs.get_editor_property("collision_trace_flag")).split(".")[-1] if bs else "NOBODY"
    try:
        ag = bs.get_editor_property("agg_geom")
        counts = "box=%d sph=%d cap=%d cvx=%d" % (len(ag.get_editor_property("box_elems")), len(ag.get_editor_property("sphere_elems")),
                                                  len(ag.get_editor_property("sphyl_elems")), len(ag.get_editor_property("convex_elems")))
    except Exception as e:
        counts = "agg?%s" % e
    try:
        lfc = m.get_editor_property("lod_for_collision")
    except Exception:
        lfc = "?"
    unreal.log_warning("RTS_COLL %s flag=%s lodcoll=%s %s pct0=%.2f" % (p.split('/')[-1], flag, lfc, counts,
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_lod_reduction_settings(m, 0).percent_triangles))
