"""Where does a fire stand inside a hero ruin? Run in the throwaway editor with Ger_Berlin open (atm_cmd.py <this file>).

For one placed actor of each hero piece it traces straight down on a 50 cm grid over the piece (complex collision), turns
the hits into a height map in the piece's own axes and picks the floor cell that is most enclosed by standing walls: the
fire burns inside the shell, where the walls catch its light. Writes
  Source/Tools/BerlinMap/atmosphere/fire_host_table.json     {piece: [local x, local y, local z]}  (read by atmosphere_gen.py)
  Saved/Berlin/atmosphere/probe/host_<piece>.json            the height map, for a picture (atmosphere_host_picture.py)
Nothing is changed in the level.
"""
import json
import math

import unreal

PIECES = ("E1_CornerTower", "E2_Courtyard", "E3_CoreRow")
STEP = 50.0
TABLE = "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/atmosphere/fire_host_table.json"
OUT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/atmosphere/probe/"
FLOOR_MAX = 420.0       # cm above the pivot: floor, or the rubble heap inside the shell
WALL_MIN = 700.0        # cm: a standing wall
WALLS_NEEDED = 10       # of 16 directions: the spot is inside the shell
REACH = 900.0           # cm: walls further than this do not enclose

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
table = {}
for piece in PIECES:
    actor = next((a for a in actors if a.get_actor_label().startswith("RUIN_%s_" % piece)), None)
    if actor is None:
        print("no actor of", piece)
        continue
    origin, extent = actor.get_actor_bounds(False)
    pivot = actor.get_actor_location()
    yaw = math.radians(actor.get_actor_rotation().yaw)
    half = max(extent.x, extent.y) + 100.0
    n = int(half * 2 / STEP) + 1
    cells = {}
    for j in range(n):
        for i in range(n):
            lx, ly = -half + i * STEP, -half + j * STEP
            wx = pivot.x + lx * math.cos(yaw) - ly * math.sin(yaw)
            wy = pivot.y + lx * math.sin(yaw) + ly * math.cos(yaw)
            hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(wx, wy, pivot.z + 5000.0), unreal.Vector(wx, wy, pivot.z - 1500.0),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
            if hit is None:
                continue
            data = hit.to_tuple()
            hit_actor = data[9]
            cells[(i, j)] = (lx, ly, data[4].z - pivot.z, hit_actor == actor)
    own = [c for c in cells.values() if c[3]]
    best, best_score = None, -1.0
    for (i, j), (lx, ly, h, mine) in cells.items():
        if h > FLOOR_MAX or h < -80.0:
            continue
        # enclosure: in how many of 16 directions does a standing wall of this ruin rise within REACH, and how near is the nearest
        walls, nearest = 0, REACH
        for k in range(16):
            a = k * math.pi / 8.0
            for step in range(1, int(REACH / STEP) + 1):
                other = cells.get((i + int(round(math.cos(a) * step)), j + int(round(math.sin(a) * step))))
                if other is not None and other[3] and other[2] >= WALL_MIN:
                    walls += 1
                    nearest = min(nearest, step * STEP)
                    break
        if nearest < 200.0 or walls < WALLS_NEEDED:
            continue                        # not against a wall (the flames would clip into it), and not outside the shell
        score = walls - h / 150.0 - abs(nearest - 350.0) / 1000.0
        if score > best_score:
            best, best_score = (lx, ly, h, walls, nearest), score
    print(piece, actor.get_actor_label(), "cells", len(cells), "own", len(own), "best", best)
    if best is not None:
        table[piece] = [round(best[0], 1), round(best[1], 1), round(best[2] + 25.0, 1), best[3], best[4]]
    with open(OUT + "host_%s.json" % piece, "w") as handle:
        json.dump(dict(piece=piece, actor=actor.get_actor_label(), step=STEP, half=half, n=n, best=best,
                       cells=[[i, j, round(c[2]), int(c[3])] for (i, j), c in cells.items()]), handle)
with open(TABLE, "w") as handle:
    json.dump(table, handle, indent=1)
print("table", table)
