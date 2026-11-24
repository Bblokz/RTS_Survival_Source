"""Survey Ger_IntroMission for the foliage + POI pass (exec'd in the throwaway editor through fol_cmd.py).

Writes into Saved/IntroFoliage/survey/:
  actors.tsv   label, class, folder, x, y, z, yaw, bounds-centre x/y, bounds half-extent x/y/z
  splines.json every actor with spline components (roads, two-tracks, rails, fences): sampled every 5 m
  heights.r16  full landscape height grid (RTSShortsLandscapeLibrary.export_landscape_region) + grid.txt
  top_*.png    ortho base-colour captures (image up = +X, right = +Y)
"""
import json
import os

import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/IntroFoliage/survey/'
os.makedirs(ROOT, exist_ok=True)


def _sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def dump_actors():
    rows = []
    for a in _sub().get_all_level_actors():
        try:
            loc = a.get_actor_location()
            rot = a.get_actor_rotation()
            origin, ext = a.get_actor_bounds(False)
            rows.append('\t'.join(str(v) for v in (
                a.get_actor_label(), a.get_class().get_name(), str(a.get_folder_path()),
                round(loc.x), round(loc.y), round(loc.z), round(rot.yaw, 1),
                round(origin.x), round(origin.y), round(ext.x), round(ext.y), round(ext.z))))
        except Exception as exc:  # noqa
            rows.append('ERR\t' + str(exc))
    with open(ROOT + 'actors.tsv', 'w', encoding='utf-8') as h:
        h.write('label\tclass\tfolder\tx\ty\tz\tyaw\tbx\tby\tex\tey\tez\n')
        h.write('\n'.join(rows))
    print('actors', len(rows))


def dump_splines(step=500.0):
    out = []
    for a in _sub().get_all_level_actors():
        comps = a.get_components_by_class(unreal.SplineComponent)
        for c in comps:
            length = c.get_spline_length()
            if length < 100:
                continue
            pts = []
            d = 0.0
            while d <= length:
                p = c.get_location_at_distance_along_spline(d, unreal.SplineCoordinateSpace.WORLD)
                pts.append((round(p.x), round(p.y), round(p.z)))
                d += step
            p = c.get_location_at_distance_along_spline(length, unreal.SplineCoordinateSpace.WORLD)
            pts.append((round(p.x), round(p.y), round(p.z)))
            out.append({'label': a.get_actor_label(), 'class': a.get_class().get_name(), 'comp': c.get_name(),
                        'length': round(length), 'closed': bool(c.is_closed_loop()), 'pts': pts})
    with open(ROOT + 'splines.json', 'w') as h:
        json.dump(out, h)
    print('splines', len(out))


def landscape():
    for a in _sub().get_all_level_actors():
        if isinstance(a, unreal.Landscape):
            return a
    raise RuntimeError('no landscape')


def dump_heights():
    ls = landscape()
    info = unreal.RTSShortsLandscapeLibrary.get_landscape_grid_info(ls)
    with open(ROOT + 'grid.txt', 'w') as h:
        h.write(info + '\n' + ls.get_actor_label() + ' ' + str(ls.get_actor_location()) + ' ' + str(ls.get_actor_scale3d()))
    parts = [int(p) for p in info.split()]
    print('grid', info)
    return parts


def export_full(rect):
    ok = unreal.RTSShortsLandscapeLibrary.export_landscape_region(landscape(), rect[0], rect[1], rect[2], rect[3],
                                                                  ROOT + 'heights.r16', [], [])
    print('export', rect, ok)


def top(name, cx, cy, width, size=2048, source=None):
    w = _world()
    rt = unreal.RenderingLibrary.create_render_target2d(w, size, size, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    cap = _sub().spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(cx, cy, 30000.0),
                                        unreal.Rotator(0.0, -90.0, 0.0))
    comp = cap.capture_component2d
    comp.set_editor_property('projection_type', unreal.CameraProjectionMode.ORTHOGRAPHIC)
    comp.set_editor_property('ortho_width', width)
    comp.set_editor_property('texture_target', rt)
    comp.set_editor_property('capture_source', source or unreal.SceneCaptureSource.SCS_BASE_COLOR)
    comp.set_editor_property('capture_every_frame', False)
    comp.capture_scene()
    comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(w, rt, ROOT, name + '.png')
    _sub().destroy_actor(cap)
    print('shot', ROOT + name + '.png')


SKIP_COMP = ('GeometryCollectionComponent', 'BillboardComponent', 'DecalComponent', 'SplineComponent', 'ArrowComponent',
             'TextRenderComponent', 'DrawFrustumComponent', 'NiagaraComponent', 'ParticleSystemComponent',
             'AudioComponent', 'BrushComponent', 'LandscapeComponent', 'LandscapeHeightfieldCollisionComponent',
             'WidgetComponent', 'SphereComponent', 'CapsuleComponent', 'BoxComponent')


def dump_footprints(path=ROOT + 'footprints.tsv'):
    """Per-actor footprint from visible primitive components (geometry collections, billboards, volumes skipped;
    instanced components one row per instance). Actor bounds are useless here: GC/child actors stretch them to the
    world origin."""
    rows = []
    for a in _sub().get_all_level_actors():
        label, cls = a.get_actor_label(), a.get_class().get_name()
        if cls in ('Landscape', 'LandscapeStreamingProxy') or label.startswith(('REAR_Skirt', 'REAR_Edge')):
            continue
        loc = a.get_actor_location()
        for c in a.get_components_by_class(unreal.PrimitiveComponent):
            cname = c.get_class().get_name()
            if cname in SKIP_COMP or not c.is_visible():
                continue
            coll = str(c.get_collision_enabled())
            if isinstance(c, unreal.InstancedStaticMeshComponent):
                mesh = c.get_editor_property('static_mesh')
                if not mesh:
                    continue
                mb = mesh.get_bounding_box()
                for k in range(c.get_instance_count()):
                    t = c.get_instance_transform(k, True)
                    s = t.scale3d
                    r = max(abs(mb.max.x - mb.min.x) * s.x, abs(mb.max.y - mb.min.y) * s.y) * 0.5
                    rows.append((label, cls, cname + '#inst', round(t.translation.x), round(t.translation.y), round(r),
                                 round(r), round(t.translation.z + mb.max.z * s.z), coll))
                continue
            o, e, _ = unreal.SystemLibrary.get_component_bounds(c)
            if e.x > 6000 or e.y > 6000:
                continue
            if abs(o.x - loc.x) > e.x + 3000 or abs(o.y - loc.y) > e.y + 3000:
                continue
            rows.append((label, cls, cname, round(o.x), round(o.y), round(e.x), round(e.y), round(o.z + e.z), coll))
    with open(path, 'w', encoding='utf-8') as h:
        h.write('label\tclass\tcomp\tcx\tcy\tex\tey\ttop\tcollision\n')
        h.write('\n'.join('\t'.join(str(v) for v in r) for r in rows))
    print('footprint rows', len(rows))


def depth_capture(name, cx=-5100, cy=300, width=72000, size=4096, only_landscape=False):
    """Ortho SceneDepth capture (R32F -> EXR). only_landscape: show-only the landscape; otherwise everything except the
    pass's own FOL_/POIW_/POIE_ actors. Covered ground = full depth < landscape depth (review r2: plants under the
    collision-less desert-mountain meshes)."""
    w = _world()
    rt = unreal.RenderingLibrary.create_render_target2d(w, size, size, unreal.TextureRenderTargetFormat.RTF_R32F)
    cap = _sub().spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(cx, cy, 30000.0),
                                        unreal.Rotator(0.0, -90.0, 0.0))
    comp = cap.capture_component2d
    comp.set_editor_property('projection_type', unreal.CameraProjectionMode.ORTHOGRAPHIC)
    comp.set_editor_property('ortho_width', width)
    comp.set_editor_property('texture_target', rt)
    comp.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_SCENE_DEPTH)
    comp.set_editor_property('capture_every_frame', False)
    actors = _sub().get_all_level_actors()
    if only_landscape:
        comp.set_editor_property('primitive_render_mode', unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
        for a in actors:
            if isinstance(a, unreal.Landscape) or a.get_class().get_name() == 'LandscapeStreamingProxy':
                comp.show_only_actor_components(a, False)
    else:
        for a in actors:
            if a.get_actor_label().startswith(('FOL_', 'POIW_', 'POIE_')):
                comp.hide_actor_components(a, False)
    comp.capture_scene()
    comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(w, rt, ROOT, name + '.exr')
    _sub().destroy_actor(cap)
    print('depth', ROOT + name + '.exr')


def cover_capture(name='cover', cx=-5100, cy=300, width=72000, size=4096):
    """Base-colour ortho capture showing ONLY meshes that cover the ground from above (desert mountains, cliff kits,
    rock formations) on a black background: non-black pixels = ground hidden from the RTS camera (review r2: 114
    crushables and ~820 cards were planted under the collision-less S_Desert_Mountain meshes)."""
    w = _world()
    rt = unreal.RenderingLibrary.create_render_target2d(w, size, size, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    cap = _sub().spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(cx, cy, 30000.0),
                                        unreal.Rotator(0.0, -90.0, 0.0))
    comp = cap.capture_component2d
    comp.set_editor_property('projection_type', unreal.CameraProjectionMode.ORTHOGRAPHIC)
    comp.set_editor_property('ortho_width', width)
    comp.set_editor_property('texture_target', rt)
    comp.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_BASE_COLOR)
    comp.set_editor_property('capture_every_frame', False)
    comp.set_editor_property('primitive_render_mode', unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
    n = 0
    for a in _sub().get_all_level_actors():
        label = a.get_actor_label()
        mesh_names = ''
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            m = c.get_editor_property('static_mesh')
            if m:
                mesh_names += m.get_name() + ' '
        if ('Mountain' in mesh_names or 'Canyon' in mesh_names or 'Cliff' in mesh_names or 'Mountain' in label
                or label.startswith(('REAR_Cliff', 'RIDGE_Cliff'))):
            comp.show_only_actor_components(a, False)
            n += 1
    comp.capture_scene()
    comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(w, rt, ROOT, name + '.png')
    _sub().destroy_actor(cap)
    print('cover capture actors', n, ROOT + name + '.png')
