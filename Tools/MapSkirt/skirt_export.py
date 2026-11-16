"""Read-only: export a map's whole landscape (heights + paint weights) and actor positions for skirt planning.

    env SKIRT_MAP = /Game/.../MapName   ->  Saved/MapSkirt/<MapName>/full_height.r16, full_<layer>.r8, grid.txt,
                                            actors.tsv (label, class, x, y, z, bounds extent), blockers.tsv
Needs the DebugGame editor build (URTSShortsLandscapeLibrary.ExportLandscapeRegion).
"""
import os
import unreal

MAP = os.environ.get('SKIRT_MAP', '/Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission')
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'MapSkirt', MAP.split('/')[-1]).replace('\\', '/') + '/'


def main():
    os.makedirs(OUT, exist_ok=True)
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    acts = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    lands = [a for a in acts if isinstance(a, unreal.Landscape)]
    if len(lands) != 1:
        raise RuntimeError('expected one landscape, found %d' % len(lands))
    land = lands[0]
    grid = [int(v) for v in unreal.RTSShortsLandscapeLibrary.get_landscape_grid_info(land).split()]
    tl = land.get_editor_property('target_layers')
    names = [str(k) for k in tl.keys()]
    infos = [tl[k].get_editor_property('layer_info_obj') for k in tl.keys()]
    ok = unreal.RTSShortsLandscapeLibrary.export_landscape_region(
        land, grid[0], grid[1], grid[2], grid[3], OUT + 'full_height.r16', infos, [OUT + 'full_%s.r8' % n for n in names])
    loc, sc = land.get_actor_location(), land.get_actor_scale3d()
    with open(OUT + 'grid.txt', 'w') as f:
        f.write('%d %d %d %d %d\n%f %f %f\n%f %f %f\n%s\n' % (grid[0], grid[1], grid[2], grid[3], grid[4], loc.x, loc.y,
                                                         loc.z, sc.x, sc.y, sc.z, ' '.join(names)))
    rows, blk = [], []
    for a in acts:
        o, e = a.get_actor_bounds(False)
        l, c = a.get_actor_label(), a.get_class().get_name()
        p = a.get_actor_location()
        rows.append('%s\t%s\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f' % (l, c, p.x, p.y, p.z, e.x, e.y))
        if c == 'BlockingVolume':
            blk.append('%s\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f' % (l, o.x, o.y, o.z, e.x, e.y, e.z))
    with open(OUT + 'actors.tsv', 'w', encoding='utf-8') as f:
        f.write('\n'.join(rows))
    with open(OUT + 'blockers.tsv', 'w', encoding='utf-8') as f:
        f.write('\n'.join(blk))
    unreal.log_warning('MapSkirt export %s ok=%s grid=%s' % (MAP, ok, grid))


main()
