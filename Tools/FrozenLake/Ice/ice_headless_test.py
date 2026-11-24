"""Throwaway-editor run for the Frozen Lake ice shaders. Never run in the shared editor; never saves a map.

  bash run_ice_test.sh            (launches UnrealEditor-Win64-DebugGame.exe on RTS_ToCopySmall with this script)

1. Builds / updates the ice assets in place (build_ice_material.build, never deletes), saving only them.
2. In the (unsaved) editor world: a 586 m test-lake plane with MI_FrozenLakeIce_TestLake (child of the Masked
   MI_FrozenLakeIce_Lake; state mask: types side by side, heavy road, vein reveal, melt front, warn glow, shell holes),
   the translucent water plane 20 cm under it, separate planes with the type MIs plus the black-ice bracket,
   tanks (training-preview meshes, one resting on the lakebed and one fully under water in the melt hole) and
   soldiers for scale, one CameraActor.
3. PIE, fog-of-war post process removed, game-camera pictures (40 m up, pitch -55, FOV 90) into Saved/FrozenLake/ice/.
4. Shader statistics after PIE (shaders compiled by then), report json, quit.
"""
import json
import math
import os
import sys

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/FrozenLake/Ice')
import build_ice_material as B  # noqa: E402

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/FrozenLake/ice/'
os.makedirs(ROOT, exist_ok=True)
TAG = 'FrozenIceTest'
LAKE = 58600.0
PLANE = '/Engine/BasicShapes/Plane'
TIGER = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/Tiger/SM_Tiger1_TrainingPreview'
KTIGER = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/TigerII/SM_KingTiger_TrainingPreview'
PZ4 = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/SM_PZIV_H_TrainingPreview'
PZ38 = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/Panzer38T/SM_PZ38T_TrainingPreview'
SOLDIER = '/Game/RTS_Survival/Blueprints/Squads/Ger/EliteInfantry/RailTroopers/Mesh/SK_RailTrooper'
TANK_PAINT = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/Tiger/Meshes/MI_Tiger1_PostApo_HullForest'
ICE = '/Game/RTS_Survival/Blueprints/Environment/FrozenLake/Ice'
# Station D strips (21 m wide, y = 200 + 21 k): heavy, medium, black (default), black darker, black lighter, vein
TYPES = [ICE + '/MI_FrozenLakeIce_Heavy', ICE + '/MI_FrozenLakeIce_Medium', ICE + '/MI_FrozenLakeIce_Infantry',
         ICE + '/Test/MI_FrozenLakeIce_Black_Darker', ICE + '/Test/MI_FrozenLakeIce_Black_Lighter',
         ICE + '/MI_FrozenLakeIce_Infantry_Vein']
WATER_MI = ICE + '/MI_FrozenLakeWater'
# ICE_TEST_ON_MAP=1 (run_ice_test.sh map): the editor starts on Ger_FrozenLake (never saved). The test lake then floats
# 300 m up and 700 m north of the real lake, casting no shadow, and the real lake gets units and shots of its own.
ON_MAP = os.environ.get('ICE_TEST_ON_MAP') == '1'
# ICE_TEST_TRACKS=1 (run_ice_test.sh tracks, implies ON_MAP): only the vehicle-track check. In PIE the trail subsystem
# is switched off (rts.TankTrails 0), and ruts are painted by hand into ITS render target, in its own mapping (no resize:
# resizing RT_TankTrails mid-PIE left the material bound to the old resource and nothing showed), inside a 164 m square
# where heavy, medium and black ice meet. MPC_TankTrails gets the subsystem's values back with TrailEnabled 1.
TRACKS = os.environ.get('ICE_TEST_TRACKS') == '1'
PREFIX = 'TR_' if TRACKS else ('FL_' if ON_MAP else '')
TRACK_CENTRE = (2122.0, -396.0)   # cm; spots inside: heavy (60, -1553), medium (-644, -3766), black (3078, -547)
TRACK_PX = 1024
TRACK_TEXEL_CM = 16.0
TRAILS_RT = '/Game/RTS_Survival/Blueprints/Environment/TankTrails/RT_TankTrails'
TRAILS_MPC = '/Game/RTS_Survival/Blueprints/Environment/TankTrails/MPC_TankTrails'
REAL_LAKE_CENTRE = (2550.0, 3250.0)  # Ger_FrozenLake: LakeOrigin (-23200, -22500), LakeSize 51500
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'shots': []}


def log(message):
    S['log'].append(str(message))
    unreal.log('FROZENICE ' + str(message))


def finish():
    S['phase'] = 'done'
    json.dump({'log': S['log'], 'build': B.report}, open(ROOT + 'test_result.json', 'w'), indent=1)
    unreal.SystemLibrary.quit_editor()


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def trace_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 200000), unreal.Vector(x, y, -200000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else None


# ------------------------------------------------------------------ layout (lake metres -> world cm)
def L(x_m, y_m, dz=0.0):
    return unreal.Vector(S['ox'] + x_m * 100.0, S['oy'] + y_m * 100.0, S['z'] + dz)


def spawn_sm(mesh_path, loc, yaw=0.0, scale=(1, 1, 1), material=None, label=None, seat=True, shadow=True):
    mesh = unreal.load_asset(mesh_path)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, loc, unreal.Rotator(0, 0, yaw))
    comp = actor.static_mesh_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(mesh)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    if not shadow:
        comp.set_cast_shadow(False)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    if material is not None:
        comp.set_material(0, unreal.load_asset(material))
    elif mesh_path != PLANE:
        # training-preview meshes carry a teal preview material; paint them like a tank
        paint = unreal.load_asset(TANK_PAINT)
        for i in range(comp.get_num_materials()):
            comp.set_material(i, paint)
    if seat:
        # seat by the mesh's own geometry box (not actor bounds: actor-bounds-seating-trap)
        box = mesh.get_bounding_box()
        bottom = box.min.z * scale[2]
        actor.set_actor_location(unreal.Vector(loc.x, loc.y, loc.z - bottom), False, True)
    actor.tags = [TAG]
    if label:
        actor.set_actor_label(label)
    return actor


def spawn_soldier(loc, yaw):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, loc, unreal.Rotator(0, 0, yaw))
    comp = actor.skeletal_mesh_component
    comp.set_skeletal_mesh_asset(unreal.load_asset(SOLDIER))
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.tags = [TAG]
    return actor


def squad(cx, cy, n=6, spread=2.2, yaw=0.0, at=None):
    at = at or L
    for i in range(n):
        a = i * 2.39996
        r = spread * math.sqrt(i + 0.5)
        spawn_soldier(at(cx + r * math.cos(a), cy + r * math.sin(a)), yaw + (i * 37) % 60 - 30)


def W(x_m, y_m, dz=0.0):
    """Real lake (Ger_FrozenLake): world metres, ice surface z 0."""
    return unreal.Vector(x_m * 100.0, y_m * 100.0, dz)


def build_scene():
    w = editor_world()
    # Lake placement: centred on the map origin, plane above everything under it (traced every 20 m).
    S['ox'], S['oy'] = -LAKE / 2.0, -LAKE / 2.0
    if ON_MAP:
        S['ox'], S['oy'] = REAL_LAKE_CENTRE[0] + 70000.0 - LAKE / 2.0, REAL_LAKE_CENTRE[1] - LAKE / 2.0
    zs = []
    for i in range(0, 0 if ON_MAP else 31):
        for j in range(0, 31):
            z = trace_z(w, S['ox'] + i * LAKE / 30.0, S['oy'] + j * LAKE / 30.0)
            if z is not None:
                zs.append(z)
    zs.sort()
    S['z'] = 30000.0 if ON_MAP else (zs[-1] if zs else 0.0) + 150.0
    log('ground under lake: n=%d min=%.0f p50=%.0f max=%.0f -> ice plane z=%.0f'
        % (len(zs), zs[0] if zs else 0, zs[len(zs) // 2] if zs else 0, zs[-1] if zs else 0, S['z']))
    # lakebed depth under the hazard station, traced before any test plane exists
    for nm, (bx, by) in () if ON_MAP else (('melt_centre', (250, 420)), ('melt_tank', (256, 418)), ('melt_sub', (243, 428)),
                         ('hole_205_395', (205, 395)), ('hole_190_430', (190, 430))):
        gz = trace_z(w, S['ox'] + bx * 100.0, S['oy'] + by * 100.0)
        S['bed_' + nm] = gz
        log('lakebed %s: ground z=%s -> %.0f cm under the ice' % (nm, gz, S['z'] - gz if gz is not None else -1))
    mat, mis = B.build(test_origin=(S['ox'], S['oy']))
    log('built %s (%s); failures=%s' % (mat.get_path_name(), list(mis), B.failures))
    if TRACKS:
        build_track_scene()
        return

    # the test lake plane (100 cm basic plane, scaled), pivot at the plane centre
    spawn_sm(PLANE, L(293, 293), scale=(LAKE / 100.0, LAKE / 100.0, 1.0),
             material=B.TEST + '/MI_FrozenLakeIce_TestLake', label='IceTestLake', seat=False, shadow=not ON_MAP)
    # open water under the ice (seen through the Masked holes); low sort priority so steam VFX draws over it
    water = spawn_sm(PLANE, L(293, 293, -20.0), scale=(LAKE / 100.0, LAKE / 100.0, 1.0), material=WATER_MI,
                     label='IceTestWater', seat=False, shadow=False)
    water.static_mesh_component.set_editor_property('translucency_sort_priority', -10)
    # Station A: mask-driven types side by side (heavy y<136, medium 136-164, black 164-210)
    spawn_sm(TIGER, L(155, 118), yaw=-80)
    spawn_sm(KTIGER, L(138, 104), yaw=-95)
    spawn_sm(PZ4, L(152, 150), yaw=-88)
    spawn_sm(PZ38, L(150, 182), yaw=-92)
    squad(146, 196, n=8, yaw=90)
    squad(161, 172, n=5, yaw=60)

    # Station D: SEPARATE planes with the type MIs, 21 m x 70 m each, from (520, 200), 3 cm above the lake
    for k, mi in enumerate(TYPES):
        spawn_sm(PLANE, L(520, 200 + k * 21, 3.0), scale=(70.0, 21.0, 1.0), material=mi,
                 label='IceTypePlane_%d' % k, seat=False, shadow=not ON_MAP)
    spawn_sm(TIGER, L(522, 200, 3.0), yaw=-90)
    spawn_sm(PZ4, L(520, 221, 3.0), yaw=-90)
    for k in (2, 3, 4):  # every black-ice strip: a squad and the light tank (readability bracket)
        spawn_sm(PZ38, L(530, 200 + k * 21, 3.0), yaw=-90)
        squad(512, 200 + k * 21, n=6, yaw=90)
    squad(516, 305, n=6, yaw=90)

    # Station B: reveal, black ice over the vein at (380, 300)
    squad(352, 292, n=6, yaw=0)
    spawn_sm(PZ38, L(345, 312), yaw=-10)

    # Station C: hazards at (250, 420): melt front radius 22 m, warn patch (250, 345), shell holes
    # sunk hulls in the melt hole: a Pz IV resting on the lakebed, a Pz 38(t) fully under the water plane
    bed = S.get('bed_melt_tank')
    spawn_sm(PZ4, L(256, 418, (bed - S['z']) if bed is not None else -170.0), yaw=30, seat=True)
    spawn_sm(PZ38, L(243, 428, -260.0), yaw=-60, seat=True)
    spawn_sm(TIGER, L(248, 346), yaw=-90)                          # on heavy ice that is about to open
    spawn_sm(PZ4, L(232, 452), yaw=120)                            # outrunning the front
    squad(206, 405, n=6, yaw=180)
    squad(266, 383, n=4, yaw=180)

    if ON_MAP:
        # units on the real lake: a squad on the black vein ice, a Pz 38(t) on medium, a Tiger on heavy
        squad(50.4, 1.6, n=8, yaw=90, at=W)
        spawn_sm(PZ38, W(-55.0, -66.0), yaw=30)
        spawn_sm(TIGER, W(185.0, 48.0), yaw=-60)
        spawn_sm(PZ4, W(73.0, -45.0), yaw=10)

    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cam = actors.spawn_actor_from_class(unreal.CameraActor, L(0, 0, 4000))
    cam.tags = [TAG, 'IceCam']
    cam.camera_component.set_editor_property('field_of_view', 90.0)
    cam.camera_component.set_editor_property('constrain_aspect_ratio', False)

    rts_h, pitch = 4000.0, 55.0
    back = rts_h / math.tan(math.radians(pitch))

    def rts(name, tx, ty, h=rts_h, p=pitch, yaw_deg=0.0, real=False):
        bk = h / math.tan(math.radians(p))
        dx, dy = math.cos(math.radians(yaw_deg)), math.sin(math.radians(yaw_deg))
        target = L(tx, ty) if not real else W(tx, ty)
        eye = unreal.Vector(target.x - dx * bk, target.y - dy * bk, target.z + h)
        S['shots'].append((PREFIX + name, eye, target))

    if ON_MAP:
        # the real lake in the map's winter light (points sampled from T_FL_IceState)
        rts('R_overview', 25.5, 32.5, h=32000.0, p=68.0, real=True)
        rts('R_vein_black_rts', 50.4, 1.6, real=True)
        rts('R_border_heavy_rts', 73.0, -39.7, real=True)
        rts('R_border_black_rts', -59.2, -60.8, real=True)
        rts('R_lead_rts', -153.8, 23.7, real=True)
        rts('R_heavy_rts', 185.2, 48.3, real=True)
        rts('R_mid_90m', 25.5, 10.0, h=9000.0, p=58.0, real=True)
    rts('A_types_mask_rts', 150, 155)
    rts('A_borders_close', 150, 160, h=2000.0, p=50.0)
    if not ON_MAP:
        rts('D_types_separate_planes_rts', 520, 232)
    rts('E_black_bracket_rts', 520, 263)
    rts('B_reveal_rts', 375, 300)
    if not ON_MAP:
        rts('B_reveal_low', 372, 300, h=1600.0, p=32.0)
    rts('C_hazards_rts', 238, 410)
    rts('C_hazards_rts_b', 238, 410)  # same view ~3 s later (pulses)
    if not ON_MAP:
        rts('C_hole_low', 240, 418, h=1400.0, p=40.0)
    rts('C_warn_heavy_rts', 250, 345)
    rts('W_mid_90m', 300, 260, h=9000.0, p=58.0)
    if not ON_MAP:
        rts('W_wide_tiling', 293, 293, h=30000.0, p=62.0)
    S['back'] = back


def build_track_scene():
    """Units for scale next to the painted tracks, the camera and the track shots (real lake, z 0)."""
    cx, cy = TRACK_CENTRE
    spawn_sm(TIGER, unreal.Vector(60.0 + 900.0, -1553.0, 0.0), yaw=90)
    spawn_sm(PZ38, unreal.Vector(-644.0 + 700.0, -3766.0 - 300.0, 0.0), yaw=90)
    squad(30.78 + 4.0, -5.47 + 3.0, n=6, yaw=0, at=W)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cam = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(cx, cy, 4000))
    cam.tags = [TAG, 'IceCam']
    cam.camera_component.set_editor_property('field_of_view', 90.0)
    cam.camera_component.set_editor_property('constrain_aspect_ratio', False)

    def shot(name, tx, ty, h, p):
        bk = h / math.tan(math.radians(p))
        eye = unreal.Vector(tx - bk, ty, h)
        S['shots'].append((PREFIX + name, eye, unreal.Vector(tx, ty, 0.0)))

    shot('tracks_rts', cx, cy, 4000.0, 55.0)
    shot('heavy_close', 60.0, -1553.0, 1800.0, 50.0)
    shot('medium_close', -644.0, -3766.0, 1800.0, 50.0)
    shot('black_close', 3078.0, -547.0, 1800.0, 50.0)
    shot('tracks_low', cx - 500.0, cy, 900.0, 28.0)
    shot('tracks_rts_off', cx, cy, 4000.0, 55.0)   # same view with TrailEnabled 0 (before/after)


def paint_tracks(world):
    """Hand-painted ruts in RT_TankTrails: twin 60 cm ruts 2.6 m apart (4 px lines, 16 px apart), five tanks in curves
    plus one crossing, one tank half faded. MPC_TankTrails points at the 164 m square."""
    unreal.SystemLibrary.execute_console_command(world, 'rts.TankTrails 0')
    rt = unreal.load_asset(TRAILS_RT)
    canvas, size, ctx = unreal.RenderingLibrary.begin_draw_canvas_to_render_target(world, rt)
    # the subsystem's mapping on Ger_FrozenLake: landscape bounds -37800..37800 cm at 16 cm per texel (4725 px)
    rt_px = int(size.x)
    rt_min, rt_size_cm = -37800.0, 75600.0
    size_cm = TRACK_PX * TRACK_TEXEL_CM
    off_x = (TRACK_CENTRE[0] - size_cm / 2 - rt_min) / rt_size_cm * rt_px
    off_y = (TRACK_CENTRE[1] - size_cm / 2 - rt_min) / rt_size_cm * rt_px
    scale = (TRACK_TEXEL_CM / (rt_size_cm / rt_px))
    tanks = []
    for k in range(5):  # roughly north-south (along +X = image u... drawn as image columns) curves
        tanks.append(([(60 + t, 150 + k * 170 + 70 * math.sin(t / 110.0 + k)) for t in range(0, TRACK_PX - 120, 6)],
                      0.5 if k == 3 else 1.0))
    tanks.append(([(t, 120 + t * 0.75 + 40 * math.sin(t / 70.0)) for t in range(100, TRACK_PX - 100, 6)], 1.0))
    for pts, value in tanks:
        colour = unreal.LinearColor(value, value, value, 1)
        for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
            dx, dy = bx - ax, by - ay
            ln = max(math.hypot(dx, dy), 1e-3)
            nx, ny = -dy / ln, dx / ln
            for side in (-8.0, 8.0):
                for off in (-1.5, -0.5, 0.5, 1.5):
                    o = side + off
                    canvas.draw_line(unreal.Vector2D(off_x + (ax + nx * o) * scale, off_y + (ay + ny * o) * scale),
                                     unreal.Vector2D(off_x + (bx + nx * o) * scale, off_y + (by + ny * o) * scale),
                                     1.5, colour)
    unreal.RenderingLibrary.end_draw_canvas_to_render_target(world, ctx)
    mn = unreal.LinearColor(rt_min, rt_min, 0, 0)
    inv = unreal.LinearColor(1.0 / rt_size_cm, 1.0 / rt_size_cm, 1.0 / rt_px, 1.0 / rt_px)
    log('trail RT %s px, painted at offset (%.0f, %.0f) px, scale %.3f' % (rt_px, off_x, off_y, scale))
    S['track_mpc'] = (mn, inv)
    apply_track_mpc(world, True)
    log('tracks painted: %d tanks, square centre (%.0f, %.0f) size %.0f cm' % (len(tanks), TRACK_CENTRE[0],
                                                                                  TRACK_CENTRE[1], size_cm))


def apply_track_mpc(world, enabled):
    """Re-applied before every shot: the subsystem applies rts.TankTrails 0 on its next tick (TrailEnabled 0), after
    paint_tracks ran in the same frame."""
    mpc = unreal.load_asset(TRAILS_MPC)
    mn, inv = S['track_mpc']
    unreal.MaterialLibrary.set_vector_parameter_value(world, mpc, 'TrailBoundsMin', mn)
    unreal.MaterialLibrary.set_vector_parameter_value(world, mpc, 'TrailBoundsInvSize', inv)
    unreal.MaterialLibrary.set_scalar_parameter_value(world, mpc, 'TrailEnabled', 1.0 if enabled else 0.0)


def strip_fog_of_war(world):
    removed = []
    for ppv in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PostProcessVolume):
        settings = ppv.get_editor_property('settings')
        wb = settings.get_editor_property('weighted_blendables')
        arr = list(wb.get_editor_property('array'))
        names = [b.get_editor_property('object').get_path_name() if b.get_editor_property('object') else 'None'
                 for b in arr]
        keep = [b for b, nm in zip(arr, names) if 'FogOfWar' not in nm and 'FOW' not in nm.upper().split('/')[-1]]
        log('ppv %s unbound=%s blendables=%s' % (ppv.get_name(), ppv.get_editor_property('unbound'), names))
        if len(keep) != len(arr):
            wb.set_editor_property('array', keep)
            settings.set_editor_property('weighted_blendables', wb)
            ppv.set_editor_property('settings', settings)
            removed.append(ppv.get_name())
    log('fog-of-war blendables removed from %s' % removed)


def tick(delta):
    S['t'] += delta
    if S.get('busy'):
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > (45.0 if ON_MAP else 20.0):
            log('editor map %s' % editor_world().get_path_name())
            S['busy'] = True
            try:
                build_scene()
            finally:
                S['busy'] = False
            S['phase'], S['mark'] = 'compile', S['t']
        elif phase == 'compile' and S['t'] - S['mark'] > 100.0:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 15.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                strip_fog_of_war(world)
                cams = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
                        if 'IceCam' in [str(t) for t in a.tags]]
                if not cams:
                    log('IceCam not found in PIE world')
                    finish()
                    return
                S['camera'] = cams[0]
                S['camera'].root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
                unreal.SystemLibrary.execute_console_command(world, 'r.MotionBlurQuality 0')
                # Fog of war is a post-process material outside the PPV/pawn camera; in seen areas it does not darken,
                # so the shots turn post-process materials off to show the look a player has in vision.
                unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.PostProcessMaterial 0')
                for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
                    for ppc in a.get_components_by_class(unreal.PostProcessComponent):
                        arr = ppc.get_editor_property('settings').get_editor_property(
                            'weighted_blendables').get_editor_property('array')
                        log('ppc %s.%s blendables=%s' % (a.get_name(), ppc.get_name(), [
                            b.get_editor_property('object').get_path_name() if b.get_editor_property('object') else None
                            for b in arr]))
                pc = unreal.GameplayStatics.get_player_controller(world, 0)
                S['pc'] = pc
                if TRACKS:
                    paint_tracks(world)
                pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
                if pawn:
                    for cc in pawn.get_components_by_class(unreal.CameraComponent):
                        pps = cc.get_editor_property('post_process_settings')
                        arr = pps.get_editor_property('weighted_blendables').get_editor_property('array')
                        log('pawn camera %s blendables=%s weight=%s' % (cc.get_name(), [
                            b.get_editor_property('object').get_path_name() if b.get_editor_property('object') else None
                            for b in arr], cc.get_editor_property('post_process_blend_weight')))
                S['phase'], S['mark'], S['first'] = 'aim', S['t'], True
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'aim':
            if not S['shots']:
                S['phase'], S['mark'] = 'stats', S['t']
                return
            name, eye, target = S['shots'][0]
            if TRACKS:
                apply_track_mpc(game_world(), not name.endswith('_off'))
            S['camera'].set_actor_location(eye, False, True)
            S['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), True)
            S['pc'].set_view_target_with_blend(S['camera'], 0.0)
            S['phase'], S['mark'] = 'shoot', S['t']
        elif phase == 'shoot' and S['t'] - S['mark'] > (12.0 if S.get('first') else 4.0):
            S['first'] = False
            name, eye, target = S['shots'].pop(0)
            vt = S['pc'].get_view_target()
            S['busy'] = True
            try:
                unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, ROOT + name + '.png', camera=S['camera'])
            finally:
                S['busy'] = False
            rot = unreal.MathLibrary.find_look_at_rotation(eye, target)
            log('shot %s eye=(%.0f,%.0f,%.0f) pitch=%.1f viewtarget=%s cam_at=%s' % (
                name, eye.x, eye.y, eye.z, rot.pitch, vt.get_name() if vt else None, S['camera'].get_actor_location()))
            S['phase'], S['mark'] = 'post', S['t']
        elif phase == 'post' and S['t'] - S['mark'] > 2.5:
            S['phase'], S['mark'] = 'aim', S['t']
        elif phase == 'stats' and S['t'] - S['mark'] > 4.0:
            mat = unreal.load_asset(B.MASTER)
            B.report['stats_master_after_pie'] = B.stats(mat)
            for nm, path in (('TestLake_mask_on', B.TEST + '/MI_FrozenLakeIce_TestLake'),
                             ('Lake_masked_mask_on', B.HOME + '/MI_FrozenLakeIce_Lake'),
                             ('Infantry', B.HOME + '/MI_FrozenLakeIce_Infantry'),
                             ('Water', B.HOME + '/MI_FrozenLakeWater')):
                B.report['stats_' + nm] = B.stats(unreal.load_asset(path))
                log('stats %s %s' % (nm, B.report['stats_' + nm]))
            log('stats after PIE %s' % B.report['stats_master_after_pie'])
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
            S['phase'], S['mark'] = 'end', S['t']
        elif phase == 'end' and S['t'] - S['mark'] > 5.0:
            finish()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log('ERROR %s: %r' % (phase, error))
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
