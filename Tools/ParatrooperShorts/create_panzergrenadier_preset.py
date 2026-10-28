"""Create a separate artillery battle with two Panzergrenadier aircraft passes."""
import copy
import json

from create_german_preset import JOBS, PROJECT, point, unit


def main():
    job = copy.deepcopy(json.loads((JOBS / "GermanTwinBomberParatrooperBattle.json").read_text(encoding="utf-8-sig")))
    job["JobName"] = "GermanPanzergrenadierArtilleryBattle"
    segment = job["Segments"][0]
    t34 = unit("GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476")
    is1 = unit("GroundVehicles/TrackBasedVehicle/Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-1_Soviet")
    ptrs = unit("Squads/Rus/Rus_Squad_MosinPTRS")
    hazmat = unit("Squads/Rus/Rus_Squad_HazmatBazooka")
    mosin = unit("Squads/Rus/Rus_Squad_Mosin")
    b4 = unit("Squads/TeamWeapons/Soviet/B4_203MM/Pit/Rus_BXP_B4HowitzerPit")
    tiger = unit("GroundVehicles/TrackBasedVehicle/Ger_Heavy/Tiger/Ger_Vehicle_TigerH1")
    panther = unit("GroundVehicles/TrackBasedVehicle/Ger_Heavy/PantherD/Ger_Vehicle_PantherD")
    scavengers = unit("Squads/Ger/Barracks_Infantry/Scavengers/Squad_Ger_Scavengers")
    howitzer = unit("Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_SFH18")
    grenadiers = unit("Squads/Ger/T2/PanzerGrenadiers/Squad_Ger_PanzerGrenadiers")
    segment.update(
        Subtype=job["JobName"],
        ClassPath=t34,
        BattleAllyClassPaths=[t34] * 4 + [is1] * 2 + [ptrs] * 2 + [hazmat] * 2 + [mosin] * 2 + [b4],
        BattleAllyOffsets=[point(0, y) for y in [-2400, -1200, 1200, 2400]]
        + [point(-1300, y) for y in [-1000, 1000]]
        + [point(800, y) for y in [-2100, -1260, -420, 420, 1260, 2100]]
        + [point(-3200)],
        BattleEnemyClassPaths=[panther] * 3 + [tiger] * 2 + [scavengers] * 4 + [howitzer] * 2,
        BattleEnemyOffsets=[point(0, y) for y in [-1800, 0, 1800]]
        + [point(1200, y) for y in [-1000, 1000]]
        + [point(-850, y) for y in [-1650, -550, 550, 1650]]
        + [point(3000, y) for y in [-1400, 1400]],
        Caption1="SOVIETS | 5× T-34-76 | 2× IS-1 (HEAT) | 2× Mosin PTRS squads | 2× Hazmat bazooka squads | 2× Mosin squads | 1× B-4 (203 mm) pit",
        Caption2="GERMANS | 2× Tiger H1 | 3× Panther D | 4× Scavenger squads | 2× Panzergrenadier drops | 2× sFH18 (150 mm) pits | 2× Ju 390 bombers",
    )
    segment["ParatrooperFormation"]["SquadSubtypes"] = ["Squad_Ger_PanzerGrenadiere"]
    # Three soldiers need wider jump spacing to cover the same rear-to-front flight as the four-man Vultures.
    segment["ParatrooperFormation"]["JumpIntervalSeconds"] = 1.2
    for flight in segment["ParatrooperFormation"]["Passes"]:
        flight["DropOffset"]["X"] = 8200.0
    job["UnitLabels"] = dict(zip(
        [t34, is1, ptrs, hazmat, mosin, b4, tiger, panther, scavengers, howitzer, grenadiers],
        ["T-34-76", "IS-1 (HEAT)", "Mosin PTRS squad", "Hazmat bazooka squad", "Mosin squad", "B-4 (203 mm) pit",
         "Tiger H1", "Panther D", "Scavenger squad", "sFH18 (150 mm) pit", "Panzergrenadier squad"],
    ))
    for class_path in job["UnitLabels"]:
        asset_path = PROJECT / "Content" / class_path.split(".")[0].removeprefix("/Game/")
        assert asset_path.with_suffix(".uasset").is_file(), class_path
    output = JOBS / (job["JobName"] + ".json")
    output.write_text(json.dumps(job, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(output)


if __name__ == "__main__":
    main()
