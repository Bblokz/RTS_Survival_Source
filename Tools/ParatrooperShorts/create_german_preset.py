"""Create the separate two-bomber German battle from the approved meadow preset."""
import copy
import json
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[3]
JOBS = PROJECT / "ShortsCreator/Jobs"
BLUEPRINTS = "/Game/RTS_Survival/Blueprints/"


def unit(relative):
    name = relative.rsplit("/", 1)[-1]
    return BLUEPRINTS + relative + "." + name + "_C"


def point(x, y=0, z=0):
    return {"X": float(x), "Y": float(y), "Z": float(z)}


def main():
    job = copy.deepcopy(json.loads((JOBS / "SovietParatrooperBattle.json").read_text(encoding="utf-8-sig")))
    job.update(JobName="GermanTwinBomberParatrooperBattle", ResolutionX=1080, ResolutionY=1920, Fps=60)
    job.pop("EditorialOverlayRecipe", None)
    segment = job["Segments"][0]
    t34 = unit("GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T34E")
    t70 = unit("GroundVehicles/TrackBasedVehicle/Rus_Light/T70/Rus_Vehicle_T-70")
    is1 = unit("GroundVehicles/TrackBasedVehicle/Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-1")
    is2 = unit("GroundVehicles/TrackBasedVehicle/Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-2")
    ptrs = unit("Squads/Rus/Rus_Squad_PTRS")
    bs3 = unit("Buildings/BuildingExpansions/RusBS3Pit/Rus_BXP_BS3Pit")
    stug = unit("GroundVehicles/TrackBasedVehicle/Ger_Medium/Stug/Ger_Vehicle_StugIII")
    panzer = unit("GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_J")
    tiger = unit("GroundVehicles/TrackBasedVehicle/Ger_Heavy/TigerII/Ger_Vehicle_TigerII")
    scavengers = unit("Squads/Ger/Barracks_Infantry/Scavengers/Squad_Ger_Scavengers")
    flak = unit("Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Flak88")
    segment.update(
        Subtype="GermanTwinBomberParatrooperBattle",
        ClassPath=t34,
        ParatrooperAbility="GA_GerParatroopers",
        BattleAllyClassPaths=[t34] * 2 + [t70] * 3 + [is1, is2] + [ptrs] * 3 + [bs3],
        BattleAllyOffsets=[point(0, y) for y in [-1400, 1400]]
        + [point(-1300, y) for y in [-1750, 0, 1750]]
        + [point(-650, -850), point(-650, 850)]
        + [point(800, y) for y in [-1200, 0, 1200]] + [point(-2200)],
        BattleEnemyClassPaths=[stug] * 3 + [panzer] * 3 + [tiger] + [scavengers] * 4 + [flak] * 2,
        BattleEnemyOffsets=[point(0, y) for y in [-1400, 0, 1400]]
        + [point(800, y) for y in [-1500, 0, 1500]] + [point(1600)]
        + [point(-850, y) for y in [-1650, -550, 550, 1650]]
        + [point(2600, y) for y in [-1250, 1250]],
        ParatrooperFormation={
            "OwningPlayer": 1,
            "LandingTrailLength": 7200.0,
            "JumpIntervalSeconds": 0.8,
            "Passes": [
                {"DropOffset": point(7200, -900), "AircraftStartOffset": point(500, 0, 3500)},
                {"DropOffset": point(7200, 900), "AircraftStartOffset": point(500, 0, 4300)},
            ],
        },
        Caption1="SOVIETS | 3× T-34E | 3× T-70 | 1× IS-1 | 1× IS-2 | 3× PTRS squads | 1× BS-3 (100 mm) pit",
        Caption2="GERMANS | 3× StuG III | 3× Panzer III J | 1× King Tiger | 4× Scavenger squads | 2× Vulture drops | 2× FlaK 88 pits | 2× Ju 390 bombers",
    )
    job["UnitLabels"] = dict(zip(
        [t34, t70, is1, is2, ptrs, bs3, stug, panzer, tiger, scavengers, flak],
        ["T-34E", "T-70", "IS-1", "IS-2", "PTRS squad", "BS-3 (100 mm) pit", "StuG III", "Panzer III J", "King Tiger", "Scavenger squad", "FlaK 88 pit"],
    ))
    for class_path in job["UnitLabels"]:
        asset_path = PROJECT / "Content" / class_path.split(".")[0].removeprefix("/Game/")
        assert asset_path.with_suffix(".uasset").is_file(), class_path
    output = JOBS / (job["JobName"] + ".json")
    output.write_text(json.dumps(job, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(output)


if __name__ == "__main__":
    main()
