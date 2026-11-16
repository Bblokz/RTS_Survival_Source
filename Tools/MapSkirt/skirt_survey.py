"""Read-only survey for adding horizon skirt planes to a map (commandlet or editor).

    env SKIRT_MAP = /Game/.../MapName      writes Saved/MapSkirt/<MapName>/survey.txt

Lists every landscape (location, scale, bounds, edit layers, material, layer infos), the map's blocking volumes, the
fog-of-war manager (its location + MapExtent = camera bounds), the nav bounds and every actor whose name hints at an
edge treatment already being there.
"""
import os
import unreal

MAP = os.environ.get('SKIRT_MAP', '/Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission')
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'MapSkirt', MAP.split('/')[-1])


def main():
    os.makedirs(OUT, exist_ok=True)
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    acts = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    lines = ['map %s actors %d' % (MAP, len(acts))]
    for a in acts:
        n = a.get_class().get_name()
        l = a.get_actor_label()
        o, e = a.get_actor_bounds(False)
        row = '%s | %s | loc (%.0f,%.0f,%.0f) | o (%.0f,%.0f,%.0f) e (%.0f,%.0f,%.0f) | scale %s' % (
            n, l, a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z, o.x, o.y, o.z,
            e.x, e.y, e.z, a.get_actor_scale3d().to_tuple())
        if isinstance(a, unreal.LandscapeProxy):
            lines.append('LANDSCAPE ' + row)
            for p in ('landscape_material', 'can_have_layers_content'):
                try:
                    lines.append('   %s = %s' % (p, a.get_editor_property(p)))
                except Exception as ex:  # noqa
                    lines.append('   %s ? %s' % (p, ex))
            try:
                tl = a.get_editor_property('target_layers')
                lines.append('   layers = %s' % [str(k) for k in tl.keys()])
            except Exception as ex:  # noqa
                lines.append('   layers ? %s' % ex)
            lines.append('   components %d' % len(a.get_components_by_class(unreal.LandscapeComponent)))
            try:
                lines.append('   grid %s' % unreal.RTSShortsLandscapeLibrary.get_landscape_grid_info(a))
            except Exception as ex:  # noqa
                lines.append('   grid ? %s' % ex)
        elif n in ('BlockingVolume', 'NavMeshBoundsVolume', 'RuntimeVirtualTextureVolume') or 'FogOfWar' in n:
            lines.append('VOL ' + row)
            if 'FogOfWar' in n:
                lines.append('   map_extent = %s' % a.get_editor_property('map_extent'))
        elif any(s in (n + l).lower() for s in ('skirt', 'horizon', 'edge', 'border', 'mountain', 'backdrop')):
            lines.append('EDGE? ' + row)
    with open(os.path.join(OUT, 'survey.txt'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    unreal.log_warning('MapSkirt survey written')


main()
