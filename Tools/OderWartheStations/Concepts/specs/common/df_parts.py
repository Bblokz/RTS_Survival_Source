"""Direction-finder station concept parts (greybox, runs inside Blender through the Breakthrough concept tools).

Frame: metres, x = right, y = back (away from the RTS camera), z = up, origin = the station actor = the centre of the
capture disc (25 m radius). The RTS camera looks from the front-right (+x, -y).

Environment switches (set by run_round.sh):
  DF_STATE = enemy | player   which side holds the station (flag, holder sign, lamp, loop heading)
  DF_ICON  = 1                 front elevation only, transparent background, no ground: the minimap icon source
"""
import math
import os

import bpy
import concept_kit

P = concept_kit.PAL
P.setdefault('aerial', (0.16, 0.52, 0.86, 1))
P.setdefault('gen', (0.86, 0.62, 0.12, 1))
P.setdefault('flag_enemy', (0.80, 0.08, 0.08, 1))
P.setdefault('flag_player', (0.16, 0.16, 0.16, 1))
P.setdefault('flag_ghost', (0.97, 0.80, 0.80, 1))
P.setdefault('ui', (0.20, 0.78, 0.35, 1))
P.setdefault('timber', (0.47, 0.35, 0.21, 1))
P.setdefault('timber_dark', (0.32, 0.24, 0.15, 1))
P.setdefault('gravel', (0.66, 0.63, 0.56, 1))
P.setdefault('mark_enemy', (0.85, 0.10, 0.10, 1))
P.setdefault('mark_player', (0.93, 0.93, 0.93, 1))
P.setdefault('cable', (0.06, 0.06, 0.06, 1))
P.setdefault('tarp', (0.33, 0.36, 0.27, 1))
P.setdefault('lamp_on', (1.0, 0.86, 0.30, 1))

STATE = os.environ.get('DF_STATE', 'enemy')
PLAYER = STATE == 'player'
ICON = os.environ.get('DF_ICON') == '1'
SUB = 'Panzer IV + soldier for scale  |  origin = capture centre (25 m radius)  |  '

# Every spec shares these view settings so the sizes compare directly.
VIEW_ZOOM = 1.5
ICON_VIEW = dict(rot=(90, 0, 0), labels=False, caption='Front elevation (icon source)')


def view_overrides(target_z=4.0):
    if ICON:
        return {'rts': dict(ICON_VIEW, target=(0, 0, target_z), res=(1600, 1100))}
    return {}


def setup(k):
    """Call first in build()."""
    bpy.context.scene.view_settings.exposure = 1.0
    if ICON:
        bpy.context.scene.render.film_transparent = True
        k.ground.hide_render = True


def rot(o, d, yaw):
    a = math.radians(yaw)
    return (o[0] + d[0] * math.cos(a) - d[1] * math.sin(a), o[1] + d[0] * math.sin(a) + d[1] * math.cos(a))


def circle(c, r, n=24, a0=0.0, a1=360.0):
    return [(c[0] + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
             c[1] + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n)]


# ------------------------------------------------------------------ ground
def yard(k, r=5.5, c=(0, 0)):
    """Open gravel yard round the capture centre: units gather here, nothing blocks it."""
    k.poly('yard', circle(c, r, 28), 0.03, color='gravel', cat='mark')


# ------------------------------------------------------------------ capture flag
def flag(k, x, y, h=7.5, z=0.0, label='Capture flag (rises 5.5 m)'):
    """Steel pole on a square concrete plinth; the flag colour follows the holder, the pale ghost marks the
    lowered position. Thinner and shorter than any aerial mast, with a ball finial and no guy wires."""
    k.box('flag_plinth', (1.0, 1.0, 0.45), (x, y, z), color='concrete', round_top=0.06, segments=2)
    k.cyl('flag_pole', 0.075, h, (x, y, z + 0.4), color='frame', segments=10, cat='mast')
    k.cyl('flag_ball', 0.15, 0.25, (x, y, z + h + 0.4), color='lamp_on' if PLAYER else 'frame', segments=10,
          cat='mast')
    yaw = 35.8
    cloth = 'flag_player' if PLAYER else 'flag_enemy'
    cx, cy = rot((x, y), (0.85, 0), yaw)
    k.box('flag_cloth', (1.6, 0.05, 1.05), (cx, cy, z + h - 0.85), color=cloth, yaw=yaw, cat='mast')
    if PLAYER:  # white cross field on the dark banner
        k.box('flag_mark', (0.55, 0.07, 0.14), (cx, cy, z + h - 0.38), color='mark_player', yaw=yaw, cat='mast')
        k.box('flag_mark', (0.14, 0.07, 0.55), (cx, cy, z + h - 0.58), color='mark_player', yaw=yaw, cat='mast')
    if not ICON:
        gx, gy = rot((x, y), (0.85, 0), yaw)
        k.box('flag_ghost', (1.6, 0.04, 1.05), (gx, gy, z + 1.2), color='flag_ghost', yaw=yaw, cat='mast')
    k.label(label, 'flag', (x, y, z + h + 0.7))


# ------------------------------------------------------------------ holder look
def holder_sign(k, x, y, yaw=0, label=True):
    """Board on two posts by the yard: red star while the enemy holds the station, white board with a black
    cross while the player does."""
    for dx in (-0.7, 0.7):
        px, py = rot((x, y), (dx, 0), yaw)
        k.box('sign_post', (0.14, 0.14, 2.0), (px, py, 0), color='timber_dark', yaw=yaw)
    k.box('sign_board', (1.8, 0.08, 1.1), (x, y, 1.0), color='mark_player' if PLAYER else 'timber', yaw=yaw)
    if PLAYER:
        k.box('sign_cross', (0.9, 0.1, 0.22), (x, y, 1.45), color='cable', yaw=yaw)
        k.box('sign_cross', (0.22, 0.1, 0.9), (x, y, 1.1), color='cable', yaw=yaw)
    else:
        star(k, (x, y, 1.55), 0.42, yaw)
    if label:
        k.label('Holder sign (red star / Balkenkreuz)', 'held', (x, y, 2.3))


def star(k, centre, r, yaw=0, color='mark_enemy', depth=0.1):
    """Five-point star painted on a board facing -y (turned by yaw)."""
    pts = []
    for i in range(10):
        a = math.radians(90 + i * 36)
        rr = r if i % 2 == 0 else r * 0.42
        pts.append((rr * math.cos(a), rr * math.sin(a)))
    obj = k.prism('star', [(px, pz) for px, pz in pts], depth, (0, 0, 0), color=color)
    obj.rotation_euler = (0, 0, math.radians(yaw))
    obj.location = (centre[0], centre[1], centre[2])
    return obj


def lamp(k, x, y, z):
    """Hut lamp: lit while the player holds the station."""
    k.box('lamp', (0.3, 0.3, 0.3), (x, y, z), color='lamp_on' if PLAYER else 'steel', round_top=0.08,
          segments=2)


# ------------------------------------------------------------------ aerials
def loop_aerial(k, base, shaft_h, r=1.05, label='Rotating loop aerial'):
    """Vertical shaft with a round frame loop; it turns slowly (enemy: slow sweep, player: hunting a bearing)."""
    x, y, z = base
    yaw = 20.0 if PLAYER else 75.0
    k.cyl('loop_shaft', 0.09, shaft_h, (x, y, z), color='aerial', segments=10, cat='mast')
    k.cyl('loop_drum', 0.28, 0.35, (x, y, z), color='steel', segments=12)
    cz = z + shaft_h + r
    n = 20
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n
        dx, dz = r * math.cos(a), r * math.sin(a)
        px, py = rot((x, y), (dx, 0), yaw)
        pts.append((px, py, cz + dz))
    for a, b in zip(pts, pts[1:] + pts[:1]):
        k.beam(a, b, w=0.11, color='aerial', name='loop')
    k.beam((x, y, cz - r), (x, y, cz + r), w=0.06, color='aerial', name='loop_spoke')
    k.label(label, 'aerial', (x, y, cz + r + 0.5))
    return cz + r


def whip(k, x, y, z, h=3.5):
    k.cyl('sense_whip', 0.035, h, (x, y, z), color='aerial', segments=6, cat='mast')


def guyed_mast(k, x, y, h, color='timber', guys=3, guy_r=None, yaw0=0, crossarm=True, name='adcock_mast'):
    """Tapered timber mast with a crossarm and guy wires to pegs (the Adcock array element)."""
    k.cyl(name, 0.17, h, (x, y, 0), r_top=0.09, color=color, segments=10, cat='mast')
    k.box('mast_foot', (0.7, 0.7, 0.25), (x, y, 0), color='concrete')
    if crossarm:
        k.beam(rot((x, y), (-0.8, 0), yaw0 + 45) + (h - 0.6,), rot((x, y), (0.8, 0), yaw0 + 45) + (h - 0.6,),
               w=0.1, color='aerial', name='crossarm')
        k.cyl('mast_tip', 0.05, 1.2, (x, y, h), color='aerial', segments=6, cat='mast')
    gr = guy_r or h * 0.55
    for i in range(guys):
        gx, gy = rot((x, y), (gr, 0), yaw0 + i * 360 / guys)
        k.beam((x, y, h * 0.82), (gx, gy, 0.15), w=0.03, color='cable', name='guy')
        k.box('guy_peg', (0.3, 0.3, 0.2), (gx, gy, 0), color='concrete_dark')


def feeder(k, a, b, z=0.9, posts=3):
    """Low feeder line on short posts (aerial to receiver)."""
    pts = [(a[0] + (b[0] - a[0]) * t / posts, a[1] + (b[1] - a[1]) * t / posts) for t in range(posts + 1)]
    for p in pts[1:-1]:
        k.box('feeder_post', (0.12, 0.12, z), (p[0], p[1], 0), color='timber_dark')
    for p0, p1 in zip(pts, pts[1:]):
        k.beam(p0 + (z,), p1 + (z,), w=0.05, color='cable', name='feeder')


# ------------------------------------------------------------------ buildings
def hut(k, x, y, w, d, h=2.6, yaw=0, roof='gable', color='timber', door=True, label='Receiver hut'):
    """Plank hut with a tar-paper roof; door faces -y (turned by yaw)."""
    k.group((x, y, 0), yaw)
    k.box('hut_sill', (w + 0.3, d + 0.3, 0.35), (0, 0, 0), color='concrete_dark')
    k.box('hut', (w, d, h), (0, 0, 0.3), color=color)
    if roof == 'gable':
        k.prism('roof', [(-w / 2 - 0.35, 0), (w / 2 + 0.35, 0), (0, 1.1)], d + 0.6, (0, 0, h + 0.3), yaw=0,
                color='steel')
    else:
        k.box('roof', (w + 0.5, d + 0.5, 0.22), (0, 0, h + 0.3), color='steel', rot=(4, 0, 0))
    if door:
        k.box('door', (0.95, 0.08, 1.95), (-w / 4, -d / 2 - 0.02, 0.35), color='interior')
        k.box('window', (0.9, 0.08, 0.7), (w / 4, -d / 2 - 0.02, 1.4), color='slot')
    k.end_group()
    k.label(label, 'hut', (x, y, h + 1.6))


def generator(k, x, y, yaw=0, shelter=True, label='Generator (runs while held)'):
    """Petrol generator on a skid with fuel drums, under a lean-to roof."""
    k.group((x, y, 0), yaw)
    k.box('gen_skid', (2.4, 1.2, 0.2), (0, 0, 0), color='timber_dark')
    k.box('gen_engine', (1.3, 0.9, 0.9), (-0.4, 0, 0.2), color='gen', round_top=0.08, segments=2)
    k.box('gen_dynamo', (0.8, 0.8, 0.7), (0.65, 0, 0.2), color='gen')
    k.cyl('gen_exhaust', 0.06, 1.6, (-0.8, 0.35, 1.0), color='cable', segments=8)
    for i, dx in enumerate((-0.6, 0.15, 0.9)):
        k.cyl('fuel_drum', 0.3, 0.9, (dx, 1.25, 0), color='steel', segments=12)
    if shelter:
        for sx in (-1.5, 1.5):
            for sy in (-0.9, 1.9):
                k.box('shelter_post', (0.12, 0.12, 2.3 if sy < 0 else 1.9), (sx, sy, 0), color='timber_dark')
        k.box('shelter_roof', (3.4, 3.2, 0.1), (0, 0.5, 2.1), color='tarp', rot=(-7, 0, 0))
    k.end_group()
    k.label(label, 'gen', (x, y, 2.8))


def progress_bar(k, z, x=0.0, y=0.0):
    k.box('ui_bar', (2.6, 0.3, 0.3), (x, y, z), color='ui', yaw=35.8, cat='mark')
    k.label('Capture bar (UI) %.0f m' % z, 'ui', (x, y, z + 0.3))


def crates(k, x, y, yaw=0, n=2):
    for i in range(n):
        px, py = rot((x, y), (i * 0.85, 0), yaw)
        k.box('crate', (0.8, 0.6, 0.55), (px, py, 0), color='wood', yaw=yaw)
