"""Hooks for place_soviet_sites.py (Source/Tools/OreMountainsMission): the built weighbridge (1A Waagehaus) and the
border stones (6A Grenzsteine). Nothing here runs on import and nothing edits the map by itself; the lead wires it in:

    import sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/OreSitesModels/WeighbridgeBorder")
    import wbb_sites
    A.update(wbb_sites.ASSETS)                                   # new keys; A["hut"] stays (the radio hut uses it)
    SITES["Checkpoint"] = wbb_sites.checkpoint(item, A, DECK, deck_point, SITES["Checkpoint"])
    SITES["Border"] = wbb_sites.border(item);  REPLACED["Border"] = []
    REPLACED["Checkpoint"] += wbb_sites.REPLACED_CP              # the old raised double barrier (ore_map_build ORE_CP_*)
    ... pss.run(only=["Checkpoint", "Border"])
    wbb_sites.seat_deck(pss)     # tilt the deck onto the road plane (the road falls ~30 cm over the 18 m footprint)
    wbb_sites.seat_boom(pss)     # boom: post / rest on their own ground, arm tip on the rest
(check() then reports the deck "within ... of valley_road / gate" and its top "off the road": expected after seat_deck.)

Border transforms: border_stones.json beside this file's model (Unreal cm, z_raw = heightfield hint only; place_item
seats each stone by landscape traces: pivot = ground line, 0.3 m of stone below it, no collision).
"""
import json
import math

WB = "/Game/RTS_Survival/Blueprints/Environment/OreMountains/Sites/Weighbridge/"
ASSETS = {
    "wb_booth": WB + "BP_OreWB_Booth",              # child of Rus_Bxp_ContainerHut (health / death / owner unchanged)
    "wb_deck": WB + "Meshes/SM_OreWB_Deck",         # 4 m (X) x 18 m (Y), pivot on its top face, 1 box hull z -50..0
    "wb_boom": WB + "BP_OreWB_BoomBarrier",         # post at the pivot, arm along +X (tag BoomArm), rest at +X 1040
}
BORDER_JSON = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreSites/Model/border_stones.json"
# booth 1 m further south than the placeholder: with the bay its mesh reaches X +278 cm, so at -604 it would stand
# 0.5 m in the 10 m road (valley_road corridor 520 cm); at -700 the north face is 562 cm from the road line.
BOOTH = (-700.0, 7504.0, 0.0)
BOOM = (-386.67, 6852.0, 0.0)                      # the existing south barrier post; the rest lands on the north one
REPLACED_CP = ["ORE_CP_BarrierPost_-1", "ORE_CP_BarrierPost_1", "ORE_CP_BarrierBar_-1", "ORE_CP_BarrierBar_1"]


def checkpoint(item, A, DECK, deck_point, old):
    """The CHECKPOINT list with the booth, deck and boom switched to the built assets (towers, kerbs, guards kept)."""
    out = []
    for e in old:
        name = e["label"].split("Checkpoint_", 1)[-1]
        if name == "Booth":
            e = item("Checkpoint", "Booth", A["wb_booth"], *BOOTH)
        elif name == "Weighbridge":
            # real collision (vehicles drive on the plate); top_flush seats the top face 2 cm over the highest road point.
            # check() will report it "within ... of valley_road": expected for a deck IN the road.
            e = item("Checkpoint", "Weighbridge", A["wb_deck"], DECK[0], DECK[1], DECK[2], [], top_flush=True)
        out.append(e)
    out.append(item("Checkpoint", "BoomBarrier", A["wb_boom"], *BOOM, [], no_collision=True))
    return out


def border(item):
    """One sites-script entry per stone / the snapped post (labels ORE_Site_Border_*)."""
    data = json.load(open(BORDER_JSON))
    out = []
    for s in data["items"]:
        name = s["label"].split("ORE_Site_Border_", 1)[-1]
        out.append(item("Border", name, s["mesh"], s["x"], s["y"], s["yaw"], [], no_collision=True, embed=3.0))
    return out


def seat_boom(pss, label="ORE_Site_Checkpoint_BoomBarrier"):
    """place_item seats an actor on the LOWEST ground of its whole footprint (here 10.4 m across the road and its
    north ditch). Re-seat the post on its own ground, lift / drop the rest post to its ground and pitch the arm so the
    tip lies on the rest fork. Editor python on the open map; returns (post z, rest dz, arm pitch)."""
    actor = pss.own_actors().get(label)
    if actor is None:
        raise RuntimeError(label + " not placed")
    loc, yaw = actor.get_actor_location(), math.radians(actor.get_actor_rotation().yaw)
    gp = pss.ground(loc.x, loc.y)
    rx, ry = loc.x + 1040.0 * math.cos(yaw), loc.y + 1040.0 * math.sin(yaw)
    gr = pss.ground(rx, ry)
    if gp is None or gr is None:
        raise RuntimeError("no landscape under the boom")
    z = gp - 5.0
    actor.set_actor_location(pss.unreal.Vector(loc.x, loc.y, z), False, True)
    dz = (gr - 5.0) - z
    pitch = math.degrees(math.atan2(dz, 1040.0))
    for comp in actor.get_components_by_class(pss.unreal.StaticMeshComponent):
        if comp.get_name() == "Rest":
            comp.set_editor_property("relative_location", pss.unreal.Vector(1040.0, 0.0, dz))
        elif comp.get_name() == "Arm":
            comp.set_editor_property("relative_rotation", pss.unreal.Rotator(0.0, pitch, 0.0))
    return z, dz, pitch


def seat_deck(pss, label="ORE_Site_Checkpoint_Weighbridge", lift_cm=1.0):
    """top_flush seats the flat deck on the highest road point; on Ore_height.raw the road under it falls ~18 cm along
    and ~20 cm across (32 cm range), which leaves a lip at the east (entry) end. Fit a plane to traced ground over the
    footprint (6 x 13 samples), tilt the deck onto it and lift it so the top clears the highest residual by lift_cm
    (heightfield fit: residuals -3.5..+11.6 cm, so the worst gap drops from ~32 to ~15 cm). Flattening the road under
    the deck in the landscape would remove the rest. Returns (pitch, roll, max residual cm)."""
    U = pss.unreal
    actor = pss.own_actors().get(label)
    if actor is None:
        raise RuntimeError(label + " not placed")
    loc = actor.get_actor_location()
    yaw = math.radians(actor.get_actor_rotation().yaw)
    cs, sn = math.cos(yaw), math.sin(yaw)
    pts = []
    for i in range(6):
        for j in range(13):
            lx, ly = -230.0 + i * 92.0, -930.0 + j * 155.0
            z = pss.ground(loc.x + lx * cs - ly * sn, loc.y + lx * sn + ly * cs)
            if z is not None:
                pts.append((lx, ly, z))
    if len(pts) < 20:
        raise RuntimeError("deck footprint not over the landscape")
    n = len(pts)
    mx, my, mz = (sum(p[k] for p in pts) / n for k in range(3))
    sxx = sum((p[0] - mx) ** 2 for p in pts)
    syy = sum((p[1] - my) ** 2 for p in pts)
    sxy = sum((p[0] - mx) * (p[1] - my) for p in pts)
    sxz = sum((p[0] - mx) * (p[2] - mz) for p in pts)
    syz = sum((p[1] - my) * (p[2] - mz) for p in pts)
    det = sxx * syy - sxy * sxy
    a = (sxz * syy - syz * sxy) / det           # dz / d local X
    b = (syz * sxx - sxz * sxy) / det           # dz / d local Y
    c = mz - a * mx - b * my                    # plane height at the deck centre
    res = max(p[2] - (a * p[0] + b * p[1] + c) for p in pts)
    xa = U.Vector(cs, sn, a)
    ya = U.Vector(-sn, cs, b)
    rot = U.MathLibrary.make_rot_from_xy(xa, ya)
    actor.set_actor_location_and_rotation(U.Vector(loc.x, loc.y, c + res + lift_cm), rot, False, True)
    return rot.pitch, rot.roll, round(res, 1)
