"""Compact pass/fail view of berlin_nav_probe for Ger_Berlin: prints only what deviates from the design.

    import berlin_nav_check; berlin_nav_check.run()      # in an editor with the map open and the navmesh built
"""
import contextlib
import importlib
import io

import unreal

import berlin_map_build as bmb
import berlin_nav_probe

ORDER = ("Character", "LightTank", "MediumTank", "HeavyTank")


def expected(vehicle_class):
    """Which agents may pass an opening directly: infantry only, small (+ infantry), or all."""
    if vehicle_class.startswith("infantry"):
        return (True, False, False, False)
    if vehicle_class.startswith("small"):
        return (True, True, False, False)
    return (True, True, True, True)


def run():
    world = bmb.world()
    if unreal.NavigationSystemV1.get_navigation_system(world).is_navigation_being_built(world):
        print("NAV STILL BUILDING")
        return None
    importlib.reload(berlin_nav_probe)
    with contextlib.redirect_stdout(io.StringIO()):
        result = berlin_nav_probe.run()
    problems = []
    for key, value in result.items():
        if key.startswith("lane ") and not isinstance(value, str):
            for leg, row in value:
                # legs through a small-vehicle gap or over the bridge are meant to stop medium and heavy
                all_classes = "north_lane" in key or "central_lane" in key     # these two carry every class end to end
                if row["Character"] != "ok" or row["LightTank"] != "ok" or \
                        (all_classes and any(row[a] != "ok" for a in ORDER)):
                    if "flank_spur" in key and leg.startswith("4"):
                        continue            # the Hbf shell wall: a detour through the gate is the design
                    problems.append("%s leg %s %s" % (key, leg.split(" ")[0], row))
        elif key.startswith("gap ") and "sink_gap_all" not in key:
            vehicle_class = key[key.index("(") + 1:key.rindex(")")]
            want = expected(vehicle_class)
            got = tuple(value[a] == "direct" for a in ORDER)
            name = key.split(" ")[1]
            if name == "gap_flank_inf":
                continue                    # 15 m from the open west end of that wall: a short way round always exists
            if got != want:
                problems.append("%s want %s got %s" % (key, want, [value[a] for a in ORDER]))
        elif key in ("zoo->exit_west", "zoo->exit_sw", "hbf->exit_nw", "sink_all_crossing", "zoo_approach->bunker"):
            if any(value[a] is None for a in ORDER):
                problems.append("%s %s" % (key, value))
    print("NAV PROBLEMS %d" % len(problems))
    for line in problems:
        print("  " + line)
    return problems
