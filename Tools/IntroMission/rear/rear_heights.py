"""Read-only height sample of Ger_IntroMission by line traces (commandlet): Saved/IntroRear/heights_<step>.npy-like
text grid. Grid covers the whole landscape; z is the first blocking hit (landscape + meshes) and the landscape-only
height (trace against the landscape component only is not possible from python, so we also record the hit actor
class name for non-landscape hits)."""
import os
import unreal

MAP = '/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission'
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'IntroRear')
STEP = 400.0
X0, X1 = -41100.0, 24900.0
Y0, Y1 = -25200.0, 25800.0


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    land = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Landscape)][0]
    rows = []
    nx = int((X1 - X0) / STEP) + 1
    ny = int((Y1 - Y0) / STEP) + 1
    rows.append('%f %f %f %d %d' % (X0, Y0, STEP, nx, ny))
    for ix in range(nx):
        x = X0 + ix * STEP
        vals = []
        for iy in range(ny):
            y = Y0 + iy * STEP
            hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                         unreal.DrawDebugTrace.NONE, True)
            if hit is None:
                vals.append('nan')
                continue
            t = hit.to_tuple()
            loc = t[4]
            actor = t[9]
            tag = 'L' if actor == land else 'M'
            vals.append('%.0f%s' % (loc.z, tag))
        rows.append(' '.join(vals))
    with open(os.path.join(OUT, 'heights_400.txt'), 'w') as f:
        f.write('\n'.join(rows))
    unreal.log_warning('IntroRear heights written')


main()
