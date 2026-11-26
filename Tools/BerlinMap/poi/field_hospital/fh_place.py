"""Editor Python for Ger_Berlin (a FULL editor with the map open; run the code with execute_python, never `py file`):
place the field hospital POI (owner's spot B, south-central field, 2026-11-26) and check it. Re-runnable: it removes its own FH_ actors first.

    exec(open(r'...\\fh_place.py').read()); check()     read-only: what stands in the footprint, ground heights
    exec(open(r'...\\fh_place.py').read()); place()     spawn FH_Shell, FH_Hospital, FH_Flag (folder Berlin/FieldHospital)
    exec(open(r'...\\fh_place.py').read()); nav()       after a nav rebuild: door / rally points on the navmesh, paths

Layout (BerlinMissionScripting.md section 13): SM_BerlinRuin_E5_Shell1St_A with its shop front (+X local) turned to
face south (yaw 180), so the door faces the RTS camera; BP_BerlinFieldHospital on the same pivot (dark cellar door,
sandbags, cots, Red Cross sheet); Dest_RedCrossFlag beside the door. Nothing is saved: save the level yourself.
"""
import json
import math

import unreal

CENTRE = unreal.Vector(-16500.0, -5800.0, 0.0)   # owner pick B "between craters" (2nd round), shifted 3 m west off the B2 facade
YAW = 180.0
FOLDER = 'Berlin/FieldHospital'
FH = '/Game/RTS_Survival/Blueprints/Environment/Berlin/FieldHospital/'
PIECES = [  # label, class path, local offset
    ('FH_Shell', '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Blueprints/BP_BerlinRuin_E5_Shell1St_A',
     (0, 0, 0)),
    ('FH_Hospital', FH + 'BP_BerlinFieldHospital', (0, 0, 0)),
    ('FH_Flag', FH + 'Dest_RedCrossFlag', (700, -30, 0)),
]
# Local points that matter (same numbers as Mission_Berlin_FieldHospital's offsets).
POINTS = {'door': (640, 210), 'rally1': (1150, 40), 'rally2': (1150, 380)}
FOOTPRINT = (-560, 1350, -720, 760)   # local x0, x1, y0, y1 of shell + dressing + rally lane
HQ = unreal.Vector(13900, 43300, 0)

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def to_world(x, y, z=0.0):
    r = math.radians(YAW)
    return unreal.Vector(CENTRE.x + x * math.cos(r) - y * math.sin(r), CENTRE.y + x * math.sin(r) + y * math.cos(r), z)


def ground(x, y):
    """Landscape height (traces ignore everything but the landscape)."""
    land = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.Landscape)
            or a.get_class().get_name() in ('Landscape', 'LandscapeStreamingProxy')]
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 60000), unreal.Vector(x, y, -60000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True,
                                                 [a for a in actors.get_all_level_actors() if a not in land],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else None


def footprint_corners():
    x0, x1, y0, y1 = FOOTPRINT
    return [to_world(x, y) for x, y in ((x0, y0), (x0, y1), (x1, y0), (x1, y1), (0, 0), (640, 210))]


def check():
    """Actors with their pivot inside the footprint (+3 m), and the ground heights at its corners."""
    x0, x1, y0, y1 = FOOTPRINT
    r = math.radians(-YAW)
    inside = []
    for a in actors.get_all_level_actors():
        p = a.get_actor_location()
        dx, dy = p.x - CENTRE.x, p.y - CENTRE.y
        lx, ly = dx * math.cos(r) - dy * math.sin(r), dx * math.sin(r) + dy * math.cos(r)
        if x0 - 300 < lx < x1 + 300 and y0 - 300 < ly < y1 + 300 and not a.get_actor_label().startswith('FH_'):
            inside.append((a.get_actor_label(), a.get_class().get_name(), round(lx), round(ly)))
    heights = [round(ground(c.x, c.y) or -99999) for c in footprint_corners()]
    out = {'inside': inside, 'ground': heights}
    print(json.dumps(out, indent=1))
    return out


def place():
    for a in actors.get_all_level_actors():
        if a.get_actor_label().startswith('FH_'):
            actors.destroy_actor(a)
    zs = sorted(z for z in (ground(c.x, c.y) for c in footprint_corners()) if z is not None)
    z = zs[len(zs) // 2]          # median: the kit's walls reach 35 cm below z = 0, so small dips are covered
    rot = unreal.Rotator(0, 0, YAW)
    for label, path, (lx, ly, lz) in PIECES:
        loc = to_world(lx, ly, z + lz)
        if label == 'FH_Flag':
            loc.z = ground(loc.x, loc.y)
        a = actors.spawn_actor_from_class(unreal.load_asset(path).generated_class(), loc, rot)
        a.set_actor_label(label)
        a.set_folder_path(FOLDER)
    print('placed at z', z, 'from', zs)


def nav():
    """Door and rally points projected onto the navmesh, and path lengths door -> rally and HQ -> rally."""
    out = {}
    for name, (x, y) in POINTS.items():
        flat = to_world(x, y)
        p = to_world(x, y, ground(flat.x, flat.y) or 0)
        # Python returns the out-param location (or a (location, ok) tuple, depending on the binding).
        proj = unreal.NavigationSystemV1.project_point_to_navigation(world, p, None, None, unreal.Vector(50, 50, 250))
        loc, ok = (proj if isinstance(proj, tuple) else (proj, proj is not None))
        out[name] = {'world': [round(p.x), round(p.y), round(p.z)],
                     'projected': [round(v) for v in (loc.x, loc.y, loc.z)] if ok and loc else None}
    door = to_world(*POINTS['door'])
    for name in ('rally1', 'rally2'):
        for start_name, start in (('door', door), ('hq', HQ)):
            path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, start, to_world(*POINTS[name]))
            pts = path.get_editor_property('path_points') if path else []
            length = sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1)) if pts else None
            out[f'{start_name}->{name}'] = {'points': len(pts), 'length_m': round(length / 100, 1) if length else None,
                                            'partial': path.is_partial() if path else None}
    print(json.dumps(out, indent=1))
    return out
