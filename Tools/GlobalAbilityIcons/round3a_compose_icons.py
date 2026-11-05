"""Round 3a (Icons A agent, 2026-11-05): the unit-drop ability icons, in the style of compose_ability_icons.py.

Every icon shows the units the ability delivers, rendered from the game's own Blueprints (round3a_render_parts.py),
and the delivery: a Ju 390 carrying vehicles under its belly, a Ju 390 coming in to land for walkers and landed
infantry, paratroopers under the German canopy, or the Mammut glider for the super-heavies (the tank big on the
ground in front, the towed glider in the sky behind it). Effects (laser, flame) are drawn, never the game's smoke.

Usage: python round3a_compose_icons.py [icon names]
Writes Saved/GlobalAbilityIcons/out/T_GA_<Name>.png and Saved/AbilityIconsA/preview_round3a.png.
"""
import math
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compose_ability_icons as cai  # noqa: E402

SS = cai.SS
SKY = cai.SKY_GER
# Feet of the landed soldiers: just above the hotkey label that covers the bottom fifth of the button.
HERO_FOOT = 404
# Bright paint (the Vultures' yellow hazmat suits, the pale Panzerfaust) blooms in the photographic finish: less gain.
HERO_GAIN = {"Hero_Vulture": 0.85, "Hero_EliteScavFaust": 1.0}
REVIEW_DIR = "D:/UE5Projects/RTS_Survival/Saved/AbilityIconsA/"


# --- shared pieces ------------------------------------------------------------------------------------------------------

def ground_shadow(img, centre, half_width, half_height, strength=0.55):
    """A soft contact shadow so a unit stands on the ground instead of floating over it."""
    x, y = centre[0] * SS, centre[1] * SS
    mask = cai.drawn(lambda dr: dr.ellipse([x - half_width * SS, y - half_height * SS, x + half_width * SS,
                                            y + half_height * SS], fill=255), blur=half_height * 0.7 * SS)
    layer = np.zeros_like(img)
    layer[..., 3] = mask * strength
    return cai.over(img, layer)


def put(img, name, centre=None, height=None, width=None, foot=None, gain=1.15, depth=0.0, rim_rgb=None, flip=False):
    """Places a cut-out by its visible box: `height` (or `width`) final pixels; either its box centre at `centre` or
    the middle of its bottom edge (the feet / tracks) at `foot`. depth fades it into the sky like far aircraft."""
    cut, pts = cai.load_cutout(name)
    if flip:
        cut = cut[:, ::-1].copy()
    x0, y0, x1, y1 = cai.bbox(cut)
    scale = height / (y1 - y0) if height else width / (x1 - x0)
    if foot is not None:
        anchor = ((x0 + x1) / 2, y1)
        at = foot
    else:
        anchor = ((x0 + x1) / 2, (y0 + y1) / 2)
        at = centre
    layer, to_canvas = cai.place(cut, pts, anchor, at, scale, gain=gain, tint=SKY, tint_amount=0.42 * depth)
    if rim_rgb:
        img = cai.over(img, cai.rim(layer, rim_rgb, 2.0 * SS, 0.45))
    return cai.over(img, layer), (x0, y0, x1, y1), scale, to_canvas


def ju390(img, at, scale, depth=0.0):
    cut, pts = cai.load_cutout("Ju390_Level")
    img, _, _ = cai.plane(img, cut, pts, at, scale, depth, SKY, gain=1.2, prop=[(r, 85.0) for r in cai.ROTORS])
    return img


def carried_vehicle(img, tank, at, scale, depth=0.0, hang=395.0, size=1.1):
    """One Ju 390 with a vehicle hanging under it, as cai.tank_drop draws each plane."""
    cut, pts = cai.load_cutout("Ju390_Level")
    plane_px = np.linalg.norm(np.array(pts["forward"]) - np.array(pts["origin"]))
    tank_cut, tank_pts = cai.load_cutout(tank)
    tank_px = np.linalg.norm(np.array(tank_pts["forward"]) - np.array(tank_pts["origin"]))
    _, to_canvas = cai.place(cut, pts, "origin", at, scale)
    o = to_canvas(pts["origin"])
    up = (to_canvas(pts["up"]) - o) / 300.0
    layer, tank_canvas = cai.place(tank_cut, tank_pts, "origin", (o - up * hang) / SS, scale * plane_px / tank_px * size,
                                   gain=1.25, tint=SKY, tint_amount=0.42 * depth)
    lift = [o + up * -120 + np.array([d * scale * SS, 0]) for d in (-120, 150)]
    hook = [tank_canvas(tank_pts["origin"]) + up * 150 + np.array([d * scale * SS, 0]) for d in (-170, 190)]
    cables = cai.drawn(lambda dr: [dr.line([tuple(a), tuple(b)], fill=255, width=max(2, int(5 * scale * SS)))
                                   for a, b in zip(lift, hook)], blur=0.7)
    cable_layer = np.zeros_like(img)
    cable_layer[..., :3] = 0.08 * cables[..., None]
    cable_layer[..., 3] = cables
    img = cai.over(cai.over(img, cable_layer), layer)
    img, _, _ = cai.plane(img, cut, pts, at, scale, depth, SKY, gain=1.2, prop=[(r, 85.0) for r in cai.ROTORS])
    return img


def mammut(img, centre, width, depth=0.35):
    """The He 111 Z towing the Mammut, small in the sky behind the delivered tank."""
    img, _, _, _ = put(img, "Mammut_Tow", centre=centre, width=width, gain=1.15, depth=depth)
    return img


# --- icon kinds ----------------------------------------------------------------------------------------------------------

def glider_delivery(tank, seed, effect=None):
    """Mammut super-heavy delivery: the tank big on the ground, the towed glider coming in behind it."""
    img = cai.background("ger", seed, horizon=0.56)
    img = mammut(img, (268, 120), 320, depth=0.12)
    img = ground_shadow(img, (238, 362), 196, 28)
    img, box, scale, to_canvas = put(img, tank, foot=(238, 372), width=400, gain=1.2)
    if effect:
        img = effect(img, tank, to_canvas)
    return cai.finish(img)


# How far under the Ju 390 a lone vehicle hangs (render units along the plane's up axis).
SINGLE_HANG = {"PzIVHarvester": 560.0, "PzIHarvester": 470.0, "Puma": 470.0}


def vehicle_drop(tanks, seed):
    """Ju 390s carrying vehicles under the belly, like the round-1 German vehicle drops (one plane per vehicle)."""
    if len(tanks) == 1:
        # one big vehicle: hung lower than the formation drops so a bulky hull (harvester arm) clears the wings
        img = cai.background("ger", seed, ground=False)
        return cai.finish(carried_vehicle(img, tanks[0], (250, 150), 0.48, hang=SINGLE_HANG.get(tanks[0], 470.0)))
    return cai.ju390_drop(tanks, seed)


def paradrop(heroes, chutes, seed, cargo=(), carried=None):
    """The Ju 390 overhead, the squads' own soldiers under canopies, and the first ones landed big in front.
    heroes = (render, foot x, height, flip); chutes = (render, x, y, height, depth), far first;
    cargo = (render, x, y, height, depth) for guns and buildings on their big canopies."""
    img = cai.background("ger", seed)
    if carried:
        # a second Ju 390 lands a vehicle (composite drops): it hangs under the belly like the vehicle drops
        img = carried_vehicle(img, carried, (330, 150), 0.40)
    else:
        img = ju390(img, (334, 70), 0.32, depth=0.25)
    for name, x, y, height, depth in chutes:
        img, _, _, _ = put(img, name, centre=(x, y), height=height, depth=depth, gain=1.2)
    for name, x, y, width, depth in cargo:
        img = cargo_drop(img, name, (x, y), width, depth)
    for name, x, height, flip in heroes:
        img = ground_shadow(img, (x, HERO_FOOT - 2), height * 0.22, 9)
        gain = HERO_GAIN.get(name, 1.2)
        img, _, _, _ = put(img, name, foot=(x, HERO_FOOT), height=height, gain=gain, rim_rgb=(255, 228, 180), flip=flip)
    return cai.finish(img)


def cargo_drop(img, name, centre, width, depth=0.0, canopy_scale=1.35):
    """A gun or building under a big cargo canopy (the cargo chute is the soldier canopy scaled up): the canopy's
    lines meet just above the load, as CargoParachuteDescentComponent rigs it."""
    img, box, scale, to_canvas = put(img, name, centre=centre, width=width, depth=depth, gain=1.2)
    top = to_canvas((0, box[1]))[1] / SS
    img, _, _, _ = put(img, "Chute_Only", foot=(centre[0], top + 4), width=width * canopy_scale, depth=depth,
                       gain=1.1)
    return img


def landed_drop(walkers, seed, plane_at=(330, 120), plane_scale=0.34):
    """A Ju 390 coming in to land, its squad already walking out across the field in front.
    walkers = (render, foot x, foot y, height, depth, flip), far first."""
    img = cai.background("ger", seed, horizon=0.6)
    img = ju390(img, plane_at, plane_scale, depth=0.2)
    for name, x, y, height, depth, flip in walkers:
        img = ground_shadow(img, (x, y - 2), height * 0.24, max(4, height * 0.03), 0.5 * (1 - depth))
        img, _, _, _ = put(img, name, foot=(x, y), height=height, depth=depth, gain=1.25, flip=flip)
    return cai.finish(img)


# --- drawn effects for the glider tanks --------------------------------------------------------------------------------

def muzzle_of(tank, to_canvas):
    """The barrel tip: the covered render pixel furthest along the tank's forward axis on screen (the gun is the part
    that reaches furthest forward on every glider tank). Returns the canvas point and the forward direction."""
    cut, pts = cai.load_cutout(tank)
    forward = cai.forward_on_screen(pts)
    rows, cols = np.nonzero(cut[..., 3] > 0.5)
    reach = cols * forward[0] + rows * forward[1]
    i = int(np.argmax(reach))
    tip = to_canvas((cols[i], rows[i]))
    direction = to_canvas((cols[i] + forward[0] * 100, rows[i] + forward[1] * 100)) - tip
    return tip, direction / np.linalg.norm(direction)


def laser_beam(img, tank, to_canvas):
    tip, direction = muzzle_of(tank, to_canvas)
    end = tip + direction * cai.CANVAS
    beam = cai.drawn(lambda dr: cai.taper(dr, tip, end, 16 * SS, 30 * SS), blur=8 * SS)
    core = cai.drawn(lambda dr: cai.taper(dr, tip, end, 5 * SS, 9 * SS), blur=1.0)
    img = cai.add(img, cai.light_layer(beam, (255, 70, 40)), 0.9)
    img = cai.add(img, cai.light_layer(core, (255, 235, 210)), 1.0)
    return cai.star(img, tip, 22 * SS, math.degrees(math.atan2(direction[1], direction[0])))


def flame_jet(img, tank, to_canvas):
    tip, direction = muzzle_of(tank, to_canvas)
    base_angle = math.atan2(direction[1], direction[0])
    rng = np.random.default_rng(5)

    def tongue(draw, k):
        for i in range(7):
            length = (120 + rng.uniform(-20, 30)) * SS * k
            spread = base_angle + rng.uniform(-0.2, 0.2)
            end = tip + np.array([math.cos(spread), math.sin(spread)]) * length
            cai.taper(draw, tip, end, 6 * SS * k, 34 * SS * k)

    img = cai.add(img, cai.light_layer(cai.drawn(lambda dr: tongue(dr, 1.0), blur=9 * SS), (255, 90, 20)), 0.95)
    img = cai.add(img, cai.light_layer(cai.drawn(lambda dr: tongue(dr, 0.6), blur=4 * SS), (255, 190, 70)), 1.0)
    return cai.add(img, cai.light_layer(cai.drawn(lambda dr: tongue(dr, 0.3), blur=2 * SS), (255, 250, 220)), 1.0)


# --- the icons ---------------------------------------------------------------------------------------------------------

ICONS = {
    # Mammut super-heavy deliveries
    "GerE50Drop": lambda: glider_delivery("E50_Ground", 401),
    "GerE75Drop": lambda: glider_delivery("E75_Ground", 403),
    "GerMausDrop": lambda: glider_delivery("Maus_Ground", 405),
    "GerJagdTigerLaserDrop": lambda: glider_delivery("JagdTigerLaser_Ground", 407, laser_beam),
    "GerJagdPantherFlamerDrop": lambda: glider_delivery("JagdPantherFlamer_Ground", 409, flame_jet),
    "GerSturmtigerDrop": lambda: glider_delivery("SturmTiger_Ground", 411),
    "GerJagdTigerDrop": lambda: glider_delivery("JagdTigerScrap_Ground", 413),
    "GerKingTiger105Drop": lambda: glider_delivery("TigerII105_Ground", 415),
    "GerTigerRailDrop": lambda: glider_delivery("TigerRail_Ground", 417),
    # Ju 390 vehicle drops
    "GerRadixitePzIIDrop": lambda: vehicle_drop(("PzIIRadix", "PzIIRadix"), 421),
    "GerRadixitePzIIIWespeDrop": lambda: vehicle_drop(("WespeRadix", "PzIIIRadix"), 423),
    "GerPzJagerLaserDrop": lambda: vehicle_drop(("PzJagerLaser", "PzJagerLaser", "PzJagerLaser"), 425),
    "GerPzIIIFlammDrop": lambda: vehicle_drop(("PzIIIFlamm", "PzIIIFlamm"), 427),
    "GerPumaDrop": lambda: vehicle_drop(("Puma",), 429),
    # Paradrops
    "GerEliteScavengersParadrop": lambda: paradrop(
        [("Hero_EliteScavFaust", 84, 250, True), ("Hero_EliteScav", 212, 260, False)],
        [("Para_PostApo_B", 420, 186, 96, 0.6), ("Para_PostApo_A", 300, 214, 120, 0.4),
         ("Para_PostApo_B", 400, 300, 140, 0.15)], 431),
    "GerRailTroopersParadrop": lambda: paradrop(
        [("Hero_RailTrooper", 150, 275, False)],
        [("Para_PostApo_B", 410, 200, 110, 0.5), ("Para_PostApo_A", 330, 290, 150, 0.15)], 433),
    "GerLightBringersParadrop": lambda: paradrop(
        [("Hero_LightBringer", 150, 275, False)],
        [("Para_Dystopia_B", 410, 200, 110, 0.5), ("Para_Dystopia_A", 330, 290, 150, 0.15)], 435),
    "GerFeuerSturmParadrop": lambda: paradrop(
        [("Hero_FeuerSturm", 84, 260, True), ("Hero_FeuerSturm", 212, 260, False)],
        [("Para_CyborgFlame_B", 420, 186, 96, 0.6), ("Para_CyborgFlame_A", 300, 214, 120, 0.4),
         ("Para_CyborgFlame_B", 400, 300, 140, 0.15)], 437),
    "GerParatroopers": lambda: paradrop(
        [("Hero_PzGren", 84, 275, True), ("Hero_Vulture", 212, 260, False)],
        [("Para_Ger_B", 420, 186, 96, 0.6), ("Para_CyborgArmored_A", 300, 214, 120, 0.4),
         ("Para_Ger_A", 400, 300, 140, 0.15)], 439),
    "GerIronStormParadrop": lambda: paradrop(
        [("Hero_IronStorm", 150, 275, False)],
        [("Para_CyborgArmored_B", 410, 200, 110, 0.5), ("Para_CyborgArmored_A", 330, 290, 150, 0.15)], 451),
    "GerPzGrenIronStormParadrop": lambda: paradrop(
        [("Hero_PzGren", 84, 275, True), ("Hero_IronStorm", 212, 260, False)],
        [("Para_CyborgArmored_B", 420, 186, 96, 0.6), ("Para_CyborgArmored_A", 300, 214, 120, 0.4),
         ("Para_CyborgArmored_B", 400, 300, 140, 0.15)], 453),
    # Composite drops: infantry paradropped beside a gun, a pit or a landed vehicle
    "GerMarderScavengerDrop": lambda: paradrop(
        [("Hero_Scavenger", 110, 275, False)],
        [("Para_PostApo_B", 196, 170, 100, 0.45), ("Para_PostApo_A", 116, 120, 84, 0.6)], 455, carried="MarderDual"),
    "GerEliteScavengersPakDrop": lambda: paradrop(
        [("Hero_EliteScavFaust", 84, 250, True), ("Hero_EliteScav", 212, 260, False)],
        [("Para_PostApo_B", 300, 196, 100, 0.45)], 457,
        cargo=[("Pak40_Gun", 420, 300, 150, 0.1), ("Pak40_Gun", 330, 150, 96, 0.5)]),
    "GerSturmPionierenPakPitDrop": lambda: paradrop(
        [("Hero_SturmPioneer", 130, 260, False)],
        [("Para_CyborgFlame_B", 236, 170, 96, 0.5)], 459, cargo=[("Pit_Pak40", 380, 300, 210, 0.05)]),
    # Landed (walk-out) drops
    "GerHarvesterDrop": lambda: vehicle_drop(("PzIVHarvester",), 471),
    "GerPzIHarvesterDrop": lambda: vehicle_drop(("PzIHarvester",), 473),
    "GerPz38tRailgunDrop": lambda: vehicle_drop(("Pz38tRail", "Pz38tRail"), 475),
    "GerScavengersDrop": lambda: landed_drop(
        [("Hero_Scavenger", 420, 336, 110, 0.45, True), ("Mortar_GrW42", 330, 352, 90, 0.3, False),
         ("Hero_Scavenger", 250, 392, 210, 0.1, True), ("Hero_Scavenger", 130, 406, 262, 0.0, False)], 477),
    "GerFelsKriegerDrop": lambda: landed_drop(
        [("Hero_FelsKrieger", 360, 352, 150, 0.35, False), ("Hero_FelsKrieger", 170, 404, 270, 0.0, False)], 461),
    "GerRedSpineDrop": lambda: landed_drop(
        [("Hero_RedSpineB", 380, 330, 120, 0.45, False), ("Hero_RedSpine", 300, 372, 170, 0.2, False),
         ("Hero_RedSpine", 160, 404, 240, 0.0, False)], 441),
    "GerRedSpinePairDrop": lambda: landed_drop(
        [("Hero_RedSpineB", 470, 322, 96, 0.55, False), ("Hero_RedSpineB", 410, 330, 110, 0.5, False),
         ("Hero_RedSpine", 340, 340, 124, 0.45, False), ("Hero_RedSpineB", 300, 372, 170, 0.2, False),
         ("Hero_RedSpine", 220, 388, 196, 0.1, False), ("Hero_RedSpine", 120, 406, 240, 0.0, False)], 443),
    "GerRailTroopersDrop": lambda: landed_drop(
        [("Hero_RailTrooper", 430, 332, 110, 0.5, True), ("Hero_RailTrooper", 350, 352, 140, 0.35, False),
         ("Hero_RailTrooper", 250, 392, 210, 0.1, True), ("Hero_RailTrooper", 140, 406, 262, 0.0, False)], 445),
}


def main():
    os.makedirs(cai.OUT, exist_ok=True)
    os.makedirs(REVIEW_DIR, exist_ok=True)
    names = sys.argv[1:] or list(ICONS)
    made = []
    for name in names:
        try:
            icon = ICONS[name]()
        except FileNotFoundError as error:
            print("SKIP", name, "missing render:", os.path.basename(str(error.filename)))
            continue
        icon.save(cai.OUT + "T_GA_%s.png" % name)
        made.append((name, icon))
        print("ICON", name)
    if not made:
        return
    columns = min(len(made), 5)
    rows = (len(made) + columns - 1) // columns
    sheet = Image.new("RGBA", (columns * 330 + 10, rows * 440 + 10), (28, 28, 30, 255))
    for i, (name, icon) in enumerate(made):
        x, y = 10 + (i % columns) * 330, 10 + (i // columns) * 440
        sheet.alpha_composite(icon.resize((320, 320), Image.LANCZOS), (x, y))
        sheet.alpha_composite(icon.resize((96, 96), Image.LANCZOS), (x, y + 326))
        sheet.alpha_composite(icon.resize((64, 64), Image.LANCZOS), (x + 104, y + 326))
        sheet.alpha_composite(icon.resize((48, 48), Image.LANCZOS), (x + 176, y + 326))
    sheet.save(REVIEW_DIR + "preview_round3a.png")


if __name__ == "__main__":
    main()
