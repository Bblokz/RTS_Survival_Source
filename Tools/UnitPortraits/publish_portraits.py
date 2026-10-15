"""Copies the picked 256x256 portraits into the TrainingUI NoIcon source folders.

picks file, one line per unit:  <portrait name>=<cropped picture file name>
  e.g. T_Ger_HeavyTank_E25=T_Ger_HeavyTank_E25_FrontRight_shot2_t2.png
The portrait name prefix picks the folder (see FOLDER_BY_PREFIX). Existing portraits are never replaced unless
--overwrite is given.

Usage:
  python publish_portraits.py <picks file> <cropped folder> [--icons-root D:/UE_Assets/Icons/TrainingUI] [--overwrite]
"""
import argparse
import shutil
from pathlib import Path

FOLDER_BY_PREFIX = {
    "T_Ger_ArmoredCar_": "GerTanks/ArmoredCars/NoIcon",
    "T_Ger_LightTank_": "GerTanks/Light/NoIcon",
    "T_Ger_MediumTank_": "GerTanks/Medium/NoIcon",
    "T_Ger_HeavyTank_": "GerTanks/Heavy/NoIcon",
    "T_Ger_Pit_": "GermanPits",
}


def read_picks(picks_file: Path) -> list:
    picks = []
    for line in picks_file.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            name, picture = (part.strip() for part in line.split("=", 1))
            picks.append((name, picture))
    return picks


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("picks_file", type=Path)
    parser.add_argument("cropped_folder", type=Path)
    parser.add_argument("--icons-root", type=Path, default=Path("D:/UE_Assets/Icons/TrainingUI"))
    parser.add_argument("--overwrite", action="store_true")
    arguments = parser.parse_args()
    for name, picture in read_picks(arguments.picks_file):
        folder = next((path for prefix, path in FOLDER_BY_PREFIX.items() if name.startswith(prefix)), None)
        source = arguments.cropped_folder / name / picture
        if folder is None or not source.exists():
            print(f"SKIP {name}: no folder for this prefix or {source} is missing")
            continue
        target = arguments.icons_root / folder / f"{name}.png"
        if target.exists() and not arguments.overwrite:
            print(f"KEEP {target} (exists; pass --overwrite to replace)")
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        print(f"WROTE {target} <- {picture}")


if __name__ == "__main__":
    main()
