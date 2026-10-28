"""Summarises -TWQualityCrewAnims runs into per gun findings with the numbers behind them.

Usage: python analyse_crew_anims.py <crew txt> [<crew txt> ...]   (Saved/TWCrewAnims/<label>_crew.txt)
Prints, per subject: the montage setup (clip, length, stretched rate), then every flagged problem with the segment
numbers that triggered it. Thresholds are guesses tuned on the PaK 40; read flags as pointers, not verdicts.
"""
import re
import sys
from collections import defaultdict

SWITCH_POP_CM = 20.0
MID_POP_CM = 15.0
SNAP_YAW_DEG = 30.0
SNAP_MOVE_CM = 30.0
SLOW_RATE = 0.75
FAST_RATE = 1.35
HAND_FAR_CM = 20.0
INSIDE_FRACTION = 0.3
FOOT_SLIDE_CM = 8.0
IDLE_GAP_S = 1.0


def parse_fields(line):
    fields = {}
    for match in re.finditer(r"(\w+)=(\[[^\]]*\]|\{[^}]*\}|\S+)", line):
        fields[match.group(1)] = match.group(2)
    for key in ("rate", "hand_l", "hand_r", "foot_slide_cmps", "foot_h"):
        match = re.search(key + r"\[([^\]]*)\]", line)
        if match:
            fields[key] = match.group(1)
    return fields


def stat(text, name):
    match = re.search(name + r"=(-?[\d.]+)", text or "")
    return float(match.group(1)) if match else None


def analyse(paths):
    setup = defaultdict(list)
    segments = defaultdict(list)
    events = defaultdict(list)
    for path in paths:
        for line in open(path, encoding="utf-8", errors="replace"):
            line = line.strip()
            match = re.search(r"subject=(\S+)", line)
            if not match:
                if "RESULT" in line:
                    events["run"].append(line)
                continue
            subject = match.group(1)
            if "crew_setup" in line:
                setup[subject].append(line.split("crew_setup ", 1)[1])
            elif "crew_seg" in line:
                segments[subject].append(parse_fields(line))
            elif any(tag in line for tag in ("crew_post", "check=FAIL", "fire ", "fire_done", "fire_forced",
                                             "crew_reload", "crew_gun", "crew_state")):
                events[subject].append(line.split("RTS_TWQC ", 1)[-1])
    for subject in sorted(set(setup) | set(segments)):
        print(f"\n===== {subject}")
        for line in setup[subject]:
            print("  setup:", re.sub(r"subject=\S+ ", "", line))
        for line in events[subject]:
            if any(tag in line for tag in ("crew_post", "check=FAIL", "fire_done", "fire_forced")):
                print("  event:", line)
        reloads = [line for line in events[subject] if "crew_reload" in line and "end=" in line]
        if reloads:
            print("  reloads:", "; ".join(re.search(r"seconds=([\d.]+)", r).group(1) + "s" for r in reloads))
        for flag in flag_segments(segments[subject]):
            print("  FLAG", flag)


def flag_segments(segments):
    flags = []
    for seg in segments:
        kind = seg.get("kind", "Other")
        montage = seg.get("montage", "none")
        who = f"op{seg.get('op')} {seg.get('role')}/{seg.get('side')} {montage} [{kind}] t={seg.get('t')}"
        duration = float(seg.get("dur", 0))
        length = float(seg.get("len", 0))
        end_pos = float(seg.get("end_pos", 0))
        states = seg.get("state", "")
        samples = max(1, int(seg.get("samples", 1)))
        is_crew_clip = kind != "Other" and montage != "none"
        switch_pop = float(seg.get("switch_pop_cm", 0))
        if switch_pop > SWITCH_POP_CM and (is_crew_clip or "Pack" in seg.get("switch_from", "") or
                                           "Deploy" in seg.get("switch_from", "") or "TW_" in seg.get(
                    "switch_from", "")):
            flags.append(f"pose pop {switch_pop:.0f}cm in one frame switching {seg.get('switch_from')} -> {montage}: {who}")
        snap_yaw = float(seg.get("snap_yaw", 0))
        snap_move = float(seg.get("snap_move", 0))
        if is_crew_clip and (snap_yaw > SNAP_YAW_DEG or snap_move > SNAP_MOVE_CM):
            flags.append(f"actor snapped {snap_move:.0f}cm/{snap_yaw:.0f}deg in one frame as the clip started: {who}")
        mid_pop = seg.get("mid_pop_cm", "0@0").split("@")
        if is_crew_clip and float(mid_pop[0]) > MID_POP_CM:
            flags.append(f"bone jumps {float(mid_pop[0]):.0f}cm in one frame at clip time {mid_pop[1]}s "
                         f"(rate {stat(seg.get('rate'), 'max')}): {who}")
        if kind.startswith(("Deploy", "Pack")) and length > 0 and end_pos < 0.75 * length and "->Ready" not in states:
            flags.append(f"transition cut at {end_pos:.2f}/{length:.2f}s after {duration:.2f}s, state {states}: {who}")
        rate = stat(seg.get("rate"), "mean")
        if is_crew_clip and rate is not None and (rate < SLOW_RATE or rate > FAST_RATE):
            flags.append(f"played at rate {rate:.2f} (clip {length:.1f}s over {duration:.1f}s): {who}")
        if kind.startswith("React"):
            for hand in ("hand_l", "hand_r"):
                low = stat(seg.get(hand), "min")
                if low is not None and low > HAND_FAR_CM:
                    flags.append(f"{hand} never closer than {low:.0f}cm to the gun during the reload "
                                 f"(mean {stat(seg.get(hand), 'mean'):.0f}cm): {who}")
        if is_crew_clip:
            inside = re.findall(r"(\w+):(\d+)", seg.get("inside", ""))
            deep = [f"{bone} {int(count) / samples:.0%}" for bone, count in inside if int(count) / samples > INSIDE_FRACTION]
            if deep:
                flags.append(f"inside gun collision: {', '.join(deep)}: {who}")
            slide = float(seg.get("slide_cm", 0))
            if slide > FOOT_SLIDE_CM and float(seg.get("capsule_travel", 0)) < 5:
                flags.append(f"planted feet skate {slide:.0f}cm over the clip "
                             f"(max {stat(seg.get('foot_slide_cmps'), 'max'):.0f}cm/s): {who}")
        if montage == "none" and "Ready_Deployed->Ready_Deployed" in states and duration > IDLE_GAP_S and \
                seg.get("role") not in (None, "None"):
            flags.append(f"plain idle for {duration:.1f}s while deployed (no crew clip): {who}")
    return flags


if __name__ == "__main__":
    analyse(sys.argv[1:])
