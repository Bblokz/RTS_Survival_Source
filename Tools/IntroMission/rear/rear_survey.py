"""Read-only survey of Ger_IntroMission for the rear-area work (commandlet or editor).

Writes Saved/IntroRear/survey.txt: landscape setup, map-wide volumes (nav, camera, fog of war, minimap), and every
actor east of X_MIN with class, label, location, bounds.
"""
import os
import unreal

MAP = '/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission'
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'IntroRear')
X_MIN = 2500.0


def main():
    os.makedirs(OUT, exist_ok=True)
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actors = unreal.EditorActorSubsystem().get_all_level_actors() if hasattr(unreal, 'EditorActorSubsystem') else []
    if not actors:
        actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    lines = ['world ' + world.get_path_name(), 'actors %d' % len(actors)]
    classes = {}
    for a in actors:
        classes[a.get_class().get_name()] = classes.get(a.get_class().get_name(), 0) + 1
    lines.append('--- class counts')
    for k, v in sorted(classes.items(), key=lambda kv: -kv[1]):
        lines.append('%5d %s' % (v, k))
    lines.append('--- landscapes / volumes / big things')
    for a in actors:
        cname = a.get_class().get_name()
        o, e = a.get_actor_bounds(False)
        big = e.x > 8000 or e.y > 8000
        key = any(s in cname for s in ('Landscape', 'Volume', 'Fog', 'Nav', 'Camera', 'MiniMap', 'Minimap',
                                        'Water', 'PostProcess', 'Sky', 'Light', 'Recast', 'Bounds', 'RuntimeVirtual'))
        if key or big:
            lines.append('%s | %s | loc %s | o (%.0f,%.0f,%.0f) e (%.0f,%.0f,%.0f) | scale %s' % (
                cname, a.get_actor_label(), a.get_actor_location(), o.x, o.y, o.z, e.x, e.y, e.z,
                a.get_actor_scale3d()))
        if isinstance(a, unreal.LandscapeProxy):
            for prop in ('component_size_quads', 'subsection_size_quads', 'num_subsections', 'landscape_material',
                         'can_have_layers_content', 'runtime_virtual_textures'):
                try:
                    lines.append('   %s = %s' % (prop, a.get_editor_property(prop)))
                except Exception as ex:  # noqa
                    lines.append('   %s ? %s' % (prop, ex))
            try:
                lines.append('   layers = %s' % [str(n) for n in a.get_target_layer_names()])
            except Exception as ex:  # noqa
                lines.append('   layers ? %s' % ex)
            comps = a.get_components_by_class(unreal.LandscapeComponent)
            lines.append('   components %d' % len(comps))
    lines.append('--- actors with X > %d' % X_MIN)
    rows = []
    for a in actors:
        loc = a.get_actor_location()
        if loc.x < X_MIN:
            continue
        o, e = a.get_actor_bounds(False)
        rows.append((loc.x, '%s | %s | (%.0f,%.0f,%.0f) yaw %.0f | ext (%.0f,%.0f,%.0f) | folder %s' % (
            a.get_class().get_name(), a.get_actor_label(), loc.x, loc.y, loc.z, a.get_actor_rotation().yaw,
            e.x, e.y, e.z, a.get_folder_path())))
    for _, r in sorted(rows):
        lines.append(r)
    lines.append('--- splines anywhere')
    for a in actors:
        if 'Spline' in a.get_class().get_name() or 'GroundPath' in a.get_class().get_name():
            comps = a.get_components_by_class(unreal.SplineComponent)
            for c in comps:
                n = c.get_number_of_spline_points()
                pts = [c.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD) for i in range(n)]
                lines.append('%s | %s | %d pts len %.0f | %s' % (
                    a.get_class().get_name(), a.get_actor_label(), n, c.get_spline_length(),
                    ' '.join('(%.0f,%.0f,%.0f)' % (p.x, p.y, p.z) for p in pts)))
    with open(os.path.join(OUT, 'survey.txt'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    unreal.log_warning('IntroRear survey written')


main()
