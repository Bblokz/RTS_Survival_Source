"""Summarises fire feedback per team weapon from -TWQualityCrewAnims runs (RTS_TWQC recoil_setup / recoil_kick lines).

Usage: python analyse_recoil.py <log> [<log> ...]   (Saved/Logs/TWCrewAnims_<label>.log)
Per gun: the feedback settings that matter, then the worst (largest magnitude) kick values over all shots, measured in
the barrel's frame: back along the barrel, lateral (sideways), roll about the barrel. Sideways = lateral + roll.
"""
import re
import sys
from collections import defaultdict

SETTING_KEYS = ("M_TranslationStiffness", "M_TranslationDamping", "M_RotationStiffness", "M_RotationDamping",
                "M_MaxBackCm", "M_MaxPitchDeg", "M_MaxCalibreForNormalization", "M_MaxSideSwayCm",
                "M_MaxOptionalRollDeg", "bM_EnableSideSway", "bM_EnableOptionalRoll", "M_OptionalRollChannel")
KICK_KEYS = ("back_cm", "lateral_cm", "pitch_deg", "roll_deg", "yaw_deg", "muzzle_lateral_cm")
# A kick window in which the gun turned more than this was the gun traversing or driving off, not recoil.
MAX_RECOIL_YAW_DEG = 3.0


def short_name(subject):
    return subject.replace("Squad_", "")


def main(paths):
    settings = {}
    kicks = defaultdict(list)
    for path in paths:
        for line in open(path, encoding="utf-8", errors="replace"):
            if "recoil_setup" in line:
                subject = re.search(r"subject=(\S+)", line).group(1)
                values = dict(re.findall(r"(\w+)=([^,()\s]+)", line))
                settings[subject] = {key: values.get(key, "-") for key in SETTING_KEYS}
            elif "recoil_kick" in line:
                values = dict(re.findall(r"(\w+)=(-?[\d.]+)", line))
                subject = re.search(r"subject=(\S+)", line).group(1)
                kicks[(path.split("_")[-1].replace(".log", ""), subject)].append(values)
    print("settings (stiffness/damping translation | rotation, back cm, pitch deg, calibre norm, sway cm, roll deg):")
    for subject, value in sorted(settings.items()):
        print(f"  {short_name(subject):20} {value['M_TranslationStiffness']:>6}/{value['M_TranslationDamping']:<5} "
              f"{value['M_RotationStiffness']:>6}/{value['M_RotationDamping']:<5} back={value['M_MaxBackCm']:<6} "
              f"pitch={value['M_MaxPitchDeg']:<5} norm={value['M_MaxCalibreForNormalization']:<4} "
              f"sway={value['M_MaxSideSwayCm']}({value['bM_EnableSideSway']}) "
              f"roll={value['M_MaxOptionalRollDeg']}({value['bM_EnableOptionalRoll']},{value['M_OptionalRollChannel']})")
    print(f"\n{'run':8} {'gun':20} {'shots':>5} {'yaw':>6} " + " ".join(f"{key:>17}" for key in KICK_KEYS))
    for (run, subject), shots in sorted(kicks.items()):
        moved = [shot for shot in shots if abs(float(shot.get("back_cm", 0))) + abs(float(shot.get("lateral_cm", 0)))
                 + abs(float(shot.get("pitch_deg", 0))) > 0.01]
        real = [shot for shot in moved if abs(float(shot.get("yaw_deg", 0))) <= MAX_RECOIL_YAW_DEG]
        skipped = len(moved) - len(real)
        if not real:
            print(f"{run:8} {short_name(subject):20} {len(shots):>5} no movement")
            continue
        yaw = float(real[0].get("barrel_yaw_world", 0))
        worst = []
        for key in KICK_KEYS:
            values = [float(shot.get(key, 0)) for shot in real]
            worst.append(max(values, key=abs))
        print(f"{run:8} {short_name(subject):20} {len(real):>5} {yaw:6.0f} " + " ".join(f"{value:17.2f}" for value in worst)
              + (f"  (+{skipped} turning)" if skipped else ""))


if __name__ == "__main__":
    main(sys.argv[1:])
