"""In-editor check of M_LandscapeImpactDecal: spawns 16 DecalActors (one per atlas cell) plus a slope-fade probe on a
landscape, frames the viewport camera, and leaves them for a screenshot. Never saves the level. Clean up with
    TEST_ACTION = 'cleanup'; exec(open(<this file>).read())

Run (UECP execute_python):
    TEST_ACTION = 'spawn'; TEST_CENTER = (x, y); exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/ImpactDecals/ue_test_decals.py').read())
    TEST_ACTION = 'compare'   # bomb K0..K2 next to shell B6A / B7A / B7B in a row at the runtime box sizes
"""
import math
import unreal

MAT = '/Game/RTS_Survival/Materials/Decals/LandscapeImpacts/MI_LandscapeImpactDecal'
BOMB_MAT = '/Game/RTS_Survival/Materials/Decals/LandscapeImpacts/MI_BombImpactDecal'
TAG = 'ImpactDecalTest'
DIAM = [40, 70, 110, 240, 330, 450, 600, 850]   # shell boxes since 2026-11-09 (B3-B7 enlarged)
DEPTH = [60, 60, 70, 100, 120, 140, 180, 240]   # decal box depth (full), cm
BOMB_DIAM = [900, 1200, 1600, 2200]             # bomb bands K0..K3 (cells 4k..4k+3)
BOMB_DEPTH = [250, 300, 350, 450]

try:
    TEST_ACTION
except NameError:
    TEST_ACTION = 'spawn'
try:
    TEST_CENTER
except NameError:
    TEST_CENTER = (80000.0, 0.0)
try:
    TEST_CAM
except NameError:
    TEST_CAM = None

EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = UES.get_editor_world()


def ground_z(x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 50000), unreal.Vector(x, y, -50000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    blocking = t[0]
    loc = t[4]
    return loc.z if blocking else None


def cleanup():
    n = 0
    for a in EAS.get_all_level_actors():
        if TAG in [str(t) for t in a.tags]:
            a.destroy_actor()
            n += 1
    print('removed', n)


def spawn_decal(x, y, cell, diam, depth, yaw, g=0.5, label=None, pitch=-90.0, mat=MAT):
    z = ground_z(x, y)
    if z is None:
        print('no ground at', x, y)
        return None
    d = EAS.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(x, y, z), unreal.Rotator(0, pitch, yaw))
    comp = d.decal
    comp.set_decal_material(unreal.load_asset(mat))
    comp.set_editor_property('decal_size', unreal.Vector(depth * 0.5, diam * 0.5, diam * 0.5))
    comp.set_editor_property('fade_screen_size', 0.0)
    comp.set_decal_color(unreal.LinearColor((cell + 0.5) / 16.0, g, 0.0, 1.0))
    d.tags = [TAG]
    d.set_actor_label(label or ('ImpactDecalTest_%02d' % cell))
    return d


def spawn():
    cleanup()
    cx, cy = TEST_CENTER
    # 4 columns x 4 rows, spacing grows with size; row = band pair.
    xs = [-1300, -500, 500, 1300]
    k = 0
    for row in range(4):
        for col in range(4):
            cell = row * 4 + col
            b = cell // 2
            x = cx + xs[col]
            y = cy + (row - 1.5) * 800
            spawn_decal(x, y, cell, DIAM[b], DEPTH[b], 0.0)
            k += 1
    print('spawned', k)


def compare():
    cleanup()
    cx, cy = TEST_CENTER
    row = [(MAT, 12, DIAM[6], DEPTH[6]), (MAT, 14, DIAM[7], DEPTH[7]), (MAT, 15, DIAM[7], DEPTH[7]),
           (BOMB_MAT, 0, BOMB_DIAM[0], BOMB_DEPTH[0]), (BOMB_MAT, 5, BOMB_DIAM[1], BOMB_DEPTH[1]),
           (BOMB_MAT, 10, BOMB_DIAM[2], BOMB_DEPTH[2])]
    x = cx - sum(d for _, _, d, _ in row) * 0.55
    for i, (mat, cell, d, dep) in enumerate(row):
        x += d * 0.55
        spawn_decal(x, cy, cell, d, dep, 30.0 * i, 0.5, 'ImpactDecalCompare_%d' % i, mat=mat)
        x += d * 0.55
    print('spawned compare row')


if TEST_ACTION == 'compare':
    compare()
elif TEST_ACTION == 'cleanup':
    cleanup()
elif TEST_ACTION == 'spawn':
    spawn()
