"""Logs tris per LOD, screen sizes, radius, LOD0 reduction pct and material slots of static meshes (read-only)."""
import sys
import unreal
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for p in sys.argv[1:]:
    m = unreal.load_asset(p)
    if not m:
        unreal.log_warning("RTS_LODINFO missing %s" % p); continue
    tris = [m.get_num_triangles(i) for i in range(m.get_num_lods())]
    ss = [round(v, 3) for v in sub.get_lod_screen_sizes(m)]
    pct = sub.get_lod_reduction_settings(m, 0).percent_triangles
    unreal.log_warning("RTS_LODINFO %s r=%.0f tris=%s ss=%s pct0=%.2f slots=%d sockets=%d" % (p.split('/')[-1], m.get_bounds().sphere_radius, tris, ss, pct, len(m.static_materials), len(m.sockets) if hasattr(m,'sockets') else -1))
