"""Turns one economy sim run (Saved/EconomySim/<run>) into measured rates and a what-if economy model.

    python analyze_economy.py <run dir> [--out analysis.md]

Reads timeline.csv, harvest_drops.csv, enemy_units.csv, events.csv and summary.md written by the RTS_EconomySim
module, plus the enemy training point costs from Source/RTS_Survival/.../EnemyTrainingHelpers.cpp. Changes nothing in
the game; the options at the end are a model fed with the measured rates, for discussion only.
"""
import argparse
import csv
import os
import re
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.normpath(os.path.join(HERE, "..", "..", "RTS_Survival"))
ENEMY_COSTS_CPP = os.path.join(SOURCE, "Enemy", "TrainingAndUnitCreation", "EnemyTrainingHelpers",
                               "EnemyTrainingHelpers.cpp")

# The design note in DeveloperSettings::GameBalance::UnitCosts: metal is twice as rare as radixite, vehicle parts
# three times. Radixite-equivalent value (RE) = R + 2 M + 3 VP.
METAL_WEIGHT = 2.0
PARTS_WEIGHT = 3.0

# Survival Normal (SurvivalWaveDefaults, SurvivalWaveSettings.cpp): authored enemy pressure per minute.
SURVIVAL_WAVE_INTERVAL = [(0, 100), (5, 90), (10, 80), (20, 70), (40, 60), (60, 55)]
SURVIVAL_WAVE_BUDGET = [(0, 30), (5, 60), (10, 100), (20, 190), (30, 300), (40, 420), (60, 680)]
SURVIVAL_TRAINING = [(0, 4), (10, 8), (20, 12), (40, 18), (60, 24)]

# Representative player purchases (R, M, VP) from CPPGameState / DeveloperSettings (Strike Division and Breakthrough).
UNITS = [
    ("Pz II F (light)", 200, 0, 50),
    ("Puma (armoured car)", 180, 0, 80),
    ("StuG III (medium TD)", 300, 0, 125),
    ("Pz III J", 250, 0, 100),
    ("Pz IV G / Pz III M (medium)", 400, 0, 200),
    ("Pz IV H", 500, 0, 300),
    ("Jagdpanzer IV/70", 475, 0, 250),
    ("Panther D", 600, 0, 350),
    ("Tiger H1", 900, 0, 450),
    ("King Tiger", 1300, 0, 650),
    ("E-100 / Maus", 1500, 0, 1000),
    ("Pak 40 pit (defense)", 380, 200, 0),
    ("Twin MG42 pit (defense)", 320, 140, 0),
    ("Flak 37 pit (defense)", 420, 220, 0),
    ("Scavengers (infantry)", 250, 0, 0),
    ("Panzergrenadiere (infantry)", 675, 200, 0),
]

# Enemy units whose class is roughly a German counterpart, for the RE per training point exchange rate.
ENEMY_EQUIVALENTS = {
    "Tank_T26": ("Pz II F", 200 + 3 * 50),
    "Tank_BT7": ("Pz II F", 200 + 3 * 50),
    "Tank_T70": ("Pz II F", 200 + 3 * 50),
    "Tank_SU_76": ("Marder", 180 + 3 * 80),
    "Tank_T34_76": ("Pz IV G", 400 + 3 * 200),
    "Tank_T34_85": ("Pz IV H", 500 + 3 * 300),
    "Tank_SU_85": ("Jagdpanzer IV/70", 475 + 3 * 250),
    "Tank_SU_100": ("Jagdpanzer IV/70", 475 + 3 * 250),
    "Tank_KV_1": ("Tiger H1", 900 + 3 * 450),
    "Tank_IS_1": ("Tiger H1", 900 + 3 * 450),
    "Tank_IS_2": ("King Tiger", 1300 + 3 * 650),
}


def lerp_curve(keys, minute):
    for index in range(1, len(keys)):
        if minute <= keys[index][0]:
            (m0, v0), (m1, v1) = keys[index - 1], keys[index]
            return v0 + (v1 - v0) * (minute - m0) / (m1 - m0)
    return keys[-1][1]


def survival_points_per_minute(minute):
    waves = lerp_curve(SURVIVAL_WAVE_BUDGET, minute) * 60.0 / lerp_curve(SURVIVAL_WAVE_INTERVAL, minute)
    return waves, lerp_curve(SURVIVAL_TRAINING, minute)


def parse_enemy_costs(path):
    """Reads the GetTankTrainingPointsCost / GetSquadTrainingPointCost switches into {subtype: points}."""
    costs = {}
    if not os.path.exists(path):
        return costs
    pending = []
    for line in open(path, encoding="utf-8-sig"):
        case = re.search(r"case E(?:Tank|Squad)Subtype::(\w+):", line)
        if case:
            pending.append(case.group(1))
            continue
        ret = re.search(r"return (\d+);", line)
        if ret and pending:
            for name in pending:
                costs[name] = int(ret.group(1))
            pending = []
        elif "default:" in line:
            pending = []
    return costs


def read_csv(path):
    if not os.path.exists(path):
        return []
    with open(path, newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def fnum(row, key):
    try:
        return float(row[key])
    except (KeyError, ValueError, TypeError):
        return 0.0


def slope(rows, key, t0, t1):
    """Per-minute growth of a cumulative column between t0 and t1 (seconds)."""
    inside = [r for r in rows if t0 <= fnum(r, "t") <= t1]
    if len(inside) < 2:
        return 0.0
    first, last = inside[0], inside[-1]
    dt = fnum(last, "t") - fnum(first, "t")
    return (fnum(last, key) - fnum(first, key)) * 60.0 / dt if dt > 0 else 0.0


def minutes_to_afford(cost, income):
    """Minutes to save cost (R, M, VP) from per-minute income (R, M, VP); inf when a needed income is <= 0."""
    worst = 0.0
    for need, rate in zip(cost, income):
        if need <= 0:
            continue
        if rate <= 0:
            return float("inf")
        worst = max(worst, need / rate)
    return worst


def fmt_minutes(value):
    return "never" if value == float("inf") else "%.1f" % value


def analyse(run_dir):
    timeline = read_csv(os.path.join(run_dir, "timeline.csv"))
    drops = read_csv(os.path.join(run_dir, "harvest_drops.csv"))
    enemy = read_csv(os.path.join(run_dir, "enemy_units.csv"))
    events = read_csv(os.path.join(run_dir, "events.csv"))
    enemy_costs = parse_enemy_costs(ENEMY_COSTS_CPP)
    out = ["# Economy sim analysis", "", "Run: `%s`" % run_dir, ""]
    if not timeline:
        out.append("No timeline rows: the run did not get past setup.")
        return "\n".join(out)
    end_t = fnum(timeline[-1], "t")
    done_events = [e for e in events if e["kind"] == "build_order_done"]
    build_done_t = fnum(done_events[0], "t") if done_events else None

    out += ["## Build order (real queue, real payments)", "", "| t (s) | event |", "|---|---|"]
    for e in events:
        if e["kind"] in ("trained", "step_done", "bxp_placed", "deploy_ordered", "build_order_done", "warning"):
            out.append("| %s | %s %s |" % (e["t"], e["kind"], e["detail"][:140]))
    out.append("")

    # ---- Harvesters ----
    per_harvester = defaultdict(lambda: {"field": "", "R": 0, "M": 0, "cycles": [], "first": None, "last": None,
                                         "after_first": 0})
    for d in drops:
        h = per_harvester[d["harvester"]]
        h["field"] = d["field"]
        amount = int(float(d["amount"]))
        h[d["resource"]] = h.get(d["resource"], 0) + amount
        t = fnum(d, "t")
        if h["first"] is None:
            h["first"] = t
        else:
            # Everything dropped after the first drop was earned in (first, last]: an unbiased rate.
            h["after_first"] += amount
        h["last"] = t
        if fnum(d, "cycle_s") > 0:
            h["cycles"].append(fnum(d, "cycle_s"))
    out += ["## Harvesters (measured from cargo)", "",
            "| # | field | drops | median cycle (s) | delivered R | delivered M | per minute (first to last drop) |",
            "|---|---|---|---|---|---|---|"]
    field_rates = defaultdict(list)
    for key in sorted(per_harvester, key=int):
        h = per_harvester[key]
        cycles = sorted(h["cycles"])
        median = cycles[len(cycles) // 2] if cycles else 0.0
        span = (h["last"] - h["first"]) if h["first"] is not None else 0.0
        rate = h["after_first"] * 60.0 / span if span > 0 else 0.0
        field_rates[h["field"]].append(rate)
        out.append("| %s | %s | %d | %.1f | %d | %d | %.1f |" % (key, h["field"], len(cycles) + 1, median, h.get("R", 0),
                                                                 h.get("M", 0), rate))
    out.append("")

    # ---- Steady state ----
    # The spawned fields run dry during a long run; take the best 5-minute window after the build order, scored by
    # harvested radixite-equivalent, so depletion and late stalls do not hide what the base earns while it is fed.
    t_start = build_done_t if build_done_t is not None else end_t * 0.5
    window = 300.0
    best_t0, best_score = t_start, -1.0
    for row in timeline:
        candidate = fnum(row, "t")
        if candidate < t_start or candidate + window > end_t:
            continue
        score = slope(timeline, "harvested_R", candidate, candidate + window) + METAL_WEIGHT * slope(
            timeline, "harvested_M", candidate, candidate + window)
        if score > best_score:
            best_t0, best_score = candidate, score
    t0, t1 = best_t0, min(end_t, best_t0 + window)
    harvested_r = slope(timeline, "harvested_R", t0, t1)
    harvested_m = slope(timeline, "harvested_M", t0, t1)
    parts = slope(timeline, "bank_VP", t0, t1)
    bank_r = slope(timeline, "bank_R", t0, t1) + slope(timeline, "R", t0, t1)
    bank_m = slope(timeline, "bank_M", t0, t1) + slope(timeline, "M", t0, t1)
    run_end_t = end_t
    end_t = t1
    harvesters = int(fnum(timeline[-1], "harvesters"))
    out += ["## Steady state: best 5-minute window after the build order (t = %.0f to %.0f s)" % (t0, t1), "",
            "| per minute | value |", "|---|---|",
            "| radixite harvested | %.0f |" % harvested_r,
            "| metal harvested | %.0f |" % harvested_m,
            "| vehicle parts produced (all converters) | %.1f |" % parts,
            "| radixite left over after conversion (banked) | %.0f |" % bank_r,
            "| metal left over after conversion (banked) | %.0f |" % bank_m,
            "| harvesters | %d |" % harvesters,
            "| converters on | %d |" % int(fnum(timeline[-1], "converters_on")), ""]

    # ---- Enemy ----
    # "trained"/"spawned": strategic AI (or anything not next to a mission spawn point); "wave": mission wave points.
    spawned = [e for e in enemy if e["origin"] in ("spawned", "trained")]
    wave_spawned = [e for e in enemy if e["origin"] == "wave"]
    by_minute = defaultdict(lambda: [0, 0, 0.0])
    tiers = Counter()
    for e in spawned:
        minute = int(fnum(e, "t") // 60)
        subtype = e["subtype"].split("::")[-1]
        points = enemy_costs.get(subtype, 10)
        by_minute[minute][0 if e["unit_type"].endswith("Tank") else 1] += 1
        by_minute[minute][2] += points
        tiers[subtype] += 1
    total_points = sum(v[2] for v in by_minute.values())
    out += ["## Enemy (strategic AI with the survival Normal training income, mission waves starved)", "",
            "- trained: %d tanks, %d squads, %d training points in %.1f min" % (
                sum(v[0] for v in by_minute.values()), sum(v[1] for v in by_minute.values()), total_points,
                run_end_t / 60.0),
            "- mix: " + ", ".join("%s x%d" % (k, v) for k, v in tiers.most_common(12)),
            "- next to mission wave spawn points (not counted above): %d" % len(wave_spawned),
            "- per minute: " + ", ".join("m%d %d tanks/%d squads/%d pts" % (m, v[0], v[1], v[2])
                                         for m, v in sorted(by_minute.items())),
            "- damage the protection absorbed (what the enemy would have dealt): %.0f" % fnum(timeline[-1],
                                                                                          "damage_absorbed"),
            ""]
    rate_samples = []
    for subtype, (name, value) in ENEMY_EQUIVALENTS.items():
        if subtype in enemy_costs:
            rate_samples.append(value / enemy_costs[subtype])
    re_per_point = sorted(rate_samples)[len(rate_samples) // 2] if rate_samples else 45.0
    out += ["Exchange rate: a training point buys about **%.0f RE** of German counterpart (median over %d matched "
            "tank pairs; RE = R + 2 M + 3 VP)." % (re_per_point, len(rate_samples)), ""]

    # ---- Pressure vs income ----
    income_re = harvested_r + METAL_WEIGHT * harvested_m
    # Conversion moves value from R/M into VP; the RE of the bundle after conversion:
    spend_re = bank_r + METAL_WEIGHT * bank_m + PARTS_WEIGHT * parts
    out += ["## Pressure versus income", "",
            "Player steady-state income: %.0f RE/min as harvested, %.0f RE/min after conversion." % (income_re, spend_re),
            "", "| minute | survival waves (pts/min) | strategic training (pts/min) | enemy RE/min | player/enemy |",
            "|---|---|---|---|---|"]
    for minute in (0, 5, 10, 15, 20, 30):
        waves, training = survival_points_per_minute(minute)
        enemy_re = (waves + training) * re_per_point
        out.append("| %d | %.0f | %.0f | %.0f | %.2f |" % (minute, waves, training, enemy_re,
                                                          spend_re / enemy_re if enemy_re else 0.0))
    out.append("")

    # ---- Affordability ----
    income = (bank_r, bank_m, parts)
    out += ["## Time to afford one unit from the steady-state surplus (no scavenging)", "",
            "Surplus per minute: R %.0f, M %.0f, VP %.1f." % income, "",
            "| unit | R | M | VP | minutes |", "|---|---|---|---|---|"]
    for name, r, m, vp in UNITS:
        out.append("| %s | %d | %d | %d | %s |" % (name, r, m, vp, fmt_minutes(minutes_to_afford((r, m, vp), income))))
    out.append("")
    radixite_harvesters = sum(1 for h in per_harvester.values() if h.get("R", 0) > 0) or 1
    metal_harvesters = sum(1 for h in per_harvester.values() if h.get("M", 0) > 0) or 1
    sustained = (harvested_r / radixite_harvesters, harvested_m / metal_harvesters)
    out += economy_model(field_rates, re_per_point, sustained)
    return "\n".join(out)


# ---------------------------------------------------------------------------------------------------------------
# What-if model, fed with the measured harvester rates. Nothing here changes the game.

CONVERTER_BASE = {"R_in": 150.0, "VP_out": 30.0}   # Ger_Ref_Converter: 25 R -> 5 VP every 10 s
SMELTER_BASE = {"M_in": 150.0, "VP_out": 30.0}     # Ger_Bxp_MetalVaultSmelter: 25 M -> 5 VP every 10 s

OPTIONS = [
    # name, converter (R in, VP out per min), smelter (M in, VP out per min), VP cost multiplier
    ("Today: 5 R -> 1 VP, 30 VP/min", (150, 30), (150, 30), 1.0),
    ("A: converter 3:1 (15 R -> 5 VP / 10 s)", (90, 30), (150, 30), 1.0),
    ("B: converter 3:1 at 45 VP/min (15 R -> 5 VP / 6.7 s)", (135, 45), (150, 30), 1.0),
    ("C: A + smelter 2.5:1 (25 M -> 10 VP / 10 s)", (90, 30), (150, 60), 1.0),
    ("D: today's converter, vehicle VP costs x0.6", (150, 30), (150, 30), 0.6),
    ("E: today's ratio, converter twice as fast (60 VP/min)", (300, 60), (150, 30), 1.0),
]

CONVERTERS_PER_REFINERY = 2

REFERENCE_UNITS = [("Pz IV G (medium)", 400, 200), ("Panther D", 600, 350), ("Tiger H1", 900, 450)]


def best_mix(income_r, income_m, converter, smelter, unit_r, unit_vp, defence_r, defence_m, max_converters,
             smelters):
    """Tanks per minute when radixite is split optimally between the tank's own R and converters feeding its VP.

    Converters are capped at max_converters (one per refinery), smelters run if metal allows. Returns
    (tanks per minute, converters used, R/min left, M/min left)."""
    r_free = income_r - defence_r
    m_free = income_m - defence_m
    vp_from_smelters = 0.0
    smelters_used = 0
    for _ in range(smelters):
        if m_free >= smelter[0]:
            m_free -= smelter[0]
            vp_from_smelters += smelter[1]
            smelters_used += 1
    best = (0.0, 0, r_free, m_free)
    for converters in range(0, max_converters + 1):
        r_left = r_free - converters * converter[0]
        if r_left < 0:
            break
        vp = converters * converter[1] + vp_from_smelters
        tanks = min(r_left / unit_r, vp / unit_vp) if unit_vp > 0 else r_left / unit_r
        if tanks > best[0]:
            best = (tanks, converters, r_left - tanks * unit_r, m_free)
    return best


def economy_model(field_rates, re_per_point, sustained):
    out = ["## What-if model (fed with the measured harvester rates)", ""]
    near = [r for r in field_rates.get("FieldB", []) if r > 0]
    far = [r for r in field_rates.get("FieldA", []) if r > 0]
    metal = [r for r in field_rates.get("Metal", []) if r > 0]
    r_near = sum(near) / len(near) if near else 280.0
    r_far = sum(far) / len(far) if far else 185.0
    m_rate = sum(metal) / len(metal) if metal else 105.0
    out += ["Pz I harvester while fed: %.0f R/min next to a refinery, %.0f R/min 30-45 m from the HQ, %.0f M/min on "
            "metal." % (r_near, r_far, m_rate),
            "Sustained in the steady-state window (fields thinning, re-routing, bombing): %.0f R/min per radixite "
            "harvester, %.0f M/min per metal harvester. The tables use the sustained rates." % sustained, ""]
    # The model runs on sustained rates: a field is not fed forever in a real game.
    r_near = r_far = sustained[0]
    m_rate = sustained[1]
    # Reference mid-game bases: (label, radixite harvesters near/far, metal harvesters, refineries, vaults with smelter)
    bases = [("Early (4 R + 1 M harvesters, 1 refinery, 1 vault)", 2, 2, 1, 1, 1),
             ("Mid (6 R + 2 M harvesters, 2 refineries, 1 vault)", 4, 2, 2, 2, 1),
             ("Late (8 R + 3 M harvesters, 3 refineries, 2 vaults)", 6, 2, 3, 3, 2)]
    defence_r, defence_m = 150.0, 75.0
    out += ["Each base keeps %d R + %d M per minute for defences (about one Pak 40 pit every 2.5 min) and spends the "
            "rest on one tank type, with radixite split optimally between the tank and converters." % (defence_r,
                                                                                                    defence_m), ""]
    for unit_name, unit_r, unit_vp in REFERENCE_UNITS:
        out += ["### %s (%d R + %d VP)" % (unit_name, unit_r, unit_vp), "",
                "| option | " + " | ".join(b[0] for b in bases) + " |",
                "|---|" + "---|" * len(bases)]
        for name, converter, smelter, vp_mlt in OPTIONS:
            cells = []
            for label, n_near, n_far, n_metal, refineries, vaults in bases:
                income_r = n_near * r_near + n_far * r_far
                income_m = n_metal * m_rate
                # SM_OpelRefinery_BuildingMesh has two expansion sockets (BXP1, BXP2): two converters per refinery.
                tanks, used, spare_r, _ = best_mix(income_r, income_m, converter, smelter, unit_r, unit_vp * vp_mlt,
                                                   defence_r, defence_m, CONVERTERS_PER_REFINERY * refineries, vaults)
                cells.append("1 per %s min, %+.0f R/min spare" % (fmt_minutes(1.0 / tanks) if tanks else "never",
                                                                  spare_r))
            out.append("| %s | %s |" % (name, " | ".join(cells)))
        out.append("")
    out += ["### Effective radixite price (unit R + R burnt in converters for its VP)", "",
            "| unit | today (5:1) | 3:1 converter | VP share today |", "|---|---|---|---|"]
    for name, r, m, vp in UNITS:
        if vp <= 0:
            continue
        today = r + 5 * vp
        out.append("| %s | %d | %d | %.0f%% |" % (name, today, r + 3 * vp, 100.0 * 5 * vp / today))
    out.append("")
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("run_dir")
    parser.add_argument("--out", default="")
    args = parser.parse_args()
    text = analyse(args.run_dir)
    out_path = args.out or os.path.join(args.run_dir, "analysis.md")
    with open(out_path, "w", encoding="utf-8") as handle:
        handle.write(text + "\n")
    print(text)


if __name__ == "__main__":
    main()
