"""Lays out the RedLine defense line on /Game/RTS_Survival/Maps/GerMissions/RedLine.

Run as a commandlet (the landscape map only saves with rendering, so no -NullRHI):
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -AllowCommandletRendering
then rebuild the static navmesh:
  UnrealEditor-Cmd <uproject> -run=ResavePackages -BuildNavigationData -MAP=RedLine -unattended -AllowCommandletRendering
Re-runnable: every actor it places is labelled RL_* and removed first. The salient constants are mirrored in
RTS_Survival/Utils/RTSMapTests/RedLine/RedLineTourScenario.cpp.
"""
import math
import random
import unreal

MAP = '/Game/RTS_Survival/Maps/GerMissions/RedLine'
B = '/Game/RTS_Survival/Blueprints/'
RL = B + 'Environment/RedLine/'
BAR = B + 'Environment/DestructableEnvActor/Military/Barricades/'
BUNK = B + 'Buildings/BuildingExpansions/RusBunkers/'
SQ = B + 'Squads/Rus/'
TW = B + 'Squads/TeamWeapons/Soviet/'
PILLBOX = RL + 'Pillbox/Rus_BXP_RL_TurretPillbox'
# In placement order (west sectors, then the mirrored east ones). The heavy guns stand at the tank lane edges
# (4th, 5th, 7th and 10th), the KV-2 howitzers near the salient and on the east wing.
PILLBOX_VARIANTS = ['_T70', '', '_BT7', '_IS2', '_T34_85', '_KV2', '_T34_85', '_T70', '', '_IS1', '_BT7', '_KV2']
ENEMY = 2
SEGMENT = 1000.0
SALIENT_Y = 1800.0
SALIENT_SLOPE = 1200.0 / 19500.0
rng = random.Random(1942)

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    if actor.get_actor_label().startswith('RL_'):
        eas.destroy_actor(actor)

classes = {}


def cls(path):
    if path not in classes:
        asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        classes[path] = asset.generated_class() if asset else None
        if not asset:
            unreal.log_warning('RLLAYOUT MISSING %s' % path)
    return classes[path]


def ground_z(x, y):
    res = unreal.SystemLibrary.line_trace_multi(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                unreal.DrawDebugTrace.NONE, True)
    hits = res[1] if isinstance(res, tuple) else res
    for hit in hits or []:
        t = hit.to_tuple()
        if t[9] and 'Landscape' in t[9].get_class().get_name():
            return t[4].z
    return 100.0


def line_y(x):
    return SALIENT_Y + abs(x) * SALIENT_SLOPE


def heading_at(x):
    # The flanks are refused: the line bows forward (towards -Y) to a salient at x = 0.
    return math.degrees(math.atan(SALIENT_SLOPE)) * (1.0 if x > 0 else -1.0)


def fwd(yaw):
    """World direction of a piece's +X (its front) for a piece yaw."""
    return math.cos(math.radians(yaw)), math.sin(math.radians(yaw))


plan = []  # (group, label, path, x, y, yaw, scale_y, ground_samples)


def place(group, label, path, x, y, yaw, scale_y=1.0, samples=None):
    plan.append((group, label, path, x, y, yaw, scale_y, samples or [(x, y)]))


# ---------------------------------------------------------------- fire trench sectors
SECTORS = [  # (x start, segments, y shift, pillbox distances, mg nest distances, mosin distances, comm segment)
    (-20200.0, 5, 0.0, [1500.0, 3500.0], [2500.0], [1500.0, 3500.0], 1),
    (-13790.0, 6, -150.0, [2000.0, 5500.0], [1500.0, 4500.0], [2500.0, 4500.0], 0),
    (-5700.0, 5, 0.0, [500.0, 3000.0], [2500.0], [1500.0, 3500.0], 4),
]
SECTORS += [(-(x + n * SEGMENT * math.cos(math.atan(SALIENT_SLOPE))), n, shift,
             [n * SEGMENT - d for d in pill], [n * SEGMENT - d for d in mg], [n * SEGMENT - d for d in mos],
             n - 1 - comm)
            for (x, n, shift, pill, mg, mos, comm) in reversed(SECTORS)]
pillbox_index = 0
trench_count = 0
for sector_index, (x0, count, shift, pillboxes, nests, mosins, comm_segment) in enumerate(SECTORS):
    heading = heading_at(x0 + 1.0)
    hx, hy = math.cos(math.radians(heading)), math.sin(math.radians(heading))
    yaw = heading - 90.0
    front = fwd(yaw)
    sx, sy = x0, line_y(x0) + shift

    def along(d):
        return sx + hx * d, sy + hy * d

    for k in range(count):
        cx, cy = along((k + 0.5) * SEGMENT)
        ends = [along(k * SEGMENT + 50.0), along((k + 0.5) * SEGMENT), along((k + 1) * SEGMENT - 50.0)]
        trench_count += 1
        place('Trenches', 'RL_Trench_%02d' % trench_count, RL + 'BP_RL_Trench_Straight', cx, cy, yaw, samples=ends)
    place('Trenches', 'RL_TrenchEnd_%d_a' % sector_index, RL + 'BP_RL_TrenchEnd', sx, sy, yaw, scale_y=-1.0)
    ex, ey = along(count * SEGMENT)
    place('Trenches', 'RL_TrenchEnd_%d_b' % sector_index, RL + 'BP_RL_TrenchEnd', ex, ey, yaw)
    for d in pillboxes:
        px, py = along(d)
        variant = PILLBOX_VARIANTS[pillbox_index]
        pillbox_index += 1
        place('Pillboxes', 'RL_Pillbox_%02d%s' % (pillbox_index, variant), PILLBOX + variant,
              px + front[0] * 620.0, py + front[1] * 620.0, yaw)
    for i, d in enumerate(nests):
        px, py = along(d)
        nx, ny = px + front[0] * 1700.0, py + front[1] * 1700.0
        tag = '%d_%d' % (sector_index, i)
        place('ForwardNests', 'RL_MGNest_' + tag, RL + 'BP_RL_MGNest', nx, ny, yaw)
        place('Squads', 'RL_Maxim_' + tag, TW + 'MaximMG/Rus_TWSquad_Maxim', nx, ny, yaw)
        for side in (-1.0, 1.0):
            wx = nx + front[0] * 480.0 + hx * side * 230.0
            wy = ny + front[1] * 480.0 + hy * side * 230.0
            place('Wire', 'RL_NestWire_%s_%d' % (tag, side), BAR + 'BP_Military_Dest_BarbedWire', wx, wy,
                  heading + rng.uniform(-6.0, 6.0))
    for i, d in enumerate(mosins):
        px, py = along(d)
        squad = SQ + ('Rus_Squad_MosinPTRS' if (sector_index + i) % 3 == 1 else 'Rus_Squad_Mosin')
        place('Squads', 'RL_TrenchSquad_%d_%d' % (sector_index, i), squad,
              px - front[0] * 450.0, py - front[1] * 450.0, yaw)
    # A communication trench back to a dugout from one segment's rear gap, clear of the gun line.
    gx, gy = along((comm_segment + 0.5) * SEGMENT)
    rear = (-front[0], -front[1])
    for j, distance in enumerate((595.0, 1595.0)):
        place('Trenches', 'RL_CommTrench_%d_%d' % (sector_index, j), RL + 'BP_RL_CommTrench',
              gx + rear[0] * distance, gy + rear[1] * distance, heading)
    place('Rear', 'RL_Dugout_%d' % sector_index, RL + 'BP_RL_Dugout', gx + rear[0] * 2330.0, gy + rear[1] * 2330.0,
          yaw + 180.0)

# ---------------------------------------------------------------- strongpoints in the joints of the line
for side in (-1.0, 1.0):
    x = side * 14500.0
    place('Bunkers', 'RL_FlankBunker_%d' % side, BUNK + 'Rus_BXP_LongCamoBunker', x, line_y(x) - 120.0, -90.0)
    # Tank lanes at |x| ~ 6750: AT-gun bunker behind, pillboxes and MG bunkers on the lane edges, BT-7 towers
    # over them.
    place('Bunkers', 'RL_LaneATBunker_%d' % side, BUNK + 'Rus_BXP_Bunker02_ZIS', side * 6750.0,
          line_y(6750.0) + 2300.0, -90.0)
    for edge in (5450.0, 8050.0):
        place('Bunkers', 'RL_LaneMGBunker_%d_%d' % (side, edge), BUNK + 'Rus_BXP_MGBunker', side * edge,
              line_y(edge) - 1600.0, -90.0)
    for tower in (4800.0, 8700.0):
        place('Bunkers', 'RL_LaneTower_%d_%d' % (side, tower), BUNK + 'Rus_BXP_GuardTowerBT-7', side * tower,
              line_y(tower) + 1500.0, -90.0)
    place('Bunkers', 'RL_FlankTower_%d' % side, BUNK + 'Rus_BXP_GuardTowerMG_Soviet', side * 20500.0,
          line_y(20500.0) + 2200.0, -90.0)
place('Bunkers', 'RL_SalientDome', BUNK + 'Rus_BXP_DomeBunkerGarrison', 0.0, line_y(0.0) - 150.0, -90.0)
place('Bunkers', 'RL_CommandBunker', BUNK + 'Rus_BXP_MegaBunker_1', 0.0, line_y(0.0) + 3900.0, -90.0)

# ---------------------------------------------------------------- gun line and artillery
GUN_BAYS = [(17000.0, 'Zis_76MM/Rus_TWSquad_Zis3_76'), (10800.0, 'SovietHowitzer/Rus_SquadTW_M1938'),
            (3000.0, 'Zis_76MM/Rus_TWSquad_Zis3_76')]
for side in (-1.0, 1.0):
    for x, squad in GUN_BAYS:
        bx, by = side * x, line_y(x) + 1700.0
        tag = '%d_%d' % (side, x)
        place('GunLine', 'RL_GunBay_' + tag, RL + 'BP_RL_GunBay', bx, by, -90.0)
        place('Squads', 'RL_BayGun_' + tag, TW + squad, bx, by + 60.0, -90.0)
    for x, squad in ((8800.0, '120Mortar/Rus_SquadTW_120mmMortar'), (3900.0, '80MMMortar/Rus_SquadTW_80mmMortar')):
        mx, my = side * x, line_y(x) + 3000.0
        tag = '%d_%d' % (side, x)
        place('GunLine', 'RL_MortarPit_' + tag, RL + 'BP_RL_MGNest', mx, my, -90.0)
        place('Squads', 'RL_Mortar_' + tag, TW + squad, mx, my, -90.0)
    place('GunLine', 'RL_B4Pit_%d' % side, TW + 'B4_203MM/Pit/Rus_BXP_B4HowitzerPit', side * 12000.0,
          line_y(12000.0) + 4600.0, -90.0)
place('Squads', 'RL_ReserveAT', SQ + 'Rus_Squad_LargePTRSSquad', 0.0, line_y(0.0) + 2200.0, -90.0)

# ---------------------------------------------------------------- obstacle belts
LANES = [(-6750.0, 1050.0), (6750.0, 1050.0)]


def in_lane(x, margin=0.0):
    return any(abs(x - c) < half + margin for c, half in LANES)


wire_index = 0
x = -21000.0
while x <= 21000.0:
    for row, (offset, every) in enumerate(((3000.0, 1), (3600.0, 2))):
        wx = x + row * 260.0
        if in_lane(wx) or (every > 1 and int((x + 21000.0) / 520.0) % every):
            continue
        wire_index += 1
        heading = heading_at(wx)
        place('Wire', 'RL_Wire_%03d' % wire_index, BAR + 'BP_Military_Dest_BarbedWireLong',
              wx, line_y(wx) - offset + rng.uniform(-50.0, 50.0), heading - 90.0 + rng.uniform(-5.0, 5.0))
    x += 520.0

hedgehog_index = 0
for row, row_offset in enumerate((4800.0, 5250.0, 5700.0)):
    x = -23800.0 + row * 300.0
    while x <= 23800.0:
        if not in_lane(x, 150.0) and not (row == 2 and abs(x) < 15000.0):
            roll = rng.random()
            kind = 'BP_Military_Dest_Hedgehog' if roll < 0.6 else (
                'BP_Military_Dest_Hedgehog4' if roll < 0.85 else 'BP_Military_Dest_LargeHedgehog')
            hedgehog_index += 1
            place('Hedgehogs', 'RL_Hedgehog_%03d' % hedgehog_index, BAR + kind, x + rng.uniform(-90.0, 90.0),
                  line_y(x) - row_offset + rng.uniform(-90.0, 90.0), rng.uniform(0.0, 360.0))
        x += 600.0
# Concrete funnels guide tanks into the lanes, under the lane bunkers' guns.
for lane_x, half in LANES:
    for edge in (-1.0, 1.0):
        for k in range(4):
            bx = lane_x + edge * (half + 150.0 + k * 40.0)
            place('Hedgehogs', 'RL_LaneBarrier_%d_%d_%d' % (lane_x, edge, k), BAR + 'BP_Military_Dest_ConcreteBarrier',
                  bx, line_y(bx) - 4300.0 - k * 300.0, edge * 12.0)

# ---------------------------------------------------------------- spawn
spawned = 0
owners = {}
for group, label, path, x, y, yaw, scale_y, samples in plan:
    actor_class = cls(path)
    if not actor_class:
        continue
    z = min(ground_z(sx, sy) for sx, sy in samples)
    actor = eas.spawn_actor_from_class(actor_class, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
    if not actor:
        unreal.log_warning('RLLAYOUT spawn failed %s' % label)
        continue
    actor.set_actor_label(label)
    actor.set_folder_path('RedLine/' + group)
    if scale_y != 1.0:
        actor.set_actor_scale3d(unreal.Vector(1.0, scale_y, 1.0))
    for component in actor.get_components_by_class(unreal.ActorComponent):
        if component.get_class().get_name() != 'RTSComponent':
            continue
        owner = component.get_editor_property('OwningPlayer')
        if owner != ENEMY:
            component.set_editor_property('OwningPlayer', ENEMY)
        owners[path.split('/')[-1]] = '%s->%s' % (owner, ENEMY)
    spawned += 1
unreal.log_warning('RLLAYOUT planned=%d spawned=%d trenches=%d pillboxes=%d wire=%d hedgehogs=%d' % (
    len(plan), spawned, trench_count, pillbox_index, wire_index, hedgehog_index))
for name, change in sorted(owners.items()):
    unreal.log_warning('RLLAYOUT owner %s %s' % (name, change))
unreal.log_warning('RLLAYOUT saved %s' % unreal.EditorLoadingAndSavingUtils.save_map(world, MAP))
