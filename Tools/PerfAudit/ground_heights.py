"""Logs landscape/ground Z at X,Y points of a map (for probe camera heights).
Usage: ground_heights.py <map> X,Y X,Y ..."""
import sys
import unreal
world = unreal.EditorLoadingAndSavingUtils.load_map(sys.argv[1])
for pt in sys.argv[2:]:
    x, y = [float(v) for v in pt.split(",")]
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 100000), unreal.Vector(x, y, -100000),
                                                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    t = hit.to_tuple()
    z = t[5].z if t[0] else None
    unreal.log_warning("RTS_GROUND %s %s %s" % (sys.argv[1].split("/")[-1], pt, z))
